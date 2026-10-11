// no-port-check: NereusSDR-original offscreen regressions for the monitor a container window remembers and for default spots.
// Modification history (NereusSDR):
//   2026-10-11 - Floating and popped-out windows remember their monitor:
//                 restored exactly when it is present, shown on the main
//                 window's screen when it is not with the saved place left
//                 alone, a screen removed and added back while running,
//                 older layouts with no monitor, operator moves, and no
//                 two default placements on one spot. The offscreen
//                 platform has one screen, so every monitor here is a fake
//                 ScreenSource. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QLoggingCategory>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ScreenSource.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

constexpr QSize kPaneSize(1000, 700);
constexpr QPoint kHostAt(100, 100);
constexpr QRect kScreenARect(0, 0, 1920, 1080);
constexpr QRect kScreenBRect(1920, 0, 1920, 1080);
constexpr QSize kWindowSize(400, 300);
constexpr QRect kOnB(2100, 200, 400, 300);
constexpr QRect kAlsoOnB(2500, 400, 400, 300);
constexpr QRect kStraddling(1800, 200, 400, 300);
// The geometry capture's own debounce is 200 ms.
constexpr int kNothingSavedWaitMs = 300;
constexpr int kSaveTimeoutMs = 10000;

ScreenInfo screenA()
{
    // A menu bar's worth less than the whole screen.
    return {QStringLiteral("A"), QString(), kScreenARect, kScreenARect.adjusted(0, 25, 0, 0)};
}

ScreenInfo screenB()
{
    return {QStringLiteral("B"), QString(), kScreenBRect, kScreenBRect};
}

// The monitors of a case: whatever the case says is plugged in.
class FakeScreens : public ScreenSource
{
public:
    QVector<ScreenInfo> list;

    QVector<ScreenInfo> screens() const override { return list; }
    ScreenInfo primary() const override { return list.isEmpty() ? ScreenInfo{} : list.first(); }
};

ContainerDocument floatingContainer(const QString& id, const QRect& geometry,
                                    const std::optional<SavedScreen>& screen = std::nullopt)
{
    ContainerDocument d;
    d.id = id;
    d.name = id;
    d.layout = ContentLayout::VerticalStack;
    d.dockMode = DockMode::Floating;
    d.geometry = geometry;
    d.screen = screen;
    d.config = {{QStringLiteral("enabled"), true},
                {QStringLiteral("showOnRx"), true},
                {QStringLiteral("showOnTx"), true},
                {QStringLiteral("border"), true},
                {QStringLiteral("pinOnTop"), false}};
    return d;
}

WorkspaceDocument workspace(const QVector<ContainerDocument>& containers)
{
    WorkspaceDocument document;
    document.mainContainerId = containers.first().id;
    document.containers = containers;
    return document;
}

// A main window on screen A with a spectrum pane, and a manager that sees
// only the fake screens.
struct Rig {
    QWidget host;
    QWidget* pane{nullptr};
    ContainerContentRegistry registry;
    std::unique_ptr<ContainerManager> manager;

    Rig(ContainerWorkspaceStore& store, FakeScreens& screens)
    {
        auto* layout = new QVBoxLayout(&host);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        pane = new QWidget(&host);
        layout->addWidget(pane);
        host.setGeometry(QRect(kHostAt, kPaneSize));
        manager = std::make_unique<ContainerManager>(pane, nullptr);
        manager->setScreenSourceForTest(&screens);
        manager->setWorkspaceAdapter(&store, &registry);
    }

    bool show()
    {
        host.show();
        return QTest::qWaitFor([this] { return pane->size() == host.size(); }, kSaveTimeoutMs);
    }

    // The rect of a floating container's window, in screen coordinates.
    QRect window(const QString& id) const
    {
        ContainerWidget* c = manager->container(id);
        return c != nullptr && c->isFloating() ? c->window()->geometry() : QRect();
    }

    QRect free(const QString& id) const
    {
        ContainerWidget* c = manager->container(id);
        return c != nullptr ? c->geometry() : QRect();
    }

    // Lets a scheduled geometry capture run, and proves it has.
    bool settleCapture()
    {
        QTest::qWait(kNothingSavedWaitMs);
        return QTest::qWaitFor([this] { return !manager->geometryCommitPendingForTest(); }, kSaveTimeoutMs);
    }
};

ContainerDocument stored(const ContainerWorkspaceStore& store, const QString& id)
{
    for (const auto& d : store.snapshot().containers) {
        if (d.id == id) {
            return d;
        }
    }
    return {};
}

} // namespace

class TstContainerWindowMonitors : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase() { QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false")); }
    void init() { WorkspacePreview::setEnabledForTest(false); }
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }

    void aScreenMatchesBySerialOrElseByNameAndGeometry()
    {
        const ScreenInfo present{QStringLiteral("DELL"), QStringLiteral("SN1"), kScreenBRect, kScreenBRect};
        // The serial decides when there is one, wherever the monitor now is.
        QVERIFY(screenMatches(present, {QStringLiteral("other"), QStringLiteral("SN1"), kScreenARect}));
        QVERIFY(!screenMatches(present, {QStringLiteral("other"), QStringLiteral("SN2"), kScreenARect}));
        // With no serial saved, name and geometry together.
        QVERIFY(screenMatches(present, {QStringLiteral("DELL"), QString(), kScreenBRect}));
        QVERIFY(!screenMatches(present, {QStringLiteral("DELL"), QString(), kScreenARect}));
        QVERIFY(!screenMatches(present, {QStringLiteral("other"), QString(), kScreenBRect}));
        // An empty serial on both sides is not a match by itself.
        const ScreenInfo unnamed{QStringLiteral("LG"), QString(), kScreenARect, kScreenARect};
        QVERIFY(!screenMatches(unnamed, {QStringLiteral("DELL"), QString(), kScreenBRect}));
        QCOMPARE(savedScreenOf(present), (SavedScreen{QStringLiteral("DELL"), QStringLiteral("SN1"), kScreenBRect}));
    }

    void windowSavedOnTheSecondScreenIsRestoredThere()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("second.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB()))}), 0)
                     .status,
                 CommitStatus::Saved);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QCOMPARE(rig.window(QStringLiteral("a")), kOnB);
    }

    // Spec "The stacking bug", cause 2: a window across two monitors was
    // recentred on every start.
    void windowAcrossTwoScreensIsRestoredExactly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("straddle.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kStraddling,
                                                           savedScreenOf(screenA()))}), 0)
                     .status,
                 CommitStatus::Saved);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QCOMPARE(rig.window(QStringLiteral("a")), kStraddling);
        QVERIFY(rig.settleCapture());
        QCOMPARE(stored(store, QStringLiteral("a")).geometry, kStraddling);
    }

    void windowsOfAnAbsentScreenAreShownOnTheMainScreenAndKeepTheirPlace()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("absent.settings"));
        {
            AppSettings settings(path);
            ContainerWorkspaceStore store(settings);
            QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB())),
                                             floatingContainer(QStringLiteral("b"), kAlsoOnB,
                                                               savedScreenOf(screenB()))}), 0)
                         .status,
                     CommitStatus::Saved);
        }
        QByteArray before;
        // A start with screen B unplugged.
        {
            AppSettings settings(path);
            settings.load();
            ContainerWorkspaceStore store(settings);
            QVERIFY(store.load().ok);
            before = ContainerDocumentCodec::encode(store.snapshot());
            FakeScreens screens;
            screens.list = {screenA()};
            Rig rig(store, screens);
            rig.manager->restoreState();
            QVERIFY(rig.show());
            const QRect a = rig.window(QStringLiteral("a"));
            const QRect b = rig.window(QStringLiteral("b"));
            qInfo() << "screen B absent, a:" << a << "b:" << b;
            QVERIFY2(kScreenARect.contains(a), "a is wholly on screen A");
            QVERIFY2(kScreenARect.contains(b), "b is wholly on screen A");
            QVERIFY(a.topLeft() != b.topLeft());
            QVERIFY(rig.settleCapture());
            rig.manager->saveState();
            QCOMPARE(stored(store, QStringLiteral("a")).geometry, kOnB);
            QCOMPARE(stored(store, QStringLiteral("b")).geometry, kAlsoOnB);
            QCOMPARE(stored(store, QStringLiteral("a")).screen, std::optional<SavedScreen>(savedScreenOf(screenB())));
            QCOMPARE(stored(store, QStringLiteral("b")).screen, std::optional<SavedScreen>(savedScreenOf(screenB())));
            QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        }
        // The next start, with it plugged back in.
        AppSettings settings(path);
        settings.load();
        ContainerWorkspaceStore store(settings);
        QVERIFY(store.load().ok);
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QCOMPARE(rig.window(QStringLiteral("a")), kOnB);
        QCOMPARE(rig.window(QStringLiteral("b")), kAlsoOnB);
    }

    void screenRemovedAndAddedBackWhileRunning()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("running.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB())),
                                         floatingContainer(QStringLiteral("b"), kAlsoOnB, savedScreenOf(screenB())),
                                         floatingContainer(QStringLiteral("c"), QRect(300, 300, 400, 300),
                                                           savedScreenOf(screenA()))}), 0)
                     .status,
                 CommitStatus::Saved);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QCOMPARE(rig.window(QStringLiteral("a")), kOnB);
        QCOMPARE(rig.window(QStringLiteral("b")), kAlsoOnB);

        screens.list = {screenA()};
        rig.manager->screensChanged();
        const QRect a = rig.window(QStringLiteral("a"));
        const QRect b = rig.window(QStringLiteral("b"));
        qInfo() << "screen B removed, a:" << a << "b:" << b;
        QVERIFY2(kScreenARect.contains(a), "a is wholly on screen A");
        QVERIFY2(kScreenARect.contains(b), "b is wholly on screen A");
        QVERIFY(a.topLeft() != b.topLeft());
        // The window that was on screen A all along has not moved.
        QCOMPARE(rig.window(QStringLiteral("c")), QRect(300, 300, 400, 300));
        QVERIFY(rig.settleCapture());
        rig.manager->saveState();
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);

        screens.list = {screenA(), screenB()};
        rig.manager->screensChanged();
        QCOMPARE(rig.window(QStringLiteral("a")), kOnB);
        QCOMPARE(rig.window(QStringLiteral("b")), kAlsoOnB);
        QCOMPARE(rig.window(QStringLiteral("c")), QRect(300, 300, 400, 300));
        QVERIFY(rig.settleCapture());
        rig.manager->saveState();
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    void olderLayoutWithNoScreenIsPlacedByItsRect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("older.settings")));
        ContainerWorkspaceStore store(settings);
        const QRect nowhere(5000, 5000, 400, 300);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("on"), kOnB),
                                         floatingContainer(QStringLiteral("across"), kStraddling),
                                         floatingContainer(QStringLiteral("lost"), nowhere)}), 0)
                     .status,
                 CommitStatus::Saved);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QCOMPARE(rig.window(QStringLiteral("on")), kOnB);
        QCOMPARE(rig.window(QStringLiteral("across")), kStraddling);
        const QRect lost = rig.window(QStringLiteral("lost"));
        qInfo() << "on no screen, shown at:" << lost;
        QVERIFY2(kScreenARect.contains(lost), "shown wholly on the main window's screen");
        QVERIFY(rig.settleCapture());
        rig.manager->saveState();
        QCOMPARE(stored(store, QStringLiteral("lost")).geometry, nowhere);
        QVERIFY(!stored(store, QStringLiteral("lost")).screen.has_value());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    void operatorMoveStoresTheRectAndItsScreen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("move.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB()))}), 0)
                     .status,
                 CommitStatus::Saved);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QWidget* form = rig.manager->container(QStringLiteral("a"))->window();
        // What the end of an operator drag does.
        form->move(300, 300);
        rig.manager->rememberOperatorGeometry(QStringLiteral("a"));
        QTRY_COMPARE_WITH_TIMEOUT(stored(store, QStringLiteral("a")).geometry,
                                  QRect(QPoint(300, 300), kWindowSize), kSaveTimeoutMs);
        QCOMPARE(stored(store, QStringLiteral("a")).screen, std::optional<SavedScreen>(savedScreenOf(screenA())));
        QCOMPARE(rig.window(QStringLiteral("a")), QRect(QPoint(300, 300), kWindowSize));

        form->move(kAlsoOnB.topLeft());
        rig.manager->rememberOperatorGeometry(QStringLiteral("a"));
        QTRY_COMPARE_WITH_TIMEOUT(stored(store, QStringLiteral("a")).geometry, kAlsoOnB, kSaveTimeoutMs);
        QCOMPARE(stored(store, QStringLiteral("a")).screen, std::optional<SavedScreen>(savedScreenOf(screenB())));
    }

    // A window shown away from its place is the operator's again once they
    // move it: that move is saved like any other.
    void operatorMoveOfAWindowShownAwayFromItsPlaceIsSaved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("claimed.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB()))}), 0)
                     .status,
                 CommitStatus::Saved);
        FakeScreens screens;
        screens.list = {screenA()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        QVERIFY(kScreenARect.contains(rig.window(QStringLiteral("a"))));
        QWidget* form = rig.manager->container(QStringLiteral("a"))->window();
        form->move(640, 480);
        rig.manager->rememberOperatorGeometry(QStringLiteral("a"));
        QTRY_COMPARE_WITH_TIMEOUT(stored(store, QStringLiteral("a")).geometry,
                                  QRect(QPoint(640, 480), kWindowSize), kSaveTimeoutMs);
        QCOMPARE(stored(store, QStringLiteral("a")).screen, std::optional<SavedScreen>(savedScreenOf(screenA())));
        // Screen B coming back does not take it away again.
        screens.list = {screenA(), screenB()};
        rig.manager->screensChanged();
        QCOMPARE(rig.window(QStringLiteral("a")), QRect(QPoint(640, 480), kWindowSize));
    }

    // Finding from the remembered-place work: a window with no usable
    // saved rect gets a default place when shown, and that place is not
    // saved as if the operator had chosen it.
    void windowsWithNoSavedRectGetDifferentPlacesThatAreNotSaved()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("invalid.settings")));
        ContainerWorkspaceStore store(settings);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), QRect()),
                                         floatingContainer(QStringLiteral("b"), QRect())}), 0)
                     .status,
                 CommitStatus::Saved);
        const QByteArray before = ContainerDocumentCodec::encode(store.snapshot());
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        const QRect a = rig.window(QStringLiteral("a"));
        const QRect b = rig.window(QStringLiteral("b"));
        qInfo() << "no saved rect, a:" << a << "b:" << b;
        QVERIFY(kScreenARect.contains(a));
        QVERIFY(kScreenARect.contains(b));
        QVERIFY(a.topLeft() != b.topLeft());
        QCOMPARE(b.topLeft() - a.topLeft(), QPoint(ContainerManager::kCascadeOffsetPx, ContainerManager::kCascadeOffsetPx));
        QVERIFY(rig.settleCapture());
        rig.manager->saveState();
        QVERIFY(!stored(store, QStringLiteral("a")).geometry.isValid());
        QVERIFY(!stored(store, QStringLiteral("b")).geometry.isValid());
        QCOMPARE(ContainerDocumentCodec::encode(store.snapshot()), before);
    }

    void defaultWindowRectStepsPastATakenSpotAndStaysOnItsScreen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("default.settings")));
        ContainerWorkspaceStore store(settings);
        const QRect taken(140, 140, 360, 300);
        QCOMPARE(store.commit(workspace({floatingContainer(QStringLiteral("a"), taken),
                                         floatingContainer(QStringLiteral("b"), taken.translated(24, 24))}), 0)
                     .status,
                 CommitStatus::Saved);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        rig.manager->restoreState();
        QVERIFY(rig.show());
        // A free spot is taken as asked.
        QCOMPARE(rig.manager->defaultWindowRect(QRect(600, 500, 360, 300)), QRect(600, 500, 360, 300));
        // Two steps past the two windows already there.
        QCOMPARE(rig.manager->defaultWindowRect(taken), taken.translated(48, 48));
        // Kept on its screen: inside screen A's usable area, and screen B's.
        const QRect lowRight = rig.manager->defaultWindowRect(QRect(1700, 1000, 360, 300));
        QVERIFY2(screenA().available.contains(lowRight), "held inside screen A");
        const QRect onB = rig.manager->defaultWindowRect(QRect(3700, 900, 360, 300));
        QVERIFY2(kScreenBRect.contains(onB), "held inside screen B");
    }

    void twoPopOutShellsDoNotShareASpot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("shells.settings")));
        ContainerWorkspaceStore store(settings);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        ContainerDocument home = floatingContainer(QStringLiteral("home"), QRect(900, 500, 400, 300));
        home.contents = {rig.registry.makeEntry(QStringLiteral("TEXT")), rig.registry.makeEntry(QStringLiteral("TEXT")),
                         rig.registry.makeEntry(QStringLiteral("TEXT"))};
        QCOMPARE(rig.manager->commitWorkspace(workspace({home}), 0).status, CommitStatus::Saved);
        QVERIFY(rig.show());
        ContainerArrangeController* arrange = rig.manager->arrangeController();
        QVERIFY(arrange);
        QVERIFY(arrange->popOut(home.contents[0].id).ok);
        QVERIFY(arrange->popOut(home.contents[1].id).ok);
        QVector<ContainerDocument> shells;
        for (const auto& d : store.snapshot().containers) {
            if (d.popOutShell) {
                shells.append(d);
            }
        }
        QCOMPARE(shells.size(), 2);
        QCOMPARE(shells[0].geometry, QRect(140, 140, 360, 300));
        QCOMPARE(shells[1].geometry, QRect(164, 164, 360, 300));
        QCOMPARE(rig.window(shells[0].id).topLeft(), QPoint(140, 140));
        QCOMPARE(rig.window(shells[1].id).topLeft(), QPoint(164, 164));
    }

    void twoPlaceFreeCommandsDoNotShareASpot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("placefree.settings")));
        ContainerWorkspaceStore store(settings);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(
                     workspace({floatingContainer(QStringLiteral("a"), QRect(1100, 200, 400, 300)),
                                floatingContainer(QStringLiteral("b"), QRect(1100, 600, 400, 300))}), 0)
                     .status,
                 CommitStatus::Saved);
        rig.manager->placeContainerFree(QStringLiteral("a"));
        rig.manager->placeContainerFree(QStringLiteral("b"));
        const QRect a = rig.free(QStringLiteral("a"));
        const QRect b = rig.free(QStringLiteral("b"));
        qInfo() << "placed free, a:" << a << "b:" << b;
        QCOMPARE(stored(store, QStringLiteral("a")).dockMode, DockMode::OverlayDocked);
        QCOMPARE(stored(store, QStringLiteral("b")).dockMode, DockMode::OverlayDocked);
        QCOMPARE(a, QRect(300, 200, 400, 300));
        QCOMPARE(b, a.translated(24, 24));
        QCOMPARE(stored(store, QStringLiteral("a")).geometry, a);
        QCOMPARE(stored(store, QStringLiteral("b")).geometry, b);
    }

    void twoRecoversDoNotShareASpot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("recover.settings")));
        ContainerWorkspaceStore store(settings);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(
                     workspace({floatingContainer(QStringLiteral("a"), QRect(5000, 200, 400, 300)),
                                floatingContainer(QStringLiteral("b"), QRect(5000, 600, 400, 300))}), 0)
                     .status,
                 CommitStatus::Saved);
        rig.manager->recoverContainer(QStringLiteral("a"));
        rig.manager->recoverContainer(QStringLiteral("b"));
        const QRect a = rig.free(QStringLiteral("a"));
        const QRect b = rig.free(QStringLiteral("b"));
        qInfo() << "recovered, a:" << a << "b:" << b;
        QCOMPARE(a, QRect(300, 200, 400, 300));
        QCOMPARE(b, a.translated(24, 24));
        // Recovering the first again leaves it where it is: its own spot
        // is not one to step past.
        rig.manager->recoverContainer(QStringLiteral("a"));
        QCOMPARE(rig.free(QStringLiteral("a")), a);
    }

    void twoNewFloatingContainersDoNotShareASpot()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("new.settings")));
        ContainerWorkspaceStore store(settings);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        QVERIFY(rig.show());
        ContainerWidget* first = rig.manager->createContainer(1, DockMode::Floating);
        ContainerWidget* second = rig.manager->createContainer(1, DockMode::Floating);
        QVERIFY(first && second);
        const QRect a = first->window()->geometry();
        const QRect b = second->window()->geometry();
        qInfo() << "new floating, first:" << a << "second:" << b;
        QVERIFY(kScreenARect.contains(a));
        QVERIFY(kScreenARect.contains(b));
        QCOMPARE(b.topLeft() - a.topLeft(), QPoint(ContainerManager::kCascadeOffsetPx, ContainerManager::kCascadeOffsetPx));
    }

    // Finding from the remembered-place work: a window made free kept its
    // screen rect as its place in the pane.
    void floatingMadeFreeRemembersARectInsideThePane()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("tofree.settings")));
        ContainerWorkspaceStore store(settings);
        FakeScreens screens;
        screens.list = {screenA(), screenB()};
        Rig rig(store, screens);
        QVERIFY(rig.show());
        QCOMPARE(rig.manager->commitWorkspace(
                     workspace({floatingContainer(QStringLiteral("main"), QRect(300, 300, 400, 300)),
                                floatingContainer(QStringLiteral("a"), kOnB, savedScreenOf(screenB()))}), 0)
                     .status,
                 CommitStatus::Saved);
        QCOMPARE(rig.window(QStringLiteral("a")), kOnB);
        rig.manager->overlayDockContainer(QStringLiteral("a"));
        const ContainerDocument after = stored(store, QStringLiteral("a"));
        qInfo() << "made free, stored:" << after.geometry << "shown:" << rig.free(QStringLiteral("a"));
        QCOMPARE(after.dockMode, DockMode::OverlayDocked);
        QVERIFY2(rig.pane->rect().contains(after.geometry), "the remembered place is inside the pane");
        QCOMPARE(after.geometry.size(), kWindowSize);
        QCOMPARE(rig.free(QStringLiteral("a")), after.geometry);
        QVERIFY(!after.screen.has_value());
        // And it stays the remembered place.
        QVERIFY(rig.settleCapture());
        rig.manager->saveState();
        QCOMPARE(stored(store, QStringLiteral("a")).geometry, after.geometry);
    }
};

QTEST_MAIN(TstContainerWindowMonitors)
#include "tst_container_window_monitors.moc"
