// no-port-check: NereusSDR-original regressions for Pop Out, the pin and Return in the workspace look.
// Modification history (NereusSDR):
//   2026-10-11 - The body menu starts with Pop Out (Return on a window); a
//                 container popped out by the header button, the menu or a
//                 drag records where it came from and Return puts it back
//                 there, with fallbacks when the neighbours, the strip or
//                 the free spot are gone; a window's header carries the
//                 pin, saved with the container; and a window an operator
//                 command makes is saved with the place and monitor it was
//                 given. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - Return after the pane changed size puts a free container
//                 where it was showing; the fallback place is the nearest
//                 one clear of every other free container; and a
//                 placement chosen in Container Settings records and
//                 clears the home as Pop Out and Return do. The timer
//                 that closes a menu left open ends with its lookup. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <memory>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ScreenSource.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/AppIcon.h"

using namespace NereusSDR;

namespace {

// Longer than the manager's 200 ms geometry commit delay.
constexpr int kPastCommitDelayMs = 300;
const QRect kFreeRect(200, 150, 320, 240);
const QRect kScreen(0, 0, 2560, 1440);

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

void press(QWidget* target, const QPoint& global)
{
    QMouseEvent event(QEvent::MouseButtonPress, target->mapFromGlobal(global), global, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void move(QWidget* target, const QPoint& global)
{
    QMouseEvent event(QEvent::MouseMove, target->mapFromGlobal(global), global, Qt::NoButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void release(QWidget* target, const QPoint& global)
{
    QMouseEvent event(QEvent::MouseButtonRelease, target->mapFromGlobal(global), global, Qt::LeftButton,
                      Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

// One monitor, larger than the main window, whatever the test platform has.
class FakeScreens : public ScreenSource {
public:
    QVector<ScreenInfo> list{ScreenInfo{QStringLiteral("Bench"), QStringLiteral("SN-1"), kScreen, kScreen}};
    QVector<ScreenInfo> screens() const override { return list; }
    ScreenInfo primary() const override { return list.first(); }
};

// What an open menu holds: its action texts in order, and whether the one
// named `watch` is ticked. `choose`, when not empty, is triggered.
struct MenuReading {
    bool opened = false;
    QStringList texts;
    bool watchedTicked = false;
};

template<typename Open>
MenuReading readMenu(Open open, const QString& choose = QString(), const QString& watch = QString())
{
    MenuReading reading;
    QTimer::singleShot(0, [&reading, choose, watch] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (menu == nullptr) {
            return;
        }
        reading.opened = true;
        QAction* chosen = nullptr;
        for (QAction* action : menu->actions()) {
            if (action->isSeparator()) {
                continue;
            }
            reading.texts.append(action->text());
            if (action->text() == watch) {
                reading.watchedTicked = action->isChecked();
            }
            if (!choose.isEmpty() && action->text() == choose) {
                chosen = action;
            }
        }
        menu->close();
        if (chosen != nullptr) {
            chosen->trigger();
        }
    });
    // Never leave a menu open under a failed lookup. The timer goes with
    // this call, so it cannot close a later case's menu.
    QObject untilMenuClosed;
    QTimer::singleShot(3000, &untilMenuClosed, [] {
        if (QWidget* popup = QApplication::activePopupWidget()) {
            popup->close();
        }
    });
    open();
    return reading;
}

// QWidget::destroy() is protected.
class ReleasableMainWindow : public QMainWindow {
public:
    void releaseNativeWindow() { destroy(); }
};

// Offline fixture: a main window whose central widget is the four-edge area
// around a plain centre pane, as in MainWindow. No radio, discovery or
// poller.
struct ReturnFixture {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath("popout-return.xml")};
    ContainerWorkspaceStore store{settings};
    ContainerContentRegistry registry;
    FakeScreens screens;
    ReleasableMainWindow window;
    QSplitter* middle{nullptr};
    QWidget* center{nullptr};
    EdgeDockArea* area{nullptr};
    std::unique_ptr<ContainerManager> manager;

    ReturnFixture()
    {
        middle = new EdgeDockSplitter(Qt::Horizontal, &window);
        center = new QWidget(middle);
        center->setMinimumWidth(400);
        middle->addWidget(center);
        area = new EdgeDockArea(middle, center, &window);
        window.setCentralWidget(area);
        window.statusBar()->setSizeGripEnabled(false);
        window.move(200, 100);
        window.resize(1400, 900);
        manager = std::make_unique<ContainerManager>(center, middle);
        manager->setScreenSourceForTest(&screens);
        manager->setEdgeDockArea(area);
        manager->setWorkspaceAdapter(&store, &registry);
    }

    ContainerDocument container(const QString& id, DockMode mode, DockEdge edge = DockEdge::Right)
    {
        ContainerDocument result;
        result.id = id;
        result.name = id;
        result.layout = ContentLayout::VerticalStack;
        result.dockMode = mode;
        result.edge = edge;
        result.header = HeaderMode::Shown;
        if (mode == DockMode::OverlayDocked) {
            result.geometry = kFreeRect;
        } else if (mode == DockMode::Floating) {
            result.geometry = QRect(900, 300, 360, 300);
        }
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        entry.id = id + QStringLiteral("-meter");
        entry.context["sliceId"] = 0;
        result.contents.append(entry);
        return result;
    }

    // Commits `containers` (the first is the main container) and shows the
    // main window.
    bool show(const QVector<ContainerDocument>& containers)
    {
        WorkspaceDocument document = store.snapshot();
        document.containers = containers;
        document.mainContainerId = containers.first().id;
        if (manager->commitWorkspace(document, document.revision).status != CommitStatus::Saved) {
            return false;
        }
        window.show();
        if (!QTest::qWaitForWindowExposed(&window)) {
            return false;
        }
        settle();
        // Whatever the first layout has to save is saved before a case
        // looks at the document.
        manager->saveState();
        QTest::qWait(kPastCommitDelayMs);
        settle();
        return true;
    }
    // "first" (the main container), "second" and "third" in the right
    // strip, then `extra`.
    bool showStripOfThree(const QVector<ContainerDocument>& extra = {})
    {
        QVector<ContainerDocument> all{container(QStringLiteral("first"), DockMode::PanelDocked),
                                       container(QStringLiteral("second"), DockMode::PanelDocked),
                                       container(QStringLiteral("third"), DockMode::PanelDocked)};
        all += extra;
        return show(all);
    }

    // Qt's offscreen plugin faults in QOffscreenBackingStore::clearHash
    // when a window is destroyed whose backing store once drew a native
    // child that another window's store drew later, after that other store
    // has gone (a test platform defect, see tst_workspace_arrange_drag and
    // tst_workspace_edge_strips). Every case here moves containers between
    // the main window and windows of their own, so each fixture ends with
    // every container back in the main window and drawn there with all of
    // its parts showing (Arrange shows the ones a window also shows); the
    // windows then go first and the main window last.
    ~ReturnFixture()
    {
        if (!manager) {
            return;
        }
        for (ContainerWidget* c : manager->allContainers()) {
            if (c->isFloating()) {
                manager->recoverContainer(c->id());
            }
        }
        settle();
        manager->setArrangeMode(true);
        settle();
        window.repaint();
        settle();
    }

    ContainerWidget* widget(const QString& id) const { return manager->container(id); }
    WorkspaceGroupHeader* header(const QString& id) const { return widget(id)->workspaceHeader(); }
    QWidget* shell(const QString& id) const { return widget(id)->window(); }
    ContainerDocument document(const QString& id) const
    {
        for (const auto& d : store.snapshot().containers) {
            if (d.id == id) {
                return d;
            }
        }
        return {};
    }
    QStringList strip(DockEdge edge) const
    {
        QStringList ids;
        for (const auto& d : store.snapshot().containers) {
            if (d.dockMode == DockMode::PanelDocked && d.edge == edge) {
                ids.append(d.id);
            }
        }
        return ids;
    }
    // The strip as shown, top to bottom or left to right.
    QStringList shownStrip(DockEdge edge) const
    {
        QStringList ids;
        const QSplitter* splitter = area->strip(edge)->splitter();
        for (int i = 0; i < splitter->count(); ++i) {
            if (auto* c = qobject_cast<ContainerWidget*>(splitter->widget(i)); c && !c->isHidden()) {
                ids.append(c->id());
            }
        }
        return ids;
    }
    QPoint outsideWindow() const { return window.mapToGlobal(QPoint(window.width() + 200, 120)); }
    QPoint leftZone() const { return area->mapToGlobal(QPoint(10, area->height() / 2)); }

    void openBodyMenu(const QString& id) const
    {
        ContainerWidget* c = widget(id);
        const QPoint at(c->width() / 2, c->height() - 12);
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, c->mapToGlobal(at));
        QApplication::sendEvent(c, &event);
    }
    // Pops `id` out by "button" (the header's), "menu" (the body menu's Pop
    // Out) or "drag" (a header drag out of the main window while
    // arranging).
    bool popOut(const QString& id, const QString& how)
    {
        if (how == QStringLiteral("button")) {
            header(id)->popOutButton()->click();
        } else if (how == QStringLiteral("menu")) {
            if (!readMenu([this, &id] { openBodyMenu(id); }, QStringLiteral("Pop Out")).opened) {
                return false;
            }
        } else {
            manager->setArrangeMode(true);
            settle();
            QWidget* grip = header(id);
            const QPoint grab = grip->mapToGlobal(QPoint(grip->width() / 2, grip->height() / 2));
            press(grip, grab);
            move(grip, grab + QPoint(0, 30));
            move(grip, outsideWindow());
            release(grip, outsideWindow());
            settle();
            manager->setArrangeMode(false);
        }
        settle();
        return widget(id) != nullptr && widget(id)->isFloating();
    }
    void returnByHeader(const QString& id)
    {
        header(id)->popOutButton()->click();
        settle();
    }
};

ContainerHome stripHome(DockEdge edge, const QString& before, const QString& after)
{
    ContainerHome home;
    home.dockMode = DockMode::PanelDocked;
    home.edge = edge;
    home.beforeId = before;
    home.afterId = after;
    return home;
}

ContainerHome freeHome(const QRect& rect, const QSize& pane)
{
    ContainerHome home;
    home.dockMode = DockMode::OverlayDocked;
    home.geometry = rect;
    home.geometryPane = pane;
    return home;
}

void threeWays()
{
    QTest::addColumn<QString>("how");
    QTest::newRow("header button") << QStringLiteral("button");
    QTest::newRow("body menu") << QStringLiteral("menu");
    QTest::newRow("drag out of the window") << QStringLiteral("drag");
}

} // namespace

class TstWorkspacePopoutReturn : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void theBodyMenuStartsWithPopOutAndReadsReturnOnAWindow()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("free"), DockMode::OverlayDocked)}));
        for (const QString& id : {QStringLiteral("second"), QStringLiteral("free")}) {
            const MenuReading docked = readMenu([&f, &id] { f.openBodyMenu(id); });
            QVERIFY(docked.opened);
            QCOMPARE(docked.texts.value(0), QStringLiteral("Pop Out"));
            QVERIFY(!docked.texts.contains(QStringLiteral("Return")));
        }
        // Choosing it makes the container a window, and the item reads
        // Return there.
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("menu")));
        QCOMPARE(f.document("second").dockMode, DockMode::Floating);
        const MenuReading floating = readMenu([&f] { f.openBodyMenu(QStringLiteral("second")); },
                                              QStringLiteral("Return"));
        QVERIFY(floating.opened);
        QCOMPARE(floating.texts.value(0), QStringLiteral("Return"));
        QVERIFY(!floating.texts.contains(QStringLiteral("Pop Out")));
        settle();
        QCOMPARE(f.document("second").dockMode, DockMode::PanelDocked);
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"first", "second", "third"}));
    }

    void aStripContainerReturnsBetweenItsNeighbours_data() { threeWays(); }
    void aStripContainerReturnsBetweenItsNeighbours()
    {
        QFETCH(QString, how);
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        QVERIFY(f.popOut(QStringLiteral("second"), how));
        ContainerDocument d = f.document("second");
        QCOMPARE(d.dockMode, DockMode::Floating);
        QVERIFY(d.home.has_value());
        QCOMPARE(*d.home, stripHome(DockEdge::Right, QStringLiteral("first"), QStringLiteral("third")));
        // It is in the saved text while the container is a window.
        QVERIFY(ContainerDocumentCodec::encode(f.store.snapshot()).contains("\"home\""));
        // The pop-out left the other two in their strip.
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"first", "third"}));

        f.returnByHeader(QStringLiteral("second"));
        d = f.document("second");
        QCOMPARE(d.dockMode, DockMode::PanelDocked);
        QCOMPARE(d.edge, DockEdge::Right);
        QVERIFY(!d.home.has_value());
        QVERIFY(!ContainerDocumentCodec::encode(f.store.snapshot()).contains("\"home\""));
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"first", "second", "third"}));
        QCOMPARE(f.shownStrip(DockEdge::Right), QStringList({"first", "second", "third"}));
        QVERIFY(!f.widget("second")->isFloating());
    }

    void aFreeContainerReturnsToItsRect_data() { threeWays(); }
    void aFreeContainerReturnsToItsRect()
    {
        QFETCH(QString, how);
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("free"), DockMode::OverlayDocked)}));
        QCOMPARE(f.widget("free")->geometry(), kFreeRect);
        QVERIFY(f.popOut(QStringLiteral("free"), how));
        ContainerDocument d = f.document("free");
        QVERIFY(d.home.has_value());
        QCOMPARE(*d.home, freeHome(kFreeRect, f.center->size()));

        f.returnByHeader(QStringLiteral("free"));
        d = f.document("free");
        QCOMPARE(d.dockMode, DockMode::OverlayDocked);
        QCOMPARE(d.geometry, kFreeRect);
        QVERIFY(!d.home.has_value());
        QVERIFY(f.widget("free")->isOverlayDocked());
        QCOMPARE(f.widget("free")->geometry(), kFreeRect);
        // And it stays there once the delayed save has run.
        QTest::qWait(kPastCommitDelayMs);
        settle();
        QCOMPARE(f.document("free").geometry, kFreeRect);
        QCOMPARE(f.widget("free")->geometry(), kFreeRect);
    }

    // The remembered rect is read against the pane it was placed in. A
    // container anchored to the bottom (the default anchor) moves down with
    // a pane that grows, and Return puts it where it was showing.
    void aFreeContainerReturnsWhereItWasShowingAfterThePaneChangedSize_data()
    {
        QTest::addColumn<bool>("beforePopOut");
        QTest::newRow("resized before the pop out") << true;
        QTest::newRow("resized while it was out") << false;
    }
    void aFreeContainerReturnsWhereItWasShowingAfterThePaneChangedSize()
    {
        QFETCH(bool, beforePopOut);
        constexpr int kTallerPx = 100;
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("free"), DockMode::OverlayDocked)}));
        QCOMPARE(f.widget("free")->geometry(), kFreeRect);
        const QSize paneWas = f.center->size();
        const auto growWindow = [&f] {
            f.window.resize(f.window.width(), f.window.height() + kTallerPx);
            settle();
            QTest::qWait(kPastCommitDelayMs);
            settle();
        };
        if (beforePopOut) {
            growWindow();
        }
        QVERIFY(f.popOut(QStringLiteral("free"), QStringLiteral("button")));
        if (!beforePopOut) {
            growWindow();
        }
        const QSize grown = f.center->size() - paneWas;
        QCOMPARE(grown, QSize(0, kTallerPx));
        const QRect showing = kFreeRect.translated(0, grown.height());

        f.returnByHeader(QStringLiteral("free"));
        QVERIFY(f.widget("free")->isOverlayDocked());
        QCOMPARE(f.widget("free")->geometry(), showing);
        // The place the operator gave it, with the pane it was given in.
        ContainerDocument d = f.document("free");
        QCOMPARE(d.geometry, kFreeRect);
        QCOMPARE(d.geometryPane, std::optional<QSize>(paneWas));
        QVERIFY(!d.home.has_value());
        QTest::qWait(kPastCommitDelayMs);
        settle();
        d = f.document("free");
        QCOMPARE(d.geometry, kFreeRect);
        QCOMPARE(d.geometryPane, std::optional<QSize>(paneWas));
        QCOMPARE(f.widget("free")->geometry(), showing);
    }

    void aSecondPopOutReplacesTheHome()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        f.returnByHeader(QStringLiteral("second"));
        f.manager->moveContainerToEdge(QStringLiteral("second"), DockEdge::Left);
        settle();
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        QCOMPARE(*f.document("second").home, stripHome(DockEdge::Left, QString(), QString()));
    }

    void returnWhenTheNeighbourBeforeItIsGoneGoesBeforeTheOneAfterIt()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("fourth"), DockMode::PanelDocked)}));
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        f.manager->moveContainerToEdge(QStringLiteral("first"), DockEdge::Left);
        settle();
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"third", "fourth"}));
        f.returnByHeader(QStringLiteral("second"));
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"second", "third", "fourth"}));
        QCOMPARE(f.shownStrip(DockEdge::Right), QStringList({"second", "third", "fourth"}));
    }

    void returnWhenBothNeighboursAreGoneGoesToTheEndOfTheStrip()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("fourth"), DockMode::PanelDocked),
                                    f.container(QStringLiteral("fifth"), DockMode::PanelDocked)}));
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        f.manager->moveContainerToEdge(QStringLiteral("first"), DockEdge::Left);
        f.manager->moveContainerToEdge(QStringLiteral("third"), DockEdge::Left);
        settle();
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"fourth", "fifth"}));
        f.returnByHeader(QStringLiteral("second"));
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"fourth", "fifth", "second"}));
        QCOMPARE(f.shownStrip(DockEdge::Right), QStringList({"fourth", "fifth", "second"}));
    }

    void returnToAStripWithNothingLeftOpensItAgain()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        f.manager->moveContainerToEdge(QStringLiteral("first"), DockEdge::Left);
        f.manager->moveContainerToEdge(QStringLiteral("third"), DockEdge::Left);
        settle();
        QVERIFY(f.strip(DockEdge::Right).isEmpty());
        QVERIFY(f.shownStrip(DockEdge::Right).isEmpty());
        f.returnByHeader(QStringLiteral("second"));
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"second"}));
        QCOMPARE(f.shownStrip(DockEdge::Right), QStringList({"second"}));
        QVERIFY(f.widget("second")->isVisible());
        QVERIFY(f.area->strip(DockEdge::Right)->isVisible());
    }

    void aFreeHomeNowOutsideThePaneIsShownInsideAndOffOtherContainers()
    {
        ReturnFixture f;
        ContainerDocument other = f.container(QStringLiteral("other"), DockMode::OverlayDocked);
        other.geometry = QRect(20, 20, 300, 100);
        // Anchored to the top left: its rect reads the same in any pane.
        ContainerDocument free = f.container(QStringLiteral("free"), DockMode::OverlayDocked);
        free.anchor = AxisLock::TopLeft;
        other.anchor = AxisLock::TopLeft;
        QVERIFY(f.showStripOfThree({free, other}));
        QVERIFY(f.popOut(QStringLiteral("free"), QStringLiteral("button")));
        // The pane is now too short for the place it came from.
        f.window.resize(1400, 330);
        settle();
        QTest::qWait(kPastCommitDelayMs);
        settle();
        const QRect pane(QPoint(0, 0), f.center->size());
        QVERIFY2(!pane.contains(kFreeRect), "the pane still holds the home rect");
        // Another free container sits where the nearest place inside is.
        const QRect nearest = FloatingContainer::clampedGeometry(kFreeRect, pane, QSize(260, 24));
        WorkspaceDocument draft = f.store.snapshot();
        for (auto& d : draft.containers) {
            if (d.id == QStringLiteral("other")) {
                d.geometry = nearest;
                d.geometryPane.reset();
            }
        }
        QCOMPARE(f.manager->commitWorkspace(draft, draft.revision).status, CommitStatus::Saved);
        settle();
        QCOMPARE(f.widget("other")->geometry(), nearest);

        f.returnByHeader(QStringLiteral("free"));
        const ContainerDocument d = f.document("free");
        QCOMPARE(d.dockMode, DockMode::OverlayDocked);
        QVERIFY(!d.home.has_value());
        const QRect shown = f.widget("free")->geometry();
        const QRect taken = f.widget("other")->geometry();
        const auto text = [](const QRect& r) {
            return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
        };
        QVERIFY2(pane.contains(shown), qPrintable(QStringLiteral("shown %1 in a pane %2").arg(text(shown), text(pane))));
        QVERIFY2(!shown.intersects(taken),
                 qPrintable(QStringLiteral("shown %1 covers the other at %2").arg(text(shown), text(taken))));
        // The least move that clears it: beside it, on the right (there is
        // no room on its left, above it or below it).
        QCOMPARE(shown, nearest.translated(nearest.width(), 0));
        QCOMPARE(d.geometry, shown);
    }

    void aFreeHomeWithNoClearSpotInThePaneStepsOffTheOtherContainer()
    {
        ReturnFixture f;
        ContainerDocument free = f.container(QStringLiteral("free"), DockMode::OverlayDocked);
        free.anchor = AxisLock::TopLeft;
        ContainerDocument other = f.container(QStringLiteral("other"), DockMode::OverlayDocked);
        other.anchor = AxisLock::TopLeft;
        QVERIFY(f.showStripOfThree({free, other}));
        QVERIFY(f.popOut(QStringLiteral("free"), QStringLiteral("button")));
        f.window.resize(1400, 330);
        settle();
        QTest::qWait(kPastCommitDelayMs);
        settle();
        const QRect pane(QPoint(0, 0), f.center->size());
        QVERIFY2(!pane.contains(kFreeRect), "the pane still holds the home rect");
        // The other free container fills the pane: nowhere is clear of it.
        WorkspaceDocument draft = f.store.snapshot();
        for (auto& d : draft.containers) {
            if (d.id == QStringLiteral("other")) {
                d.geometry = pane;
                d.geometryPane.reset();
            }
        }
        QCOMPARE(f.manager->commitWorkspace(draft, draft.revision).status, CommitStatus::Saved);
        settle();
        QCOMPARE(f.widget("other")->geometry(), pane);

        f.returnByHeader(QStringLiteral("free"));
        const ContainerDocument d = f.document("free");
        QCOMPARE(d.dockMode, DockMode::OverlayDocked);
        const QRect shown = f.widget("free")->geometry();
        QVERIFY(pane.contains(shown));
        QVERIFY(shown.topLeft() != f.widget("other")->geometry().topLeft());
        QCOMPARE(d.geometry, shown);
    }

    // Finding its way through the manager, a placement chosen in Container
    // Settings is a pop-out or a return like any other.
    void placementChosenInContainerSettingsRecordsAndClearsTheHome()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        const auto choose = [&f](DockMode mode) {
            ContainerSettingsDialog dialog(f.widget("second"), nullptr, f.manager.get());
            auto* placement = dialog.findChild<QComboBox*>(QStringLiteral("containerPlacement"));
            if (placement == nullptr) {
                return false;
            }
            placement->setCurrentIndex(placement->findData(int(mode)));
            return dialog.applyDraft().status == CommitStatus::Saved;
        };
        QVERIFY(choose(DockMode::Floating));
        settle();
        ContainerDocument d = f.document("second");
        QCOMPARE(d.dockMode, DockMode::Floating);
        QVERIFY(d.home.has_value());
        QCOMPARE(*d.home, stripHome(DockEdge::Right, QStringLiteral("first"), QStringLiteral("third")));

        QVERIFY(choose(DockMode::OverlayDocked));
        settle();
        d = f.document("second");
        QCOMPARE(d.dockMode, DockMode::OverlayDocked);
        QVERIFY(!d.home.has_value());
        // Popped out again, the old strip is not its home: it has the free
        // place it was popped out of, or none when that place had no rect.
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        d = f.document("second");
        QVERIFY(!d.home.has_value() || d.home->dockMode == DockMode::OverlayDocked);
    }

    void aWindowWithNoHomeReturnsToTheMainStrip()
    {
        // An older document, or a container created as a window.
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("window"), DockMode::Floating)}));
        QVERIFY(!f.document("window").home.has_value());
        f.returnByHeader(QStringLiteral("window"));
        const ContainerDocument d = f.document("window");
        QCOMPARE(d.dockMode, DockMode::PanelDocked);
        QCOMPARE(d.edge, DockEdge::Right);
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"first", "second", "third", "window"}));
    }

    void theMainContainerWithNoHomeReturnsToItsStrip()
    {
        ReturnFixture f;
        ContainerDocument main = f.container(QStringLiteral("first"), DockMode::Floating, DockEdge::Left);
        QVERIFY(f.show({main, f.container(QStringLiteral("second"), DockMode::PanelDocked)}));
        f.returnByHeader(QStringLiteral("first"));
        QCOMPARE(f.document("first").dockMode, DockMode::PanelDocked);
        QCOMPARE(f.document("first").edge, DockEdge::Left);
    }

    void aWindowDockedByAnArrangingDragDoesNotGoHome()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        QVERIFY(f.popOut(QStringLiteral("second"), QStringLiteral("button")));
        QVERIFY(f.document("second").home.has_value());
        f.manager->setArrangeMode(true);
        settle();
        QWidget* grip = f.header("second");
        const QPoint grab = grip->mapToGlobal(QPoint(grip->width() / 2, grip->height() / 2));
        press(grip, grab);
        move(grip, grab + QPoint(0, 30));
        move(grip, f.leftZone());
        release(grip, f.leftZone());
        settle();
        const ContainerDocument d = f.document("second");
        QCOMPARE(d.dockMode, DockMode::PanelDocked);
        QCOMPARE(d.edge, DockEdge::Left);
        QVERIFY(!d.home.has_value());
        QCOMPARE(f.strip(DockEdge::Right), QStringList({"first", "third"}));
    }

    void thePinShowsOnlyOnAWindowAndIsSavedWithTheContainer()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("window"), DockMode::Floating)}));
        // Docked: no pin.
        QVERIFY(f.header("second")->pinButton() != nullptr);
        QVERIFY(!f.header("second")->pinButton()->isVisibleTo(f.header("second")));

        WorkspaceGroupHeader* header = f.header("window");
        QAbstractButton* pin = header->pinButton();
        QVERIFY(pin->isVisibleTo(header));
        QCOMPARE(pin->size(), QSize(16, 14));
        // Left of the return arrow, right of the eye.
        QVERIFY(pin->geometry().right() < header->popOutButton()->geometry().left());
        QVERIFY(pin->geometry().left() > header->keepShownButton()->geometry().right());
        QVERIFY(!pin->isChecked());
        QCOMPARE(pin->property(AppIcon::kIconProperty).toString(), QStringLiteral("pin"));
        QCOMPARE(pin->iconSize(), QSize(WorkspaceGroupHeader::kHeaderPinIconPx,
                                        WorkspaceGroupHeader::kHeaderPinIconPx));
        QVERIFY(!f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        const auto optionsMenu = [&f] {
            return readMenu([&f] { f.header("window")->optionsButton()->click(); }, QString(),
                            QStringLiteral("Pin on Top"));
        };
        MenuReading menu = optionsMenu();
        QVERIFY(menu.opened);
        QVERIFY(menu.texts.contains(QStringLiteral("Pin on Top")));
        QVERIFY(!menu.watchedTicked);

        pin->click();
        settle();
        QVERIFY(f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(f.shell("window")->isVisible());
        QVERIFY(pin->isChecked());
        QCOMPARE(pin->property(AppIcon::kIconProperty).toString(), QStringLiteral("pinned"));
        QVERIFY(f.widget("window")->isPinOnTop());
        QCOMPARE(f.document("window").config.value("pinOnTop").toBool(), true);
        menu = optionsMenu();
        QVERIFY(menu.opened);
        QVERIFY(menu.watchedTicked);
        // An unrelated commit does not undo it.
        f.manager->moveContainerToEdge(QStringLiteral("third"), DockEdge::Left);
        settle();
        QVERIFY(pin->isChecked());
        QVERIFY(f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));

        pin->click();
        settle();
        QVERIFY(!f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(!pin->isChecked());
        QCOMPARE(pin->property(AppIcon::kIconProperty).toString(), QStringLiteral("pin"));
        QCOMPARE(f.document("window").config.value("pinOnTop").toBool(), false);
        menu = optionsMenu();
        QVERIFY(menu.opened);
        QVERIFY(!menu.watchedTicked);

        // The menu item is the same switch.
        QVERIFY(readMenu([&f] { f.header("window")->optionsButton()->click(); },
                         QStringLiteral("Pin on Top")).opened);
        settle();
        QVERIFY(pin->isChecked());
        QCOMPARE(f.document("window").config.value("pinOnTop").toBool(), true);
        QVERIFY(f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    }

    void aSavedPinnedWindowComesBackPinned()
    {
        ReturnFixture f;
        ContainerDocument window = f.container(QStringLiteral("window"), DockMode::Floating);
        window.config["pinOnTop"] = true;
        QVERIFY(f.showStripOfThree({window}));
        QAbstractButton* pin = f.header("window")->pinButton();
        QVERIFY(pin->isVisibleTo(f.header("window")));
        QVERIFY(pin->isChecked());
        QCOMPARE(pin->property(AppIcon::kIconProperty).toString(), QStringLiteral("pinned"));
        QVERIFY(f.shell("window")->windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    }

    // A window an operator command makes is saved with the place and the
    // monitor it was given, so the next start does not place it afresh.
    void aPoppedOutWindowIsSavedWhereItWasPut_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("how");
        for (const QString& id : {QStringLiteral("second"), QStringLiteral("free")}) {
            for (const QString& how : {QStringLiteral("button"), QStringLiteral("menu"), QStringLiteral("drag")}) {
                QTest::addRow("%s by %s", qPrintable(id), qPrintable(how)) << id << how;
            }
        }
    }
    void aPoppedOutWindowIsSavedWhereItWasPut()
    {
        QFETCH(QString, id);
        QFETCH(QString, how);
        ReturnFixture f;
        QVERIFY(f.showStripOfThree({f.container(QStringLiteral("free"), DockMode::OverlayDocked)}));
        QVERIFY(f.popOut(id, how));
        QTest::qWait(kPastCommitDelayMs);
        settle();
        const ContainerDocument d = f.document(id);
        QVERIFY(d.geometry.isValid());
        QCOMPARE(d.geometry.topLeft(), f.shell(id)->geometry().topLeft());
        QVERIFY2(d.screen.has_value(), "the window was saved without its monitor");
        QCOMPARE(*d.screen, savedScreenOf(f.screens.list.first()));
    }

    void aFreeContainerAtThePaneOriginPoppedOutIsSavedWhereItIsShown()
    {
        // Its rect read as a screen rect is at the origin, where no window
        // is ever put: the app gives it a place, and that place is saved.
        ReturnFixture f;
        ContainerDocument free = f.container(QStringLiteral("free"), DockMode::OverlayDocked);
        free.geometry = QRect(0, 0, 320, 240);
        QVERIFY(f.showStripOfThree({free}));
        QVERIFY(f.popOut(QStringLiteral("free"), QStringLiteral("button")));
        QTest::qWait(kPastCommitDelayMs);
        settle();
        const ContainerDocument d = f.document("free");
        const QRect shown = f.shell("free")->geometry();
        QVERIFY(shown.topLeft() != QPoint(0, 0));
        QCOMPARE(d.geometry.topLeft(), shown.topLeft());
        QVERIFY(d.screen.has_value());
        // Its home is still the free place it had.
        QCOMPARE(*d.home, freeHome(QRect(0, 0, 320, 240), f.center->size()));
    }

    void newWindowsAreSavedWhereTheyWerePut()
    {
        ReturnFixture f;
        QVERIFY(f.showStripOfThree());
        ContainerWidget* a = f.manager->createContainer(1, DockMode::Floating);
        QVERIFY(a != nullptr);
        settle();
        ContainerWidget* b = f.manager->createContainer(1, DockMode::Floating);
        QVERIFY(b != nullptr);
        settle();
        QTest::qWait(kPastCommitDelayMs);
        settle();
        const ContainerDocument da = f.document(a->id());
        const ContainerDocument db = f.document(b->id());
        QVERIFY2(da.geometry.isValid() && db.geometry.isValid(), "a new window was saved with no place");
        QCOMPARE(da.geometry, f.shell(a->id())->geometry());
        QCOMPARE(db.geometry, f.shell(b->id())->geometry());
        QVERIFY(da.geometry.topLeft() != db.geometry.topLeft());
        QVERIFY(da.screen.has_value());
        QVERIFY(db.screen.has_value());
        // Created as windows: nowhere to go back to but the main strip.
        QVERIFY(!da.home.has_value());
    }
};

QTEST_MAIN(TstWorkspacePopoutReturn)
#include "tst_workspace_popout_return.moc"
