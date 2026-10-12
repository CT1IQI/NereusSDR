// no-port-check: NereusSDR-original regressions for a floating container's size during a drag and on its screen.
// Modification history (NereusSDR):
//   2026-10-12 - A drag moves a floating container's window and never
//                 resizes it (a window dragged across the join of two
//                 screens grew to 32767 x 25223 on Windows and stopped the
//                 app); the height follower leaves a dragged window alone;
//                 a window is never larger than its screen, whatever size
//                 was saved; and a resize by the grip is still the
//                 operator's. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include <QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QStatusBar>
#include <QTemporaryDir>
#include <memory>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ScreenSource.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

constexpr int kSaveTimeoutMs = 10000;
// One monitor with a menu bar and a task bar's worth taken off it.
constexpr QRect kScreen(0, 0, 2560, 1440);
constexpr QRect kAvailable(0, 25, 2560, 1375);
// Clear of the main window (200,100 1400x900), so a drop leaves a window.
constexpr QRect kWindowRect(1800, 300, 360, 300);
// The size a window had grown to when the app stopped (Windows, 2026-10-12).
constexpr QSize kAbsurdSize(28458, 22355);
const QString kId = QStringLiteral("floater");

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

class FakeScreens : public ScreenSource {
public:
    QVector<ScreenInfo> list{ScreenInfo{QStringLiteral("Bench"), QStringLiteral("SN-1"), kScreen, kAvailable}};
    QVector<ScreenInfo> screens() const override { return list; }
    ScreenInfo primary() const override { return list.first(); }
};

// Counts the "changed size during a drag" warnings and keeps them out of
// the test's output; everything else goes to the handler before.
int g_sizeWarnings = 0;
QString g_lastSizeWarning;
QtMessageHandler g_previousHandler = nullptr;

void countSizeWarnings(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg && message.contains(QStringLiteral("changed size during a drag"))) {
        ++g_sizeWarnings;
        g_lastSizeWarning = message;
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

// Counts the resizes of one widget.
class ResizeCounter : public QObject {
public:
    int count = 0;
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Resize) {
            ++count;
        }
        return false;
    }
};

// How a case runs: the look, and with it whether Arrange is on (a header
// drag then goes through the manager's drop rule).
struct Mode {
    bool lookOn;
    bool arranging;
};

void modes()
{
    QTest::addColumn<bool>("lookOn");
    QTest::addColumn<bool>("arranging");
    QTest::newRow("look off") << false << false;
    QTest::newRow("look on") << true << false;
    QTest::newRow("look on, arranging") << true << true;
}

// Offline fixture: a main window and one floating container. With the look
// on the main window holds the four-edge area, as in MainWindow, and a
// main container in its right strip. No radio, discovery or poller.
struct DragRig {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath(QStringLiteral("drag-size.xml"))};
    ContainerWorkspaceStore store{settings};
    ContainerContentRegistry registry;
    FakeScreens screens;
    QMainWindow window;
    std::unique_ptr<ContainerManager> manager;
    bool lookOn;

    explicit DragRig(const Mode& mode) : lookOn(mode.lookOn) {}

    ContainerDocument container(const QString& id, DockMode dockMode) const
    {
        ContainerDocument d;
        d.id = id;
        d.name = id;
        d.layout = ContentLayout::VerticalStack;
        d.dockMode = dockMode;
        d.edge = DockEdge::Right;
        d.header = HeaderMode::Shown;
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        entry.id = id + QStringLiteral("-meter");
        entry.context["sliceId"] = 0;
        d.contents.append(entry);
        return d;
    }

    // Saves a workspace holding `floater`, then builds the manager and
    // restores it, as a start of the app does.
    bool restore(ContainerDocument floater)
    {
        floater.id = kId;
        floater.name = kId;
        WorkspaceDocument document = store.snapshot();
        if (lookOn) {
            document.containers = {container(QStringLiteral("main"), DockMode::PanelDocked), floater};
        } else {
            document.containers = {floater};
        }
        document.mainContainerId = document.containers.first().id;
        if (store.commit(document, document.revision).status != CommitStatus::Saved) {
            return false;
        }
        window.statusBar()->setSizeGripEnabled(false);
        window.move(200, 100);
        window.resize(1400, 900);
        if (lookOn) {
            auto* middle = new EdgeDockSplitter(Qt::Horizontal, &window);
            auto* center = new QWidget(middle);
            center->setMinimumWidth(400);
            middle->addWidget(center);
            auto* area = new EdgeDockArea(middle, center, &window);
            window.setCentralWidget(area);
            manager = std::make_unique<ContainerManager>(center, middle);
            manager->setScreenSourceForTest(&screens);
            manager->setEdgeDockArea(area);
        } else {
            auto* pane = new QWidget(&window);
            window.setCentralWidget(pane);
            manager = std::make_unique<ContainerManager>(pane, nullptr);
            manager->setScreenSourceForTest(&screens);
        }
        manager->setWorkspaceAdapter(&store, &registry);
        manager->restoreState();
        window.show();
        if (!QTest::qWaitForWindowExposed(&window)) {
            return false;
        }
        settle();
        return widget() != nullptr && widget()->isFloating();
    }

    ContainerDocument floater(bool autoHeight, const QRect& geometry = kWindowRect) const
    {
        ContainerDocument d = container(kId, DockMode::Floating);
        d.geometry = geometry;
        d.autoHeight = autoHeight;
        return d;
    }

    ContainerWidget* widget() const { return manager->container(kId); }
    QWidget* form() const { return widget()->window(); }
    // What a drag takes hold of: the group header, or the title bar with
    // the look off.
    QWidget* handle() const
    {
        if (lookOn) {
            return widget()->workspaceHeader();
        }
        return widget()->findChild<QWidget*>(QStringLiteral("containerTitleBar"));
    }
    ContainerDocument stored() const
    {
        for (const auto& d : store.snapshot().containers) {
            if (d.id == kId) {
                return d;
            }
        }
        return {};
    }
    bool saved()
    {
        return QTest::qWaitFor([this] { return !manager->geometryCommitPendingForTest(); }, kSaveTimeoutMs);
    }
};

} // namespace

class TstContainerDragKeepsSize : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { g_previousHandler = qInstallMessageHandler(countSizeWarnings); }
    void cleanupTestCase()
    {
        qInstallMessageHandler(g_previousHandler);
        WorkspacePreview::setEnabledForTest(false);
    }
    void init()
    {
        g_sizeWarnings = 0;
        g_lastSizeWarning.clear();
    }
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }

    // The platform resizes the window while a drag moves it (Windows, on a
    // change of screen). The next move, a turn of the event loop and the
    // release each find it back at its press size, and that size is saved.
    void aDragNeverChangesTheWindowsSize_data() { modes(); }
    void aDragNeverChangesTheWindowsSize()
    {
        QFETCH(bool, lookOn);
        QFETCH(bool, arranging);
        WorkspacePreview::setEnabledForTest(lookOn);
        DragRig rig({lookOn, arranging});
        QVERIFY(rig.restore(rig.floater(false)));
        if (arranging) {
            rig.manager->setArrangeMode(true);
            settle();
        }
        QVERIFY(rig.saved());
        QWidget* form = rig.form();
        QWidget* handle = rig.handle();
        QVERIFY(handle);
        const QSize pressSize = form->size();
        const QPoint pressPos = form->pos();
        const QSize grown(pressSize.width() * 2, pressSize.height() * 2);
        QVERIFY(grown.width() <= kAvailable.width() && grown.height() <= kAvailable.height());

        const QPoint grab = handle->mapToGlobal(handle->rect().center());
        press(handle, grab);
        move(handle, grab + QPoint(40, 30));
        QCOMPARE(form->size(), pressSize);
        QCOMPARE(g_sizeWarnings, 0);

        // Resized from outside; the next move puts the size back.
        form->resize(grown);
        QCOMPARE(form->size(), grown);
        move(handle, grab + QPoint(60, 40));
        QCOMPARE(form->size(), pressSize);
        QCOMPARE(g_sizeWarnings, 1);
        QVERIFY2(g_lastSizeWarning.contains(kId), qPrintable(g_lastSizeWarning));
        QVERIFY2(g_lastSizeWarning.contains(QStringLiteral("found %1x%2").arg(grown.width()).arg(grown.height())),
                 qPrintable(g_lastSizeWarning));
        QVERIFY2(g_lastSizeWarning.contains(
                     QStringLiteral("restored %1x%2").arg(pressSize.width()).arg(pressSize.height())),
                 qPrintable(g_lastSizeWarning));
        QVERIFY2(g_lastSizeWarning.contains(QStringLiteral("screen ")), qPrintable(g_lastSizeWarning));
        QVERIFY2(g_lastSizeWarning.contains(QStringLiteral("devicePixelRatio ")), qPrintable(g_lastSizeWarning));

        // Resized again with no move after it: a turn of the event loop
        // puts it back (never from inside the resize event itself).
        form->resize(grown);
        QCOMPARE(form->size(), grown);
        settle();
        QCOMPARE(form->size(), pressSize);

        // And once more just before the release.
        form->resize(grown);
        release(handle, grab + QPoint(60, 40));
        QCOMPARE(form->size(), pressSize);
        settle();
        QCOMPARE(form->size(), pressSize);
        // One line for the whole drag.
        QCOMPARE(g_sizeWarnings, 1);

        // It moved, it is still a window, and its press size is what is saved.
        QVERIFY(rig.widget()->isFloating());
        QVERIFY(form->pos() != pressPos);
        QTRY_COMPARE_WITH_TIMEOUT(rig.stored().geometry.topLeft(), form->pos(), kSaveTimeoutMs);
        QVERIFY(rig.saved());
        QCOMPARE(rig.stored().geometry.size(), pressSize);
        QCOMPARE(form->size(), pressSize);

        // The next drag warns again if it has to: the count is per drag.
        const QPoint again = handle->mapToGlobal(handle->rect().center());
        press(handle, again);
        move(handle, again + QPoint(30, 20));
        form->resize(grown);
        move(handle, again + QPoint(40, 30));
        release(handle, again + QPoint(40, 30));
        settle();
        QCOMPARE(form->size(), pressSize);
        QCOMPARE(g_sizeWarnings, 2);
        QVERIFY(rig.saved());
    }

    // A window saved at 28458 x 22355 is shown no larger than its screen's
    // available area, and the next save writes that size.
    void aSavedAbsurdSizeIsShownWithinItsScreen_data()
    {
        QTest::addColumn<bool>("lookOn");
        QTest::addColumn<bool>("screenSaved");
        QTest::newRow("look off, no monitor saved") << false << false;
        QTest::newRow("look off, monitor saved") << false << true;
        QTest::newRow("look on, no monitor saved") << true << false;
        QTest::newRow("look on, monitor saved") << true << true;
    }
    void aSavedAbsurdSizeIsShownWithinItsScreen()
    {
        QFETCH(bool, lookOn);
        QFETCH(bool, screenSaved);
        WorkspacePreview::setEnabledForTest(lookOn);
        DragRig rig({lookOn, false});
        ContainerDocument saved = rig.floater(false, QRect(kWindowRect.topLeft(), kAbsurdSize));
        if (screenSaved) {
            saved.screen = savedScreenOf(rig.screens.primary());
        }
        QVERIFY(rig.restore(saved));
        QCOMPARE(rig.stored().geometry.size(), kAbsurdSize);
        const QSize shown = rig.form()->size();
        QVERIFY2(shown.width() <= kAvailable.width() && shown.height() <= kAvailable.height(),
                 qPrintable(QStringLiteral("%1x%2").arg(shown.width()).arg(shown.height())));
        QVERIFY(rig.form()->isVisible());

        rig.manager->saveState();
        QVERIFY(rig.saved());
        const QSize written = rig.stored().geometry.size();
        QVERIFY2(written.width() <= kAvailable.width() && written.height() <= kAvailable.height(),
                 qPrintable(QStringLiteral("%1x%2").arg(written.width()).arg(written.height())));
        settle();
        QCOMPARE(rig.form()->size(), shown);
        QCOMPARE(g_sizeWarnings, 0);
    }

    // A window whose height follows its content is not resized while a
    // drag moves it, whatever its content asks for; the end of the drag
    // gives it that height.
    void theHeightFollowerLeavesADraggedWindowAlone_data() { modes(); }
    void theHeightFollowerLeavesADraggedWindowAlone()
    {
        QFETCH(bool, lookOn);
        QFETCH(bool, arranging);
        WorkspacePreview::setEnabledForTest(lookOn);
        DragRig rig({lookOn, arranging});
        QVERIFY(rig.restore(rig.floater(true)));
        if (arranging) {
            rig.manager->setArrangeMode(true);
            settle();
        }
        QVERIFY(rig.saved());
        QWidget* form = rig.form();
        QWidget* handle = rig.handle();
        ContainerContentHost* host = rig.manager->contentHost(kId);
        QVERIFY(handle && host);
        const int natural = form->height();

        // The follower works: a height that is not the content's is put
        // right when the content next says what it needs.
        form->resize(form->width(), natural + 50);
        settle();
        QCOMPARE(form->height(), natural + 50);
        emit host->preferredContentHeightChanged();
        settle();
        QCOMPARE(form->height(), natural);

        form->resize(form->width(), natural + 50);
        settle();
        QVERIFY(rig.saved());
        QCOMPARE(form->height(), natural + 50);
        const QSize pressSize = form->size();
        ResizeCounter resizes;
        form->installEventFilter(&resizes);
        const QPoint grab = handle->mapToGlobal(handle->rect().center());
        press(handle, grab);
        move(handle, grab + QPoint(40, 30));
        emit host->preferredContentHeightChanged();
        settle();
        move(handle, grab + QPoint(60, 40));
        settle();
        QCOMPARE(resizes.count, 0);
        QCOMPARE(form->size(), pressSize);
        QCOMPARE(g_sizeWarnings, 0);

        // It resumes when the drag ends.
        release(handle, grab + QPoint(60, 40));
        settle();
        form->removeEventFilter(&resizes);
        QVERIFY(rig.widget()->isFloating());
        QCOMPARE(form->height(), natural);
        QCOMPARE(form->width(), pressSize.width());
        QCOMPARE(g_sizeWarnings, 0);
        QVERIFY(rig.saved());
    }

    // A resize by the corner grip is the operator's: it sticks, it is
    // saved, a drag afterwards keeps it, and it stops at the screen's
    // available area.
    void aResizeByTheGripStillSticks_data() { modes(); }
    void aResizeByTheGripStillSticks()
    {
        QFETCH(bool, lookOn);
        QFETCH(bool, arranging);
        WorkspacePreview::setEnabledForTest(lookOn);
        DragRig rig({lookOn, arranging});
        QVERIFY(rig.restore(rig.floater(false)));
        if (arranging) {
            rig.manager->setArrangeMode(true);
            settle();
        }
        QVERIFY(rig.saved());
        QWidget* form = rig.form();
        auto* grip = rig.widget()->findChild<QWidget*>(QStringLiteral("containerResizeGrip"));
        QVERIFY(grip);
        const QSize before = form->size();
        const QPoint place = form->pos();
        const QPoint corner = grip->mapToGlobal(grip->rect().center());
        press(grip, corner);
        move(grip, corner + QPoint(40, 30));
        release(grip, corner + QPoint(40, 30));
        settle();
        const QSize resized = before + QSize(40, 30);
        QCOMPARE(form->size(), resized);
        QCOMPARE(form->pos(), place);
        QTRY_COMPARE_WITH_TIMEOUT(rig.stored().geometry.size(), resized, kSaveTimeoutMs);
        QVERIFY(rig.saved());
        QCOMPARE(form->size(), resized);

        // A drag afterwards holds the new size, not the old one.
        QWidget* handle = rig.handle();
        QVERIFY(handle);
        const QPoint grab = handle->mapToGlobal(handle->rect().center());
        press(handle, grab);
        move(handle, grab + QPoint(40, 30));
        release(handle, grab + QPoint(40, 30));
        settle();
        QCOMPARE(form->size(), resized);
        QVERIFY(rig.widget()->isFloating());
        QVERIFY(rig.saved());
        QCOMPARE(rig.stored().geometry.size(), resized);
        QCOMPARE(g_sizeWarnings, 0);

        // Pulled far past the screen: no larger than its available area.
        const QPoint far = grip->mapToGlobal(grip->rect().center());
        press(grip, far);
        move(grip, far + QPoint(30000, 30000));
        QCOMPARE(form->size(), kAvailable.size());
        release(grip, far + QPoint(30000, 30000));
        settle();
        QCOMPARE(form->size(), kAvailable.size());
        QTRY_COMPARE_WITH_TIMEOUT(rig.stored().geometry.size(), kAvailable.size(), kSaveTimeoutMs);
        QVERIFY(rig.saved());
    }
};

QTEST_MAIN(TstContainerDragKeepsSize)
#include "tst_container_drag_keeps_size.moc"
