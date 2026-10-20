// no-port-check: NereusSDR-original test file.
//
// =================================================================
// tests/tst_tx_section_content.cpp  (NereusSDR)
// =================================================================
//
// The TX applet's rows as independent container content.
//
// A container can hold the TX Monitor controls (or any other row) on their
// own.  The applet stays the wiring authority: it makes each copy and wires
// it, so the copy shows and sets the same model state as the applet's own
// row and follows the same transmit-permission gates.  The applet's own
// panel is unchanged — it still shows every row together.
//
// Tests:
//   1. The registry offers all seven row types, and the operator's
//      "add item" menu would list them.
//   2. A copy is its own instance, not the applet's own row, and two copies
//      are two instances.
//   3. The copy shows the model state.
//   4. The copy's control sets the model, and the applet's own row follows.
//   5. The applet's own panel still has every row.
//   6. The transmit-permission gate reaches the copy.
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QMargins>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>

#include "core/AppSettings.h"
#include "core/RadioStatus.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxChainSection.h"
#include "gui/applets/TxKeyingSection.h"
#include "gui/applets/TxMonitorSection.h"
#include "gui/applets/TxSectionContent.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const char* const kRowTypes[] = {
    "tx.monitor", "tx.power", "tx.filter", "tx.chain",
    "tx.vox", "tx.processing", "tx.keying",
};

// The applet's own row's widgets, by the object names the stylesheet and the
// rest of the app find them by.
const char* const kAppletRowWidgets[] = {
    "TxLevButton", "TxEqButton", "TxCfcButton", "TxRfPowerSlider",
    "TxVoxButton", "TxAppletPsaBtn", "TxHolderLabel", "TxTakeTransmitButton",
};

} // namespace

class TestTxSectionContent : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. The registry offers every row type ────────────────────────────
    void registryOffersEveryRow()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        for (const char* type : kRowTypes) {
            const QString id = QString::fromLatin1(type);
            QVERIFY2(registry.hasWidgetFactory(id), qPrintable(id));
            QVERIFY2(registry.isAvailable(id), qPrintable(id));

            bool listed = false;
            for (const auto& descriptor : registry.descriptors()) {
                if (descriptor.typeId == id) {
                    listed = true;
                    QVERIFY2(!descriptor.title.isEmpty(), qPrintable(id));
                    break;
                }
            }
            QVERIFY2(listed, qPrintable(id));
        }
    }

    // ── 2. A copy is its own instance ───────────────────────────────────
    void aCopyIsItsOwnInstance()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        QVERIFY(copy != &applet);

        auto* copyLev = copy->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* ownLev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(copyLev);
        QVERIFY(ownLev);
        QVERIFY(copyLev != ownLev);

        // A second copy is a second instance, not the first one again.
        auto* second = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(second);
        QVERIFY(second != copy);
        auto* secondLev = second->findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(secondLev);
        QVERIFY(secondLev != copyLev);
        QVERIFY(secondLev != ownLev);
    }

    // ── 3. The copy shows the model state ────────────────────────────────
    void theCopyShowsTheModelState()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);
        QVERIFY(chain->levButton());

        // The model starts with the leveler on.
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(chain->levButton()->isChecked());

        rm.transmitModel().setTxLevelerOn(false);
        QVERIFY(!chain->levButton()->isChecked());

        rm.transmitModel().setTxLevelerOn(true);
        QVERIFY(chain->levButton()->isChecked());
    }

    // ── 4. The copy sets the model, and the applet's own row follows ─────
    void theCopySetsTheModelAndTheAppletFollows()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);
        QVERIFY(chain->levButton());
        auto* ownLev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(ownLev);

        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(ownLev->isChecked());

        // The copy's press is the model's answer.
        chain->levButton()->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), false);
        QVERIFY(!ownLev->isChecked());

        // And back the other way: the applet's own press moves the copy.
        ownLev->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
        QVERIFY(chain->levButton()->isChecked());
    }

    // ── 5. The applet's own panel still has every row ────────────────────
    void theAppletKeepsEveryRow()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        for (const char* name : kAppletRowWidgets) {
            const QString objectName = QString::fromLatin1(name);
            QVERIFY2(applet.findChild<QWidget*>(objectName), qPrintable(objectName));
        }
    }

    // ── 6. The registry is the authority on what the workspace can build ─
    void theRegistryIsTheAuthorityOnTheRows()
    {
        // A workspace with no TX rows registered: the rows are not offered.
        ContainerContentRegistry bare;
        for (const char* type : kRowTypes) {
            QVERIFY2(!bare.hasWidgetFactory(QString::fromLatin1(type)), type);
        }

        // A workspace the app registered them in: every row is offered.
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);
        for (const char* type : kRowTypes) {
            QVERIFY2(registry.hasWidgetFactory(QString::fromLatin1(type)), type);
        }
    }

    // ── 7. The add-item menu offers the rows, and only the rows the
    //       registry has ──────────────────────────────────────────────────
    void theAddItemMenuOffersTheRowsTheRegistryHas()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        const QStringList titles = txMenuTitles(registry);
        QCOMPARE(titles.size(), 7);
        QVERIFY(titles.contains(QStringLiteral("TX Monitor")));
        QVERIFY(titles.contains(QStringLiteral("TX Keying")));

        // A workspace with no TX rows has no TX submenu at all, rather than
        // one whose entries cannot be built.
        ContainerContentRegistry bare;
        QVERIFY(txMenuTitles(bare).isEmpty());
    }

    // ── 9. A copy gives itself the margins the applet's column gives ─────
    // The applet's own rows give none, because their column already has
    // the margins.  A copy in a container has no column of its own, so it
    // gives itself the margins the column would have given it.
    void aCopyGivesItselfTheColumnMargins()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.chain"), &host);
        QVERIFY(copy);
        auto* chain = qobject_cast<TxChainSection*>(copy);
        QVERIFY(chain);

        const QMargins copyMargins = chain->layout()->contentsMargins();
        QCOMPARE(copyMargins.left(), 4);
        QCOMPARE(copyMargins.right(), 4);
        QCOMPARE(copyMargins.top(), 2);
        QCOMPARE(copyMargins.bottom(), 2);

        // The applet's own row gives none: its column already has them.
        auto* own = applet.findChild<TxChainSection*>();
        QVERIFY(own);
        const QMargins ownMargins = own->layout()->contentsMargins();
        QCOMPARE(ownMargins.left(), 0);
        QCOMPARE(ownMargins.right(), 0);
        QCOMPARE(ownMargins.top(), 0);
        QCOMPARE(ownMargins.bottom(), 0);
    }

    // ── 8. A copy the container destroys leaves the applet's list ─────────
    // The copy belongs to the container, which owns it: closing the
    // container destroys the copy.  The applet's timers and gates walk its
    // list of copies, so a copy that is gone must not stay in it.
    void aCopyTheContainerDestroysLeavesTheAppletsList()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        QList<QWidget*> copies;
        for (const char* type : kRowTypes) {
            auto* copy = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host);
            QVERIFY(copy);
            copies << copy;
        }

        // While the copies live, the applet's pushers and gates reach them.
        applet.syncFromModel();
        applet.setTransmitPermitted(true);

        // The container closes: every copy goes with it.
        for (QWidget* copy : copies) {
            delete copy;
        }

        // The applet's pushers and gates must not touch a copy that is gone.
        applet.syncFromModel();
        applet.setTransmitPermitted(false);
        applet.setTransmitSettingsPermitted(false);
        applet.setTransmitChainSettingsPermitted(false);
        applet.setVoxPermitted(false);
        applet.setTxProfilePermitted(false);
        applet.setPureSignalArmingPermitted(false);
        applet.setTransmitPermitted(true);
        applet.setTransmitSettingsPermitted(true);
        applet.setTransmitChainSettingsPermitted(true);
        applet.setVoxPermitted(true);
        applet.setTxProfilePermitted(true);
        applet.setPureSignalArmingPermitted(true);

        // The applet's own row still works.
        QVERIFY(applet.findChild<QPushButton*>(QStringLiteral("TxLevButton")));
    }

    // ── 9. The model's signals do not reach a copy that is gone ──────────
    // A copy's model wiring belongs to the copy.  After the container
    // destroys it, a model change must not write to its widgets; the copy's
    // own refresh timer must stop with it too.  Run with MallocScribble=1 on
    // macOS (or under ASan) so a write to freed memory fails every time.
    void theModelDoesNotReachACopyThatIsGone()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        const int timersBefore = applet.findChildren<QTimer*>().size();
        QWidget host;
        QList<QWidget*> copies;
        for (const char* type : kRowTypes) {
            auto* copy = registry.createWidget(
                registry.makeEntry(QString::fromLatin1(type)), &host);
            QVERIFY(copy);
            copies << copy;
        }
        for (QWidget* copy : copies) {
            delete copy;
        }
        // No timer a copy started outlives it.
        QCOMPARE(applet.findChildren<QTimer*>().size(), timersBefore);

        // Every model signal the power, filter, chain and VOX rows follow.
        TransmitModel& tx = rm.transmitModel();
        tx.setPower(tx.power() == 37 ? 38 : 37);
        tx.setTunePowerForBand(Band::Band20m, 23);
        tx.setFilterLow(tx.filterLow() + 10);
        tx.setFilterHigh(tx.filterHigh() - 10);
        tx.setTxLevelerOn(!tx.txLevelerOn());
        tx.setTxEqEnabled(!tx.txEqEnabled());
        tx.setCfcEnabled(!tx.cfcEnabled());
        tx.setVoxEnabled(!tx.voxEnabled());
        tx.setVoxThresholdDb(tx.voxThresholdDb() == -40 ? -41 : -40);
        tx.setVoxHangTimeMs(tx.voxHangTimeMs() == 300 ? 310 : 300);
        rm.radioStatus().setForwardPower(12.5);
        QTest::qWait(120);   // the 20 Hz power refresh and the 100 ms VOX poll

        // The applet's own row still follows the model.
        auto* own = applet.findChild<QSlider*>(QStringLiteral("TxRfPowerSlider"));
        QVERIFY(own);
        QCOMPARE(own->value(), tx.power());
    }

    // ── 6. The transmit-permission gate reaches the copy ─────────────────
    void theTransmitGateReachesTheCopy()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        ContainerContentRegistry registry;
        registerTxSectionContent(registry, &applet);

        QWidget host;
        auto* copy = registry.createWidget(registry.makeEntry("tx.keying"), &host);
        QVERIFY(copy);
        auto* keying = qobject_cast<TxKeyingSection*>(copy);
        QVERIFY(keying);
        QVERIFY(keying->moxButton());
        QVERIFY(keying->tuneButton());
        QVERIFY(keying->moxButton()->isEnabled());
        QVERIFY(keying->tuneButton()->isEnabled());

        const QString reason = QStringLiteral("The Core will not transmit");
        applet.setTransmitPermitted(false, reason);

        QVERIFY(!keying->moxButton()->isEnabled());
        QVERIFY(!keying->tuneButton()->isEnabled());
        QCOMPARE(keying->moxButton()->toolTip(), reason);

        // The applet's own row is gated the same way.
        QVERIFY(!applet.moxButton()->isEnabled());
        QVERIFY(!applet.tuneButton()->isEnabled());

        applet.setTransmitPermitted(true);
        QVERIFY(keying->moxButton()->isEnabled());
        QVERIFY(keying->tuneButton()->isEnabled());
    }
private:
    // The titles the add-item menu's TX submenu offers, for a workspace over
    // this registry.  Empty when the workspace offers no TX rows.
    QStringList txMenuTitles(ContainerContentRegistry& registry)
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath("settings"));
        ContainerWorkspaceStore store(settings);
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);
        WorkspaceDocument document;
        document.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = QStringLiteral("A");
        container.layout = ContentLayout::VerticalStack;
        document.containers = {container};
        if (manager.commitWorkspace(document, 0).status != CommitStatus::Saved) {
            return {};
        }

        ContainerSettingsDialog dialog(manager.container("A"), nullptr, &manager);
        dialog.show();
        QCoreApplication::processEvents();

        // The button that opens the add-item menu.  It is the "+" button
        // with this tooltip; the "Add ->" button beside it adds a selected
        // available item and opens no menu.
        QPushButton* add = nullptr;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->toolTip() == QStringLiteral("Add item (popup)")) {
                add = button;
                break;
            }
        }
        if (!add) {
            return {};
        }
        add->click();
        QCoreApplication::processEvents();

        // The popup's TX submenu, when the workspace has the rows.
        QMenu* txMenu = nullptr;
        for (QMenu* candidate : dialog.findChildren<QMenu*>()) {
            if (candidate->title() == QStringLiteral("TX")) {
                txMenu = candidate;
                break;
            }
        }
        if (!txMenu) {
            return {};
        }
        QStringList titles;
        for (QAction* action : txMenu->actions()) {
            titles << action->text();
        }
        return titles;
    }
};

QTEST_MAIN(TestTxSectionContent)
#include "tst_tx_section_content.moc"
