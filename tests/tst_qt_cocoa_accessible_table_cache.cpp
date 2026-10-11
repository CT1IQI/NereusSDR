// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Test uses callsign-shaped fixture strings only.
//
// The Qt 6.11.0 Cocoa accessibility cache repair
// (gui/QtCocoaAccessibleTableCache.h) on every wiring shape the app's live
// item views use.
//
// On that Cocoa image, +[QMacAccessibilityElement removeElementsFromCache:]
// (qcocoaaccessibilityelement.mm:219-226,257-267,342-362) deletes the real
// cell interfaces of expired native rows through
// QAccessibleCache::deleteInterface, while QAccessibleTable keeps their IDs
// and calls role() on the null interface during the next RowsInserted or
// RowsRemoved (itemviews.cpp:645-741). Offscreen there is no Cocoa bridge,
// so each case performs that deletion itself through the public
// QAccessible::deleteAccessibleInterface, then changes rows the way the
// app does:
//   - QTableWidget insertRow / removeRow / setRowCount (ConnectionPanel,
//     LanScanDialog, DSP assets, notch, filter preset, transverter and
//     needle calibration tables)
//   - QTreeWidget item insert and removal
//   - QListWidget append and trim (HL2 I2C log)
//   - QComboBox clear and refill through its popup list (profile, model,
//     antenna, sample rate and message combos)
//   - QTableView over two QSortFilterProxyModels on a custom model
//     (FreeDVReporterDialog, real dialog)
// The repair is gated to that Cocoa image, so the cases force it on through
// the test seam; one case checks that without it nothing is cleared.

#include <QtTest>
#include <QAccessible>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qplatformintegration.h>
#include <QComboBox>
#include <QListWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTreeWidget>

#include "core/AppSettings.h"
#include "core/FreeDVStation.h"
#include "gui/FreeDVReporterDialog.h"
#include "gui/QtCocoaAccessibleTableCache.h"
#include "models/FreeDVStationModel.h"

#include <memory>

using namespace NereusSDR;

namespace {

// Builds the view's accessible child cache the way an assistive client does
// when it walks the rows. Returns the child IDs.
QList<QAccessible::Id> walkChildren(QAbstractItemView* view)
{
    QList<QAccessible::Id> ids;
    QAccessibleInterface* table = QAccessible::queryAccessibleInterface(view);
    if (table == nullptr || table->tableInterface() == nullptr) {
        return ids;
    }
    const int children = table->childCount();
    for (int i = 0; i < children; ++i) {
        if (QAccessibleInterface* child = table->child(i)) {
            ids.append(QAccessible::uniqueId(child));
        }
    }
    return ids;
}

// Walks the children, then deletes every child interface the way the
// Qt 6.11.0 Cocoa bridge does when those native rows expire. Returns how
// many interfaces were deleted.
int walkThenExpireChildren(QAbstractItemView* view)
{
    const QList<QAccessible::Id> ids = walkChildren(view);
    for (QAccessible::Id id : ids) {
        QAccessible::deleteAccessibleInterface(id);
    }
    return int(ids.size());
}

void fillTable(QTableWidget& table, int rows)
{
    table.setRowCount(rows);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < table.columnCount(); ++column) {
            table.setItem(row, column,
                          new QTableWidgetItem(QStringLiteral("r%1c%2").arg(row).arg(column)));
        }
    }
}

FreeDVStation makeStation(const QString& sid, const QString& callsign, int snr)
{
    FreeDVStation s;
    s.sid = sid;
    s.callsign = callsign;
    s.gridSquare = QStringLiteral("EM00");
    s.version = QStringLiteral("1.9.0");
    s.frequencyHz = 14236000;
    s.txMode = QStringLiteral("700D");
    s.status = QStringLiteral("Active");
    s.snrVal = snr;
    s.lastUpdate = QDateTime::currentDateTime();
    return s;
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

class TestQtCocoaAccessibleTableCache : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        setAccessibilityActive(true);
        QVERIFY2(QAccessible::isActive(),
                 "accessibility must be active so item views send table model changes");
    }

    void init() { setQtCocoaAccessibleTableCacheRepairForcedForTest(true); }

    void cleanup() { setQtCocoaAccessibleTableCacheRepairForcedForTest(false); }

    void cleanupTestCase() { setAccessibilityActive(false); }

    void tableWidgetInsertRemoveAndResize()
    {
        QTableWidget table(0, 3);
        installQtCocoaAccessibleTableCacheRepair(&table);
        fillTable(table, 2);

        QVERIFY(walkThenExpireChildren(&table) > 0);
        table.insertRow(table.rowCount());
        QCOMPARE(table.rowCount(), 3);

        QVERIFY(walkThenExpireChildren(&table) > 0);
        table.removeRow(0);
        QCOMPARE(table.rowCount(), 2);

        QVERIFY(walkThenExpireChildren(&table) > 0);
        table.setRowCount(5);
        QVERIFY(walkThenExpireChildren(&table) > 0);
        table.setRowCount(1);
        QCOMPARE(table.rowCount(), 1);
        QCOMPARE(table.item(0, 0)->text(), QStringLiteral("r1c0"));
    }

    void treeWidgetInsertAndRemoveItems()
    {
        QTreeWidget tree;
        tree.setColumnCount(2);
        installQtCocoaAccessibleTableCacheRepair(&tree);
        auto* first = new QTreeWidgetItem(&tree, {QStringLiteral("first"), QStringLiteral("1")});
        new QTreeWidgetItem(first, {QStringLiteral("child"), QStringLiteral("1.1")});
        new QTreeWidgetItem(&tree, {QStringLiteral("second"), QStringLiteral("2")});
        tree.expandAll();

        QVERIFY(walkThenExpireChildren(&tree) > 0);
        new QTreeWidgetItem(&tree, {QStringLiteral("third"), QStringLiteral("3")});
        QCOMPARE(tree.topLevelItemCount(), 3);

        QVERIFY(walkThenExpireChildren(&tree) > 0);
        new QTreeWidgetItem(first, {QStringLiteral("child 2"), QStringLiteral("1.2")});
        QCOMPARE(first->childCount(), 2);

        QVERIFY(walkThenExpireChildren(&tree) > 0);
        delete tree.takeTopLevelItem(1);
        QCOMPARE(tree.topLevelItemCount(), 2);
        QCOMPARE(tree.topLevelItem(1)->text(0), QStringLiteral("third"));
    }

    void listWidgetAppendAndTrim()
    {
        QListWidget list;
        installQtCocoaAccessibleTableCacheRepair(&list);
        list.addItem(QStringLiteral("one"));
        list.addItem(QStringLiteral("two"));

        QVERIFY(walkThenExpireChildren(&list) > 0);
        list.addItem(QStringLiteral("three"));
        QVERIFY(walkThenExpireChildren(&list) > 0);
        delete list.takeItem(0);

        QCOMPARE(list.count(), 2);
        QCOMPARE(list.item(0)->text(), QStringLiteral("two"));
    }

    void comboPopupClearAndRefill()
    {
        QComboBox combo;
        installQtCocoaAccessibleTableCacheRepair(combo.view());
        combo.addItems({QStringLiteral("Default"), QStringLiteral("DX")});
        combo.setCurrentIndex(1);

        QVERIFY(walkThenExpireChildren(combo.view()) > 0);
        combo.clear();
        QCOMPARE(combo.count(), 0);
        combo.addItems({QStringLiteral("Default"), QStringLiteral("DX"), QStringLiteral("Contest")});
        QVERIFY(walkThenExpireChildren(combo.view()) > 0);
        combo.addItem(QStringLiteral("Ragchew"));

        QCOMPARE(combo.count(), 4);
    }

    // The real FreeDV Reporter dialog: stations join and leave from the
    // reporter feed, and a column filter hides and shows rows through its
    // proxies.
    void freedvReporterStationsJoinLeaveAndFilter()
    {
        FreeDVStationModel stations;
        auto dialog = std::make_unique<FreeDVReporterDialog>(&stations, nullptr, nullptr);
        auto* view = dialog->findChild<QTableView*>();
        QVERIFY(view);
        stations.onStationAdded(QStringLiteral("sid-1"), makeStation(QStringLiteral("sid-1"), QStringLiteral("K1ABC"), 12));
        stations.onStationAdded(QStringLiteral("sid-2"), makeStation(QStringLiteral("sid-2"), QStringLiteral("K2ABC"), 0));

        QVERIFY(walkThenExpireChildren(view) > 0);
        stations.onStationAdded(QStringLiteral("sid-3"), makeStation(QStringLiteral("sid-3"), QStringLiteral("K3ABC"), 7));
        QCOMPARE(view->model()->rowCount(), 3);

        QVERIFY(walkThenExpireChildren(view) > 0);
        stations.onStationRemoved(QStringLiteral("sid-1"));
        QCOMPARE(view->model()->rowCount(), 2);

        QVERIFY(walkThenExpireChildren(view) > 0);
        dialog->setColumnFilterForTest(12, 0 /* FILTER_GTE */, QStringLiteral("5"));
        QCOMPARE(view->model()->rowCount(), 1);
        QVERIFY(walkThenExpireChildren(view) > 0);
        dialog->clearColumnFilterForTest(12);
        QCOMPARE(view->model()->rowCount(), 2);

        AppSettings::instance().remove(QStringLiteral("FreeDvReporter/ColumnFilters"));
    }

    // The repair clears only the accessibility cache: with the gate open an
    // insert drops the cached cell interfaces, and with it closed (any
    // platform but the Qt 6.11.0 Cocoa image) the cache is left as Qt keeps
    // it.
    void repairClearsCacheOnlyWhenGateIsOpen()
    {
        QTableWidget table(0, 2);
        installQtCocoaAccessibleTableCacheRepair(&table);
        fillTable(table, 2);

        setQtCocoaAccessibleTableCacheRepairForcedForTest(false);
        const QList<QAccessible::Id> kept = walkChildren(&table);
        QVERIFY(!kept.isEmpty());
        table.insertRow(table.rowCount());
        for (QAccessible::Id id : kept) {
            QVERIFY(QAccessible::accessibleInterface(id) != nullptr);
        }

        setQtCocoaAccessibleTableCacheRepairForcedForTest(true);
        const QList<QAccessible::Id> cleared = walkChildren(&table);
        QVERIFY(!cleared.isEmpty());
        table.insertRow(table.rowCount());
        for (QAccessible::Id id : cleared) {
            QVERIFY(QAccessible::accessibleInterface(id) == nullptr);
        }
        QCOMPARE(table.rowCount(), 4);
        QCOMPARE(table.item(1, 1)->text(), QStringLiteral("r1c1"));
    }
};

QTEST_MAIN(TestQtCocoaAccessibleTableCache)
#include "tst_qt_cocoa_accessible_table_cache.moc"
