// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Test uses callsign-shaped fixture strings only.
//
// Spot List accessibility cache under live spot traffic.
//
// On the Qt 6.11.0 Cocoa image, +[QMacAccessibilityElement
// removeElementsFromCache:] (qcocoaaccessibilityelement.mm:219-226,
// 257-267, 342-362) calls QAccessibleCache::deleteInterface for the real
// cell interfaces of expired native rows. QAccessibleTable keeps those IDs
// in its own child cache and, on the next RowsInserted or RowsRemoved,
// calls role() on the null interface QAccessible::accessibleInterface()
// returns for them (itemviews.cpp:645-741). A spot arriving from the Core
// then crashes inside SpotTableModel::addSpot.
//
// Offscreen there is no Cocoa bridge, so each case performs the bridge's
// deletion itself through the public QAccessible::deleteAccessibleInterface
// (the same QAccessibleCache::deleteInterface call) and then drives the
// Spot List the way the app does: SpotTableModel -> BandFilterProxy ->
// spotListTable, built by SpotHubDialog. The dialog's repair is gated to
// that Cocoa image, so the cases force it on through the test seam.

#include <QtTest>
#include <QAccessible>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>
#include <QPushButton>
#include <QTableView>

#include "core/DxClusterClient.h"
#include "core/DxSpot.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/WsjtxClient.h"
#include "gui/QtCocoaAccessibleTableCache.h"
#include "gui/SpotHubDialog.h"
#include "models/SpotModel.h"
#include "models/SpotTableModel.h"

#include <memory>

using namespace NereusSDR;

namespace {

DxSpot makeSpot(const QString& call, double mhz, const QString& source = QStringLiteral("Cluster"))
{
    DxSpot s;
    s.dxCall = call;
    s.freqMhz = mhz;
    s.spotterCall = QStringLiteral("TEST");
    s.comment = QStringLiteral("fixture");
    s.utcTime = QTime(12, 0);
    s.source = source;
    return s;
}

// The clients, models and dialog the app wires together, owned here.
struct SpotListFixture {
    DxClusterClient cluster;
    DxClusterClient rbn;
    WsjtxClient wsjtx;
    SpotCollectorClient collector;
    PotaClient pota;
    FreeDVReporterClient freedv;
    PskReporterClient psk;
    SpotModel spots;
    SpotTableModel table;
    DxccColorProvider dxcc;
    std::unique_ptr<SpotHubDialog> dialog;
    QTableView* view{nullptr};

    SpotListFixture()
    {
        dialog = std::make_unique<SpotHubDialog>(&cluster, &rbn, &wsjtx, &collector, &pota,
                                                 &freedv, &psk, &spots, &table, &dxcc);
        view = dialog->findChild<QTableView*>(QStringLiteral("spotListTable"));
    }
};

// Builds the table's accessible child cache the way an assistive client
// does when it walks the rows, then deletes every child interface the way
// the Qt 6.11.0 Cocoa bridge does when those native rows expire. Returns
// how many interfaces were deleted.
int walkThenExpireChildren(QTableView* view)
{
    QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
    if (table == nullptr || table->tableInterface() == nullptr) {
        return -1;
    }
    QList<QAccessible::Id> ids;
    const int children = table->childCount();
    for (int i = 0; i < children; ++i) {
        if (QAccessibleInterface* child = table->child(i)) {
            ids.append(QAccessible::uniqueId(child));
        }
    }
    for (QAccessible::Id id : std::as_const(ids)) {
        QAccessible::deleteAccessibleInterface(id);
    }
    return int(ids.size());
}

// Turns the platform's accessibility on or off. Qt 6.11's
// QAccessible::setActive does that; Qt 6.8's (Linux CI) only tells the
// activation observers and leaves QAccessible::isActive() false, so there
// the platform's own switch is set, as Qt's tests do.
void setAccessibilityActive(bool active)
{
    QAccessible::setActive(active);
    if (QAccessible::isActive() == active) {
        return;
    }
    if (QPlatformAccessibility* platform = QGuiApplicationPrivate::platformIntegration()->accessibility()) {
        platform->setActive(active);
    }
}

} // namespace

class TestSpotListAccessibility : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        setAccessibilityActive(true);
        QVERIFY2(QAccessible::isActive(),
                 "accessibility must be active so item views send table model changes");
        setQtCocoaAccessibleTableCacheRepairForcedForTest(true);
    }

    void cleanupTestCase()
    {
        setQtCocoaAccessibleTableCacheRepairForcedForTest(false);
        setAccessibilityActive(false);
    }

    // The crash in the field: a spot arrives after the bridge expired rows.
    void spotArrivesAfterExpiredRows()
    {
        SpotListFixture f;
        QVERIFY(f.view);
        f.dialog->resize(900, 600);
        f.dialog->show();
        f.table.addSpot(makeSpot(QStringLiteral("K1ABC"), 14.025));
        f.table.addSpot(makeSpot(QStringLiteral("K2ABC"), 7.030));
        QVERIFY(walkThenExpireChildren(f.view) > 0);
        QAbstractItemModel* proxy = f.view->model();
        const auto proxyRowOf = [proxy](const QString& call) {
            for (int row = 0; row < proxy->rowCount(); ++row) {
                if (proxy->index(row, SpotTableModel::ColDxCall).data().toString() == call) {
                    return row;
                }
            }
            return -1;
        };
        const QPersistentModelIndex kept =
            proxy->index(proxyRowOf(QStringLiteral("K2ABC")), SpotTableModel::ColDxCall);
        QVERIFY(kept.isValid());
        QSignalSpy resetSpy(proxy, &QAbstractItemModel::modelReset);

        f.table.addSpot(makeSpot(QStringLiteral("K3ABC"), 21.030));

        QCOMPARE(proxy->rowCount(), 3);
        // Only the accessibility cache is cleared: the proxy is not reset
        // and persistent indexes keep their spots.
        QCOMPARE(resetSpy.count(), 0);
        QVERIFY(kept.isValid());
        QCOMPARE(kept.data().toString(), QStringLiteral("K2ABC"));
        const int newRow = proxyRowOf(QStringLiteral("K3ABC"));
        QVERIFY(newRow >= 0);
        QAccessibleInterface* table = QAccessible::queryAccessibleInterface(f.view);
        QVERIFY(table);
        QAccessibleInterface* cell =
            table->tableInterface()->cellAt(newRow, SpotTableModel::ColDxCall);
        QVERIFY(cell);
        QCOMPARE(cell->text(QAccessible::Name), QStringLiteral("K3ABC"));
    }

    // The 500-spot cap trims the oldest row: RowsRemoved walks the cache too.
    void trimmedSpotAfterExpiredRows()
    {
        SpotListFixture f;
        QVERIFY(f.view);
        f.table.setMaxSpots(2);
        f.table.addSpot(makeSpot(QStringLiteral("K1ABC"), 14.025));
        f.table.addSpot(makeSpot(QStringLiteral("K2ABC"), 7.030));
        QVERIFY(walkThenExpireChildren(f.view) > 0);

        f.table.addSpot(makeSpot(QStringLiteral("K3ABC"), 21.030));

        QCOMPARE(f.view->model()->rowCount(), 2);
    }

    // A band pill hides and shows rows through the proxy.
    void bandFilterAfterExpiredRows()
    {
        SpotListFixture f;
        QVERIFY(f.view);
        f.table.addSpot(makeSpot(QStringLiteral("K1ABC"), 14.025));
        f.table.addSpot(makeSpot(QStringLiteral("K2ABC"), 7.030));
        auto* pill = f.dialog->findChild<QPushButton*>(QStringLiteral("spotListBandPill_20m"));
        QVERIFY(pill);
        const bool wasChecked = pill->isChecked();
        if (!wasChecked) {
            pill->setChecked(true);
        }
        QVERIFY(walkThenExpireChildren(f.view) > 0);

        pill->setChecked(false);
        QCOMPARE(f.view->model()->rowCount(), 1);
        QVERIFY(walkThenExpireChildren(f.view) > 0);
        pill->setChecked(true);
        QCOMPARE(f.view->model()->rowCount(), 2);

        pill->setChecked(wasChecked);
    }

    // Clear resets the model; a hidden dialog still keeps its interface.
    void clearAndHiddenAfterExpiredRows()
    {
        SpotListFixture f;
        QVERIFY(f.view);
        f.dialog->show();
        f.table.addSpot(makeSpot(QStringLiteral("K1ABC"), 14.025));
        f.dialog->hide();
        QVERIFY(walkThenExpireChildren(f.view) > 0);

        f.table.clear();
        QCOMPARE(f.view->model()->rowCount(), 0);
        f.table.addSpot(makeSpot(QStringLiteral("K2ABC"), 7.030));
        QVERIFY(walkThenExpireChildren(f.view) > 0);
        f.table.addSpot(makeSpot(QStringLiteral("K3ABC"), 21.030));
        QCOMPARE(f.view->model()->rowCount(), 2);
    }
};

QTEST_MAIN(TestSpotListAccessibility)
#include "tst_spot_list_accessibility.moc"
