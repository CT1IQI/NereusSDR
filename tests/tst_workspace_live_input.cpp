// no-port-check: NereusSDR-original workspace preview live-input regressions.
// Modification history (NereusSDR):
//   2026-10-08 - Real mouse input on the preview's Arrange headers, menus
//                 and drags, routed the way AppKit routes it through native
//                 child views, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//
// Why this test routes clicks itself: on macOS every widget under the main
// window is a native child view (the Metal meter and spectrum leaves promote
// their siblings, and those promote their ancestors; see nativeTopology()).
// AppKit hit-tests native views top-down and delivers a click to the topmost
// view under the cursor; it lets a click through a view only when that
// view's window carries Qt::WindowTransparentForInput. The widget attribute
// WA_TransparentForMouseEvents is honoured by Qt's alien routing only, inside
// one native view. The offscreen platform does no native hit-test: a click
// sent to the top-level window is routed by QWidget::childAt, which skips
// transparent widgets, so a plain QTest::mouseClick on the window or on the
// button never sees what JJ sees. nativeTarget() reproduces the AppKit rule
// over the real widget tree, and every event below goes, through
// QWindowSystemInterface, to the native window AppKit would pick.
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QDrag>
#include <QLabel>
#include <QLoggingCategory>
#include <QMenu>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QWindow>
#include <functional>
#include <memory>
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

constexpr int kMenuWaitMs = 2000;

// Known lines this test filters, so the log shows only what is new:
// - the offscreen platform has no GPU and does not move or stack native
//   child windows, and says so for every native view;
// - QTest notes a move outside the pressed window, which is what AppKit
//   delivers during a drag, by design here;
// - QMenu asks for the native container as its transient parent and Qt
//   refuses it ("... must be a top level window", JJ's log). The popup keeps
//   the main window as its transient parent; MenuProbe asserts that.
QtMessageHandler g_previousHandler = nullptr;
void filterPlatformNoise(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg
        && (message.startsWith(QLatin1String("This plugin does not support"))
            || message.startsWith(QLatin1String("QRhiWidget:"))
            || (message.startsWith(QLatin1String("Mouse event at"))
                && message.contains(QLatin1String("occurs outside target window")))
            || message.endsWith(QLatin1String("must be a top level window.")))) {
        return;
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

bool passesInput(const QWidget* widget)
{
    const QWindow* window = widget->windowHandle();
    return window != nullptr && window->flags().testFlag(Qt::WindowTransparentForInput);
}

// The native descendants whose nearest native ancestor is `native`, in paint
// order (the last one is on top), as AppKit holds them as subviews.
void nativeSubviews(QWidget* parent, QList<QWidget*>& out)
{
    for (QObject* object : parent->children()) {
        auto* widget = qobject_cast<QWidget*>(object);
        if (!widget || widget->isWindow()) {
            continue;
        }
        if (widget->internalWinId()) {
            out.append(widget);
        } else {
            nativeSubviews(widget, out);
        }
    }
}

bool insideClip(QWidget* widget, QWidget* native, const QPoint& global)
{
    // A native child view is clipped by its alien ancestors up to its
    // native superview.
    for (QWidget* w = widget; w; w = w->parentWidget()) {
        if (!w->rect().contains(w->mapFromGlobal(global))) {
            return false;
        }
        if (w == native) {
            break;
        }
    }
    return true;
}

// AppKit's hit-test: the topmost visible native view under the point, skipping
// views whose window is transparent for input.
QWidget* nativeTarget(QWidget* native, const QPoint& global)
{
    QList<QWidget*> subviews;
    nativeSubviews(native, subviews);
    for (auto it = subviews.crbegin(); it != subviews.crend(); ++it) {
        QWidget* candidate = *it;
        if (!candidate->isVisible() || passesInput(candidate) || !insideClip(candidate, native, global)) {
            continue;
        }
        return nativeTarget(candidate, global);
    }
    return native;
}

// A mouse gesture as AppKit delivers it: the press goes to the view the
// hit-test picks and the moves and the release follow the pressed view.
class NativeMouse {
public:
    explicit NativeMouse(QWidget* topLevel) : m_topLevel(topLevel) {}

    QWidget* targetAt(const QPoint& global) const { return nativeTarget(m_topLevel, global); }

    void press(const QPoint& global)
    {
        m_grab = targetAt(global)->windowHandle();
        QTest::mousePress(m_grab, Qt::LeftButton, Qt::NoModifier, m_grab->mapFromGlobal(global));
    }
    void move(const QPoint& global)
    {
        QTest::mouseMove(m_grab, m_grab->mapFromGlobal(global));
    }
    void release(const QPoint& global)
    {
        QTest::mouseRelease(m_grab, Qt::LeftButton, Qt::NoModifier, m_grab->mapFromGlobal(global));
        m_grab = nullptr;
    }
    void click(const QPoint& global)
    {
        press(global);
        release(global);
    }

private:
    QWidget* m_topLevel;
    QWindow* m_grab{nullptr};
};

// Qt's drop routing inside the native view AppKit picked (QWidgetWindow's
// findDnDTarget): the widget under the point, or the nearest ancestor that
// accepts drops, up to a window.
QWidget* dropReceiver(QWidget* nativeView, const QPoint& global)
{
    QWidget* widget = nativeView->childAt(nativeView->mapFromGlobal(global));
    if (!widget) {
        widget = nativeView;
    }
    while (widget && !widget->isWindow() && !widget->acceptDrops()) {
        widget = widget->parentWidget();
    }
    return widget && widget->acceptDrops() ? widget : nullptr;
}

// Watches where a pressed-button move goes. The host starts its QDrag from
// its filter on the entry header and consumes that move; a move that is not
// a drag start goes on to the header's parents. QDrag has no parent and
// sends nothing, so this is the observable start of the drag.
struct DragStartSpy : QObject {
    DragStartSpy(QWidget* header, QWidget* beyond) : m_header(header), m_beyond(beyond)
    {
        qApp->installEventFilter(this);
    }
    ~DragStartSpy() override { qApp->removeEventFilter(this); }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress && watched == m_header) {
            m_press = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
        }
        if (event->type() == QEvent::MouseMove
            && static_cast<QMouseEvent*>(event)->buttons().testFlag(Qt::LeftButton)) {
            const QPoint global = static_cast<QMouseEvent*>(event)->globalPosition().toPoint();
            if (watched == m_header
                && (global - m_press).manhattanLength() >= QApplication::startDragDistance()) {
                ++headerMovesPastDistance;
            } else if (watched == m_beyond) {
                ++propagated;
            }
        }
        return false;
    }
    int headerMovesPastDistance{0};
    int propagated{0};
private:
    QWidget* m_header;
    QWidget* m_beyond;
    QPoint m_press;
};

QPoint centreOf(const QWidget* widget)
{
    return widget->mapToGlobal(widget->rect().center());
}

// Reads the popup a click opened, from inside its exec() loop, and acts on
// it with real clicks. Records what it saw; closes the menu when `act` does
// not pick an item.
struct MenuProbe {
    bool opened{false};
    QStringList actions;
    QWindow* transientParent{nullptr};
    bool transientIsTopLevel{false};
    QHash<QString, bool> enabled;

    void arm(std::function<void(QMenu*)> act = {})
    {
        QTimer::singleShot(50, qApp, [this, act] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            opened = true;
            for (QAction* action : menu->actions()) {
                if (!action->isSeparator()) {
                    actions.append(action->text());
                    enabled.insert(action->text(), action->isEnabled());
                }
            }
            if (QWindow* window = menu->windowHandle()) {
                transientParent = window->transientParent();
                transientIsTopLevel = transientParent && transientParent->isTopLevel();
            }
            if (act) {
                act(menu);
            } else {
                menu->close();
            }
        });
    }
};

QAction* actionNamed(QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

// Clicks a menu item as the user does: a click on the item's rectangle.
void clickItem(QMenu* menu, const QString& text)
{
    QAction* action = actionNamed(menu, text);
    if (!action) {
        menu->close();
        return;
    }
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
}

// JJ's workspace (from his report, never from his settings): the main panel
// alone in the right strip, header Hidden, a vertical stack of 15 entries,
// strip thickness 398; a second container floating and hidden.
struct LiveFixture {
    std::unique_ptr<MainWindow> window;
    ContainerManager* manager{nullptr};
    ContainerWorkspaceStore* store{nullptr};
    ContainerContentRegistry* registry{nullptr};
    QString mainId;
    QString rxEntryId;
    QString displayEntryId;
    QString modMonitorEntryId;

    bool build()
    {
        window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window->resize(1512, 869);
        window->show();
        if (!QTest::qWaitForWindowExposed(window.get())) {
            return false;
        }
        manager = window->findChild<ContainerManager*>();
        if (!manager) {
            return false;
        }
        store = manager->workspaceStore();
        registry = manager->contentRegistry();
        WorkspaceDocument document = store->snapshot();
        ContainerDocument main;
        main.id = document.mainContainerId.isEmpty() ? QStringLiteral("main") : document.mainContainerId;
        main.name = QStringLiteral("Main Panel");
        main.layout = ContentLayout::VerticalStack;
        main.dockMode = DockMode::PanelDocked;
        main.edge = DockEdge::Right;
        main.header = HeaderMode::Hidden;
        const QStringList types{"meter.ananMulti", "applet:Display", "applet:mod_monitor", "applet:rx",
                                "applet:TX", "applet:RADE", "applet:vax", "applet:tci",
                                "applet:tci_clients", "applet:RfKit", "applet:amp", "applet:pure_signal",
                                "applet:PHCW", "applet:tuner", "applet:s_meter"};
        for (const QString& type : types) {
            ContentEntry entry = registry->makeEntry(type);
            entry.context["sliceId"] = 0;
            if (type == QStringLiteral("applet:rx")) {
                rxEntryId = entry.id;
            } else if (type == QStringLiteral("applet:Display")) {
                displayEntryId = entry.id;
            } else if (type == QStringLiteral("applet:mod_monitor")) {
                modMonitorEntryId = entry.id;
            }
            main.contents.append(entry);
        }
        ContainerDocument bars;
        bars.id = QStringLiteral("meter-bars");
        bars.name = QStringLiteral("Meter Bars");
        bars.layout = ContentLayout::VerticalStack;
        bars.dockMode = DockMode::Floating;
        bars.visible = false;
        bars.geometry = QRect(200, 200, 300, 400);
        for (int i = 0; i < 13; ++i) {
            ContentEntry entry = registry->makeEntry(QStringLiteral("meter.customBar"));
            entry.context["sliceId"] = 0;
            bars.contents.append(entry);
        }
        document.mainContainerId = main.id;
        document.containers = {main, bars};
        document.arrangement.strips[DockEdge::Right].thickness = 398;
        mainId = main.id;
        if (manager->commitWorkspace(document, document.revision).status != CommitStatus::Saved) {
            return false;
        }
        settle();
        return true;
    }

    bool enterArrange()
    {
        // Containers > Arrange Workspace, the menu bar item.
        auto* arrange = window->findChild<QAction*>(QStringLiteral("containersArrangeWorkspace"));
        if (!arrange) {
            return false;
        }
        arrange->trigger();
        settle();
        return manager->arrangeMode();
    }

    ContainerWidget* mainContainer() const { return manager->container(mainId); }
    WorkspaceGroupHeader* groupHeader() const { return mainContainer()->workspaceHeader(); }
    ContainerContentHost* host() const { return manager->contentHost(mainId); }

    WorkspaceEntryHeader* entryHeader(const QString& id) const
    {
        return host()->findChild<WorkspaceEntryHeader*>(QStringLiteral("workspaceEntryHeader_") + id);
    }

    // Scrolls an entry's header into the strip's view, as the user scrolls.
    void reveal(QWidget* widget)
    {
        if (auto* scroll = host()->findChild<QScrollArea*>()) {
            scroll->ensureWidgetVisible(widget, 0, 40);
        }
        settle();
    }

    const ContainerDocument* document(const QString& id) const
    {
        static ContainerDocument found;
        for (const auto& d : store->snapshot().containers) {
            if (d.id == id) {
                found = d;
                return &found;
            }
        }
        return nullptr;
    }

    int entryIndex(const QString& containerId, const QString& entryId) const
    {
        const ContainerDocument* d = document(containerId);
        if (!d) {
            return -1;
        }
        for (int i = 0; i < d->contents.size(); ++i) {
            if (d->contents[i].id == entryId) {
                return i;
            }
        }
        return -1;
    }

    QString describe(QWidget* widget) const
    {
        return QString::fromLatin1(widget->metaObject()->className()) + QLatin1Char('#') + widget->objectName();
    }
};

} // namespace

class TstWorkspaceLiveInput : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules("nereus.*.debug=false\nnereus.*.info=false\nnereussdr.*.info=false");
        g_previousHandler = qInstallMessageHandler(filterPlatformNoise);
        WorkspacePreview::setEnabledForTest(true);
        AppSettings::setProfileOverride(QStringLiteral("workspace-live-input-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes(5));
    }
    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
        RadioDiscovery::clearHoldOffForTest();
        WorkspacePreview::setEnabledForTest(false);
    }

    // The proxy for macOS: every visible native view that draws over others
    // and claims to be transparent for the mouse must say so to the window
    // system, or AppKit gives it every click under it.
    void nativeOverlaysLetClicksThrough()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        // The premise: the container is a native child view, as JJ's log shows
        // ("ContainerWidgetClassWindow must be a top level window").
        QVERIFY(f.mainContainer()->internalWinId() != 0);
        QStringList blocking;
        for (QWidget* widget : f.window->findChildren<QWidget*>()) {
            if (!widget->isWindow() && widget->internalWinId() && widget->isVisible()
                && widget->testAttribute(Qt::WA_TransparentForMouseEvents) && !passesInput(widget)) {
                blocking.append(f.describe(widget) + QStringLiteral(" in ")
                                + f.describe(widget->parentWidget()));
            }
        }
        QVERIFY2(blocking.isEmpty(), qPrintable(QStringLiteral("native views that swallow clicks: ") + blocking.join(", ")));
    }

    void groupHeaderOptionsOpensTheMenuInArrange()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        QVERIFY(header && header->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(header->optionsButton());
        QWidget* hit = mouse.targetAt(at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        MenuProbe probe;
        probe.arm();
        mouse.click(at);
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QVERIFY(probe.actions.contains(QStringLiteral("Move to Edge")));
        QVERIFY(probe.actions.contains(QStringLiteral("Pop Out")));
        // The popup's transient parent is the main window, a top-level.
        QVERIFY(probe.transientIsTopLevel);
    }

    void entryHeaderOptionsOpensTheMenuInArrange()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        QVERIFY(header->isVisible());
        NativeMouse mouse(f.window.get());
        const QPoint at = centreOf(header->optionsButton());
        QWidget* hit = mouse.targetAt(at);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the click lands on ") + f.describe(hit)));
        MenuProbe probe;
        probe.arm();
        mouse.click(at);
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QVERIFY(probe.actions.contains(QStringLiteral("Pop Out")));
        QVERIFY(probe.enabled.value(QStringLiteral("Pop Out")));
        QVERIFY(probe.transientIsTopLevel);
    }

    void headerDragMovesTheContainerToTheLeftEdge()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        NativeMouse mouse(f.window.get());
        const QPoint from = centreOf(grip);
        QWidget* hit = mouse.targetAt(from);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the press lands on ") + f.describe(hit)));
        EdgeDockArea* area = f.manager->edgeDockArea();
        const QPoint to = area->mapToGlobal(area->targetBandRect(DockEdge::Left).center());
        mouse.press(from);
        mouse.move(from + QPoint(-20, 0));
        mouse.move(to + QPoint(10, 0));
        mouse.move(to);
        mouse.release(to);
        settle();
        QCOMPARE(f.document(f.mainId)->edge, DockEdge::Left);
        QCOMPARE(f.document(f.mainId)->dockMode, DockMode::PanelDocked);
    }

    void entryHeaderDragReordersTheEntry()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        // Mod Monitor (third) above Display (second): both in view at once,
        // as the strip shows them.
        QCOMPARE(f.entryIndex(f.mainId, f.modMonitorEntryId), 2);
        WorkspaceEntryHeader* header = f.entryHeader(f.modMonitorEntryId);
        WorkspaceEntryHeader* display = f.entryHeader(f.displayEntryId);
        QVERIFY(header && display);
        f.reveal(display);
        QVERIFY(header->isVisible() && display->isVisible());
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        NativeMouse mouse(f.window.get());
        const QPoint from = centreOf(grip);
        QWidget* hit = mouse.targetAt(from);
        QVERIFY2(hit == header || header->isAncestorOf(hit),
                 qPrintable(QStringLiteral("the press lands on ") + f.describe(hit)));
        // The press and the move past the drag distance reach the header and
        // the host takes the move as its drag start. The offscreen platform's
        // drag returns at once (it has no drag-and-drop), so the drop half is
        // delivered below, to the view AppKit would give it.
        DragStartSpy started(header, f.host());
        mouse.press(from);
        mouse.move(from + QPoint(0, QApplication::startDragDistance() + 4));
        mouse.release(from + QPoint(0, QApplication::startDragDistance() + 4));
        QVERIFY(started.headerMovesPastDistance > 0);
        QCOMPARE(started.propagated, 0);
        // AppKit delivers the drop to the native view under the cursor. Just
        // below Display's header, above its centre: Mod Monitor goes first.
        const QPoint to = display->mapToGlobal(QPoint(display->width() / 2, display->height() + 4));
        QWidget* dropView = mouse.targetAt(to);
        QWidget* receiver = dropReceiver(dropView, to);
        QVERIFY2(receiver != nullptr,
                 qPrintable(QStringLiteral("no view accepts the drop under the cursor; it lands on ")
                            + f.describe(dropView)));
        QMimeData mime;
        mime.setData(ContainerArrangeController::kMimeType,
                     f.manager->arrangeController()->mimeData(f.modMonitorEntryId));
        const QPointF local = receiver->mapFromGlobal(QPointF(to));
        QDragEnterEvent enter(local.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &enter);
        QVERIFY(enter.isAccepted());
        QDragMoveEvent over(local.toPoint(), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &over);
        QVERIFY(over.isAccepted());
        QDropEvent drop(local, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(receiver, &drop);
        QVERIFY(drop.isAccepted());
        settle();
        QCOMPARE(f.entryIndex(f.mainId, f.modMonitorEntryId), 1);
        QCOMPARE(f.entryIndex(f.mainId, f.displayEntryId), 2);
    }

    void moveToEdgeFromTheMenu()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        NativeMouse mouse(f.window.get());
        MenuProbe probe;
        bool submenuOpened = false;
        probe.arm([&](QMenu* menu) {
            clickItem(menu, QStringLiteral("Move to Edge"));
            QTimer::singleShot(400, qApp, [&, menu] {
                auto* sub = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                if (!sub || sub == menu) {
                    menu->close();
                    return;
                }
                submenuOpened = true;
                clickItem(sub, QStringLiteral("Left"));
            });
        });
        mouse.click(centreOf(f.groupHeader()->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(submenuOpened, kMenuWaitMs);
        settle();
        QCOMPARE(f.document(f.mainId)->edge, DockEdge::Left);
    }

    void popOutAnEntryFromItsMenu()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceEntryHeader* header = f.entryHeader(f.rxEntryId);
        QVERIFY(header);
        f.reveal(header);
        NativeMouse mouse(f.window.get());
        MenuProbe probe;
        probe.arm([](QMenu* menu) { clickItem(menu, QStringLiteral("Pop Out")); });
        mouse.click(centreOf(header->optionsButton()));
        QTRY_VERIFY_WITH_TIMEOUT(probe.opened, kMenuWaitMs);
        settle();
        QCOMPARE(f.entryIndex(f.mainId, f.rxEntryId), -1);
        bool shell = false;
        for (const auto& d : f.store->snapshot().containers) {
            if (d.popOutShell && d.contents.size() == 1 && d.contents[0].id == f.rxEntryId) {
                shell = d.dockMode == DockMode::Floating;
            }
        }
        QVERIFY(shell);
        // Put RX back before the window goes: the offscreen platform's
        // backing store keeps the moved native view and faults on teardown.
        QVERIFY(f.manager->arrangeController()->returnEntry(f.rxEntryId).ok);
        settle();
    }

    // What made it look locked: under the cursor the header's open hand never
    // showed (the view on top has the arrow), and nothing answered. There is
    // no lock glyph and no disabled item: the container is not locked.
    void arrangeDoesNotLookLocked()
    {
        LiveFixture f;
        QVERIFY(f.build());
        QVERIFY(f.enterArrange());
        WorkspaceGroupHeader* header = f.groupHeader();
        auto* grip = header->findChild<QLabel*>(QStringLiteral("workspaceGrip"));
        QVERIFY(grip);
        auto* lock = header->findChild<WorkspaceLockGlyph*>();
        QVERIFY(lock);
        QVERIFY(!lock->isVisible());
        NativeMouse mouse(f.window.get());
        QWidget* view = mouse.targetAt(centreOf(grip));
        QWidget* under = view->childAt(view->mapFromGlobal(centreOf(grip)));
        QWidget* shown = under ? under : view;
        QVERIFY2(shown->cursor().shape() == Qt::OpenHandCursor,
                 qPrintable(QStringLiteral("cursor over the grip comes from ") + f.describe(shown)));
        WorkspaceEntryHeader* entry = f.entryHeader(f.rxEntryId);
        QVERIFY(entry);
        f.reveal(entry);
        QWidget* entryView = mouse.targetAt(centreOf(entry));
        QVERIFY2(entryView == entry || entry->isAncestorOf(entryView),
                 qPrintable(QStringLiteral("the entry header is under ") + f.describe(entryView)));
        QCOMPARE(entry->cursor().shape(), Qt::OpenHandCursor);
    }
};

QTEST_MAIN(TstWorkspaceLiveInput)
#include "tst_workspace_live_input.moc"
