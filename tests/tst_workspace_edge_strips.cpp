// no-port-check: NereusSDR-original workspace preview four-edge strip regressions.
// Modification history (NereusSDR):
//   2026-10-07 - Strip placement, Move to Edge, the drop preview and the
//                 edge drag commit and cancel for the workspace preview by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Strip minimum and default thickness, lock on strip handles,
//                 opaque free containers, pop-out size, and Reveal and Hidden
//                 inside strips, by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - The right strip and its handle take exactly today's Main
//                 Panel and splitter handle width; the fixture's middle
//                 splitter is the app's EdgeDockSplitter. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <memory>
#include "core/AppSettings.h"
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

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// Offline fixture: a main window whose central widget is the four-edge
// area around a plain centre pane, in an EdgeDockSplitter as in
// MainWindow. No radio, discovery or poller.
struct EdgeFixture {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath("edge-strips.xml")};
    ContainerWorkspaceStore store{settings};
    ContainerContentRegistry registry;
    QMainWindow window;
    QSplitter* middle{nullptr};
    QWidget* center{nullptr};
    EdgeDockArea* area{nullptr};
    std::unique_ptr<ContainerManager> manager;

    EdgeFixture()
    {
        middle = new EdgeDockSplitter(Qt::Horizontal, &window);
        center = new QWidget(middle);
        center->setMinimumWidth(400);
        middle->addWidget(center);
        area = new EdgeDockArea(middle, center, &window);
        window.setCentralWidget(area);
        window.resize(1200, 800);
        manager = std::make_unique<ContainerManager>(center, middle);
        manager->setEdgeDockArea(area);
        manager->setWorkspaceAdapter(&store, &registry);
    }

    ContainerDocument container(const QString& id, DockEdge edge)
    {
        ContainerDocument result;
        result.id = id;
        result.name = id;
        result.layout = ContentLayout::VerticalStack;
        result.dockMode = DockMode::PanelDocked;
        result.edge = edge;
        result.header = HeaderMode::Always;
        ContentEntry entry = registry.makeEntry(QStringLiteral("meter.signalMaxBin"));
        entry.id = id + QStringLiteral("-meter");
        entry.context["sliceId"] = 0;
        result.contents.append(entry);
        return result;
    }

    // A container of several stacked meters: taller than an end strip's
    // 112 px floor.
    ContainerDocument tallContainer(const QString& id, DockEdge edge, HeaderMode header)
    {
        ContainerDocument result = container(id, edge);
        result.header = header;
        for (const QString& type : {QStringLiteral("meter.comp"), QStringLiteral("meter.signalMaxBin")}) {
            ContentEntry entry = registry.makeEntry(type);
            entry.id = id + QStringLiteral("-") + type;
            entry.context["sliceId"] = 0;
            result.contents.append(entry);
        }
        return result;
    }

    bool commit(const QList<ContainerDocument>& containers)
    {
        WorkspaceDocument document = store.snapshot();
        document.containers = containers;
        document.mainContainerId = containers.first().id;
        const auto result = manager->commitWorkspace(document, document.revision);
        settle();
        return result.status == CommitStatus::Saved;
    }

    void show()
    {
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
    }

    QStringList stripIds(DockEdge edge) const
    {
        QStringList ids;
        for (QWidget* w : area->strip(edge)->containers()) {
            if (auto* c = qobject_cast<ContainerWidget*>(w)) {
                ids.append(c->id());
            }
        }
        return ids;
    }

    QStringList documentIds(DockEdge edge) const
    {
        QStringList ids;
        for (const auto& d : store.snapshot().containers) {
            if (d.dockMode == DockMode::PanelDocked && d.edge == edge) {
                ids.append(d.id);
            }
        }
        return ids;
    }

    const ContainerDocument* document(const QString& id) const
    {
        static ContainerDocument found;
        for (const auto& d : store.snapshot().containers) {
            if (d.id == id) {
                found = d;
                return &found;
            }
        }
        return nullptr;
    }
};

void press(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseButtonPress, local, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void move(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseMove, local, global, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void release(QWidget* target, const QPoint& global)
{
    const QPoint local = target->mapFromGlobal(global);
    QMouseEvent event(QEvent::MouseButtonRelease, local, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

QPoint areaGlobal(EdgeDockArea* area, const QPoint& local)
{
    return area->mapToGlobal(local);
}

} // namespace

class TstWorkspaceEdgeStrips : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void twoLeftContainersKeepOrderAndExtentsAcrossResize()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("b", DockEdge::Left),
                          f.container("main", DockEdge::Right)}));
        f.show();
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"a", "b"}));
        QCOMPARE(f.stripIds(DockEdge::Right), QStringList{"main"});
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        QVERIFY(left->isVisible());
        QCOMPARE(left->width(), EdgeDockStrip::kSideThicknessPx);

        left->setExtents({400, 200});
        settle();
        const QList<int> before = left->extents();
        QVERIFY(before[0] > before[1]);
        const double ratio = double(before[0]) / before[1];

        f.window.resize(1500, 1000);
        settle();
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"a", "b"}));
        // The strip keeps its thickness; the centre takes the new room.
        QCOMPARE(left->width(), EdgeDockStrip::kSideThicknessPx);
        const QList<int> after = left->extents();
        QVERIFY(after[0] + after[1] > before[0] + before[1]);
        QVERIFY2(qAbs(double(after[0]) / after[1] - ratio) < 0.05,
                 qPrintable(QStringLiteral("%1/%2 vs %3").arg(after[0]).arg(after[1]).arg(ratio)));

        // Saved extents restore the same order and proportions.
        f.manager->saveState();
        const auto saved = f.store.snapshot().arrangement.strips.value(DockEdge::Left);
        QCOMPARE(saved.extents.value("a"), after[0]);
        QCOMPARE(saved.extents.value("b"), after[1]);
        QVERIFY(!f.store.snapshot().extensions.contains("splitterSizes"));
    }

    void emptyStripTakesNoSpace()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        f.show();
        for (DockEdge edge : {DockEdge::Left, DockEdge::Top, DockEdge::Bottom}) {
            QVERIFY(f.area->strip(edge)->isHidden());
        }
        QCOMPARE(f.area->verticalSplitter()->sizes().at(0), 0);
        QCOMPARE(f.area->verticalSplitter()->sizes().at(2), 0);
        QCOMPARE(f.area->middleSplitter()->sizes().at(0), 0);
        // Only the right strip and its 3 px handle sit beside the centre.
        QCOMPARE(f.center->height(), f.area->height());
        QCOMPARE(f.center->width() + f.area->strip(DockEdge::Right)->width()
                     + EdgeDockStrip::kRightHandlePx, f.area->width());
        // Today's share: 256 of 1280.
        // Today's Main Panel width is the strip's width: the 3 px-handle
        // splitter's 1024/256 split with the panel raised to its minimum.
        // The right strip has no padding.
        f.window.resize(1512, 800);
        settle();
        constexpr int kPadding = 2 * EdgeDockStrip::kRightPaddingPx;
        const int minimum = qMax(f.manager->container("main")->minimumWidth(),
                                 f.manager->container("main")->minimumSizeHint().width());
        const int panel = qMax(256, minimum);
        const int share = int(std::lround((f.area->width() - 3) * double(panel) / (1024 + panel)));
        if (minimum == 260) {
            // The value the 1512 px preview profile shows today.
            QCOMPARE(share, 306);
        }
        QVERIFY2(qAbs(f.area->strip(DockEdge::Right)->width() - (share + kPadding)) <= 2,
                 qPrintable(QStringLiteral("%1 vs %2").arg(f.area->strip(DockEdge::Right)->width())
                                .arg(share + kPadding)));
        QVERIFY(qAbs(f.manager->container("main")->width() - share) <= 2);
    }

    // JJ's choice (2026-10-08): the right strip and its handle take
    // exactly the width today's Main Panel and splitter handle take, so
    // the spectrum and the Main Panel come out the same size as today and
    // the spectrum area is pixel-comparable with the baseline. "Today" is
    // the switch-off layout itself: a 3 px-handle QSplitter of spectrum
    // and a panel with the container's minimum, setSizes({1024, 256}).
    void rightStripTakesTodaysPanelWidthExactly()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* main = f.manager->container("main");
        EdgeDockStrip* right = f.area->strip(DockEdge::Right);
        const int minimum = qMax(main->minimumWidth(), main->minimumSizeHint().width());

        QWidget today;
        today.setAttribute(Qt::WA_DontShowOnScreen);
        auto* splitter = new QSplitter(Qt::Horizontal, &today);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(EdgeDockArea::kMigratedHandlePx);
        auto* spectrum = new QWidget(splitter);
        spectrum->setMinimumWidth(f.center->minimumWidth());
        auto* panel = new QWidget(splitter);
        panel->setMinimumWidth(minimum);
        splitter->addWidget(spectrum);
        splitter->addWidget(panel);
        splitter->setSizes({EdgeDockArea::kMigratedTotalShare - EdgeDockArea::kMigratedPanelShare,
                            EdgeDockArea::kMigratedPanelShare});
        today.show();

        // The window widths of the visual review profiles.
        for (int width : {1280, 1512, 1920}) {
            f.window.resize(width, 800);
            settle();
            today.resize(f.area->width(), 800);
            splitter->setGeometry(today.rect());
            splitter->setSizes({EdgeDockArea::kMigratedTotalShare - EdgeDockArea::kMigratedPanelShare,
                                EdgeDockArea::kMigratedPanelShare});
            settle();
            const QString at = QStringLiteral("at %1 px").arg(width);
            // No padding: the Main Panel's content is the whole strip.
            QCOMPARE(main->width(), right->width());
            QVERIFY2(main->width() == panel->width(),
                     qPrintable(QStringLiteral("panel %1 vs today's %2 %3")
                                    .arg(main->width()).arg(panel->width()).arg(at)));
            // The handle on the spectrum side is today's 3 px.
            QSplitterHandle* handle = f.middle->handle(f.middle->indexOf(right));
            QVERIFY(handle != nullptr && handle->isVisible());
            QCOMPARE(handle->width(), EdgeDockArea::kMigratedHandlePx);
            QVERIFY2(f.center->width() == spectrum->width(),
                     qPrintable(QStringLiteral("spectrum %1 vs today's %2 %3")
                                    .arg(f.center->width()).arg(spectrum->width()).arg(at)));
            QCOMPARE(right->mapTo(f.area, QPoint()).x(), panel->x());
        }
        // The value the 1512 px preview profile shows today (Qt 6.11): the
        // spectrum 1203 px, the handle 3, the Main Panel 306 from x 1206.
        if (minimum == 260) {
            f.window.resize(1512, 800);
            settle();
            QCOMPARE(f.area->width(), 1512);
            QCOMPARE(f.center->width(), 1203);
            QCOMPARE(right->mapTo(f.area, QPoint()).x(), 1206);
            QCOMPARE(main->width(), 306);
        }
        // A dragged thickness still wins.
        f.area->setThickness(DockEdge::Right, 400);
        settle();
        QCOMPARE(right->width(), 400);
        QCOMPARE(main->width(), 400);
        QCOMPARE(f.center->width(), f.area->width() - 400 - EdgeDockStrip::kRightHandlePx);
    }

    void sideStripNeverClipsItsWidestContainer()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* a = f.manager->container("a");
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        int widest = qMax(a->minimumWidth(), a->minimumSizeHint().width());
        if (auto* scroll = a->contentHolder()->findChild<QScrollArea*>(); scroll && scroll->widget()) {
            widest = qMax(widest, scroll->widget()->minimumSizeHint().width());
        }
        constexpr int kPadding = 2 * EdgeDockStrip::kPaddingPx;
        QCOMPARE(left->minimumWidth(), widest + kPadding);
        // A thickness below the minimum (saved, or dragged) never clips it.
        f.area->setThickness(DockEdge::Left, 100);
        settle();
        QVERIFY2(left->width() >= widest + kPadding, qPrintable(QString::number(left->width())));
        QVERIFY(a->width() >= widest);
        QCOMPARE(f.area->thickness(DockEdge::Left), widest + kPadding);
        // Dragging the area handle cannot go below it either.
        QSplitterHandle* handle = f.area->middleSplitter()->handle(1);
        const QPoint grab = handle->mapToGlobal(handle->rect().center());
        press(handle, grab);
        move(handle, grab - QPoint(200, 0));
        release(handle, grab - QPoint(200, 0));
        settle();
        QVERIFY2(left->width() >= widest + kPadding, qPrintable(QString::number(left->width())));
    }

    void endStripDefaultShowsTallestContainerWhole()
    {
        constexpr int kPadding = 2 * EdgeDockStrip::kPaddingPx;
        for (HeaderMode header : {HeaderMode::Always, HeaderMode::Reveal, HeaderMode::Hidden}) {
            EdgeFixture f;
            QVERIFY(f.commit({f.tallContainer("top", DockEdge::Top, header),
                              f.container("main", DockEdge::Right)}));
            f.show();
            ContainerWidget* top = f.manager->container("top");
            const int content = f.manager->contentHost("top")->preferredContentHeight();
            const int headerExtent = top->headerExtent();
            // Reveal and Hidden reserve no header room.
            QCOMPARE(headerExtent, header == HeaderMode::Always ? WorkspaceGroupHeader::kHeight : 0);
            QVERIFY2(content + headerExtent + kPadding > EdgeDockStrip::kEndThicknessPx,
                     qPrintable(QString::number(content)));
            EdgeDockStrip* strip = f.area->strip(DockEdge::Top);
            QCOMPARE(strip->height(), content + headerExtent + kPadding);
            QVERIFY(top->contentHolder()->height() >= content);
            // Made smaller by the user, the content scrolls instead of clipping.
            const int smaller = content + headerExtent + kPadding - 30;
            f.area->setThickness(DockEdge::Top, smaller);
            settle();
            QCOMPARE(strip->height(), smaller);
            auto* scroll = top->contentHolder()->findChild<QScrollArea*>();
            QVERIFY(scroll != nullptr);
            QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        }
        // 112 px stays the least a short container starts at.
        EdgeFixture f;
        QVERIFY(f.commit({f.container("top", DockEdge::Top), f.container("main", DockEdge::Right)}));
        f.show();
        const int content = f.manager->contentHost("top")->preferredContentHeight()
            + f.manager->container("top")->headerExtent() + kPadding;
        QCOMPARE(f.area->strip(DockEdge::Top)->height(),
                 qMax(int(EdgeDockStrip::kEndThicknessPx), content));
    }

    void lockedContainerBlocksStripHandles()
    {
        EdgeFixture f;
        ContainerDocument a = f.container("a", DockEdge::Left);
        a.locked = true;
        QVERIFY(f.commit({a, f.container("b", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        EdgeDockStrip* left = f.area->strip(DockEdge::Left);
        const auto drag = [&](QSplitterHandle* handle, const QPoint& by) {
            const QPoint grab = handle->mapToGlobal(handle->rect().center());
            press(handle, grab);
            move(handle, grab + by);
            release(handle, grab + by);
            settle();
        };
        const QList<int> before = left->extents();
        drag(left->splitter()->handle(1), QPoint(0, 80));
        QCOMPARE(left->extents(), before);
        // The area handle beside the strip does not resize it either.
        const int width = left->width();
        drag(f.area->middleSplitter()->handle(1), QPoint(80, 0));
        QCOMPARE(left->width(), width);

        // Unlocked, the same drags resize.
        WorkspaceDocument document = f.store.snapshot();
        for (auto& d : document.containers) {
            d.locked = false;
        }
        QCOMPARE(f.manager->commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        settle();
        drag(left->splitter()->handle(1), QPoint(0, 80));
        QVERIFY(left->extents() != before);
        drag(f.area->middleSplitter()->handle(1), QPoint(80, 0));
        QVERIFY(left->width() != width);
    }

    void freeContainerPaintsItsWholeRect()
    {
        EdgeFixture f;
        f.center->setAutoFillBackground(true);
        QPalette red = f.center->palette();
        red.setColor(QPalette::Window, Qt::red);
        f.center->setPalette(red);
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        f.manager->placeContainerFree("a");
        settle();
        // Taller than its content, so part of it holds no content at all.
        WorkspaceDocument document = f.store.snapshot();
        for (auto& d : document.containers) {
            if (d.id == "a") {
                d.autoHeight = false;
                d.geometry = QRect(40, 40, 300, 420);
            }
        }
        QCOMPARE(f.manager->commitWorkspace(document, document.revision).status, CommitStatus::Saved);
        settle();
        ContainerWidget* a = f.manager->container("a");
        QCOMPARE(a->parentWidget(), f.center);
        QCOMPARE(a->geometry(), QRect(40, 40, 300, 420));
        const QImage image = f.window.grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        const QRect r = a->geometry().translated(f.center->mapTo(&f.window, QPoint()));
        // Its corners, outside the plate's radius, and inside it: its
        // edges, its centre, its foot.
        constexpr int kInset = 12;
        for (const QPoint& p : {QPoint(r.left() + 1, r.bottom() - 1), QPoint(r.right() - 1, r.bottom() - 1),
                                QPoint(r.left() + kInset, r.bottom() - kInset),
                                QPoint(r.right() - kInset, r.bottom() - kInset),
                                QPoint(r.center().x(), r.bottom() - kInset),
                                QPoint(r.center().x(), r.center().y() + 100),
                                QPoint(r.left() + kInset, r.center().y())}) {
            const QColor c = image.pixelColor(QPoint(int(p.x() * dpr), int(p.y() * dpr)));
            QVERIFY2(c.red() < 128, qPrintable(QStringLiteral("%1,%2 %3")
                                                   .arg(p.x()).arg(p.y()).arg(c.name())));
        }
    }

    void popOutFromAStripKeepsTheContainerSize()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("main", DockEdge::Right)}));
        f.show();
        ContainerWidget* a = f.manager->container("a");
        const int width = a->width();
        const int preferred = f.manager->contentHost("a")->preferredContentHeight()
            + WorkspaceGroupHeader::kHeight;
        f.manager->floatContainer("a");
        settle();
        const ContainerDocument* d = f.document("a");
        QCOMPARE(d->dockMode, DockMode::Floating);
        QCOMPARE(d->geometry.width(), width);
        QVERIFY2(d->geometry.height() <= qMax(120, preferred),
                 qPrintable(QString::number(d->geometry.height())));
        QVERIFY(a->window() != &f.window);
        QCOMPARE(a->window()->size(), d->geometry.size());
        // Docked back before teardown: see the Task 4 report on the Qt
        // offscreen plugin's clearHash.
        f.manager->moveContainerToEdge("a", DockEdge::Left);
        settle();
        QCOMPARE(f.document("a")->dockMode, DockMode::PanelDocked);
    }

    void revealAndHiddenRecoverInsideStrips()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.tallContainer("r", DockEdge::Top, HeaderMode::Reveal),
                          f.tallContainer("h", DockEdge::Top, HeaderMode::Hidden),
                          f.tallContainer("arr", DockEdge::Top, HeaderMode::Hidden),
                          f.container("main", DockEdge::Right)}));
        f.show();
        const int thickness = f.area->strip(DockEdge::Top)->height();
        ContainerWidget* r = f.manager->container("r");
        ContainerWidget* h = f.manager->container("h");
        ContainerWidget* arr = f.manager->container("arr");
        for (ContainerWidget* c : {r, h, arr}) {
            QCOMPARE(c->headerExtent(), 0);
            QVERIFY(!c->chromeVisible());
        }
        const QPoint contentTop = r->contentHolder()->mapTo(r, QPoint());

        // Reveal: 120 ms in the top band shows the header over the content.
        const auto hover = [](QWidget* target, const QPoint& position) {
            QMouseEvent event(QEvent::MouseMove, position, target->mapToGlobal(position),
                              Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };
        hover(r, QPoint(40, 6));
        QTRY_VERIFY_WITH_TIMEOUT(r->chromeVisible(), 1000);
        QCOMPARE(r->contentHolder()->mapTo(r, QPoint()), contentTop);
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);
        hover(r, QPoint(40, r->height() - 6));
        QTRY_VERIFY_WITH_TIMEOUT(!r->chromeVisible(), 2000);

        // Hidden: the context menu's Reveal controls recovers it.
        h->recoverChrome();
        settle();
        QVERIFY(h->chromeVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);

        // Hidden: Arrange recovers it, and turning Arrange off hides it.
        f.manager->setArrangeMode(true);
        settle();
        QVERIFY(arr->chromeVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);
        f.manager->setArrangeMode(false);
        settle();
        QVERIFY(!arr->chromeVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), thickness);
    }

    void moveToEdgeReordersAndCommits()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("a", DockEdge::Left), f.container("b", DockEdge::Left),
                          f.container("main", DockEdge::Right)}));
        f.show();
        f.manager->moveContainerToEdge("b", DockEdge::Left, 0);
        settle();
        QCOMPARE(f.documentIds(DockEdge::Left), (QStringList{"b", "a"}));
        QCOMPARE(f.stripIds(DockEdge::Left), (QStringList{"b", "a"}));
        f.manager->moveContainerToEdge("a", DockEdge::Top);
        settle();
        QCOMPARE(f.stripIds(DockEdge::Top), QStringList{"a"});
        QVERIFY(f.area->strip(DockEdge::Top)->isVisible());
        QCOMPARE(f.area->strip(DockEdge::Top)->height(), EdgeDockStrip::kEndThicknessPx);
        QCOMPARE(f.document("a")->edge, DockEdge::Top);
        // Place Free: over the centre pane, out of every strip.
        f.manager->placeContainerFree("b");
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::OverlayDocked);
        QCOMPARE(f.manager->container("b")->parentWidget(), f.center);
        QVERIFY(f.area->strip(DockEdge::Left)->isHidden());
        // A locked container stays where it is.
        WorkspaceDocument locked = f.store.snapshot();
        for (auto& d : locked.containers) {
            d.locked = d.id == "main";
        }
        QCOMPARE(f.manager->commitWorkspace(locked, locked.revision).status, CommitStatus::Saved);
        settle();
        f.manager->moveContainerToEdge("main", DockEdge::Left);
        settle();
        QCOMPARE(f.document("main")->edge, DockEdge::Right);
    }

    void dropPreviewSeamShowsBetweenContainers()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        f.manager->showDropPreviewForTest(DockEdge::Right, 1);
        settle();
        QVERIFY(f.area->targetBandsVisible());
        QVERIFY(f.area->dropPreviewVisible());
        const QRect preview = f.area->dropPreviewRect();
        const QRect first(f.manager->container("main")->mapTo(f.area, QPoint()), f.manager->container("main")->size());
        const QRect second(f.manager->container("b")->mapTo(f.area, QPoint()), f.manager->container("b")->size());
        QCOMPARE(preview.height(), EdgeDockStrip::kInsertPreviewPx);
        QVERIFY(preview.center().y() > first.top() && preview.center().y() < second.bottom());
        QVERIFY(qAbs(preview.center().y() - (first.bottom() + second.top()) / 2) <= 2);
        QVERIFY(f.area->strip(DockEdge::Right)->geometry().contains(preview.center()));

        // An empty edge previews the strip it would open, inside its padding.
        f.manager->showDropPreviewForTest(DockEdge::Left, 0);
        settle();
        QCOMPARE(f.area->dropPreviewRect().left(), EdgeDockStrip::kPaddingPx);
        QCOMPARE(f.area->dropPreviewRect().width(), EdgeDockStrip::kSideThicknessPx - 2 * EdgeDockStrip::kPaddingPx);
        QCOMPARE(f.area->targetBandRect(DockEdge::Left), QRect(0, 0, EdgeDockArea::kTargetBandPx, f.area->height()));

        f.manager->clearDropPreviewForTest();
        QVERIFY(!f.area->dropPreviewVisible());
        QVERIFY(!f.area->targetBandsVisible());
        // Arrange shows the bands without a preview.
        f.manager->setArrangeMode(true);
        QVERIFY(f.area->targetBandsVisible());
        QVERIFY(!f.area->dropPreviewVisible());
        f.manager->setArrangeMode(false);
        QVERIFY(!f.area->targetBandsVisible());
    }

    void dropTargetsResolveEdgesStripsAndDetach()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        const QRect r = f.area->rect();
        auto target = f.area->dropTargetAt(areaGlobal(f.area, r.center() - QPoint(150, 0)));
        QVERIFY(!target.valid);
        QVERIFY(!target.detach);
        target = f.area->dropTargetAt(areaGlobal(f.area, QPoint(10, r.center().y())));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Left);
        QCOMPARE(target.index, 0);
        target = f.area->dropTargetAt(areaGlobal(f.area, QPoint(r.center().x() - 150, r.height() - 10)));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Bottom);
        const QRect firstRight(f.manager->container("main")->mapTo(f.area, QPoint()), f.manager->container("main")->size());
        target = f.area->dropTargetAt(areaGlobal(f.area, firstRight.center() + QPoint(0, firstRight.height() / 2 - 2)));
        QVERIFY(target.valid);
        QCOMPARE(target.edge, DockEdge::Right);
        QCOMPARE(target.index, 1);
        target = f.area->dropTargetAt(f.window.mapToGlobal(QPoint(f.window.width() + 200, 40)));
        QVERIFY(!target.valid);
        QVERIFY(target.detach);
    }

    void headerDragCommitsOnlyOnAValidRelease()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right), f.container("b", DockEdge::Right)}));
        f.show();
        ContainerWidget* b = f.manager->container("b");
        QWidget* header = b->workspaceHeader();
        QVERIFY(header);
        const QPoint grab = header->mapToGlobal(QPoint(header->width() / 2, header->height() / 2));
        const QPoint nearTop = areaGlobal(f.area, QPoint(f.area->width() / 3, 12));
        const QPoint middleOfCenter = areaGlobal(f.area, QPoint(f.area->width() / 3, f.area->height() / 2));
        const quint64 revision = f.store.snapshot().revision;

        // Escape cancels: no commit, preview gone.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, nearTop);
        QVERIFY(f.area->dropPreviewVisible());
        QVERIFY(f.area->targetBandsVisible());
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(b, &escape);
        release(header, nearTop);
        settle();
        QVERIFY(!f.area->dropPreviewVisible());
        QVERIFY(!f.area->targetBandsVisible());
        QCOMPARE(f.store.snapshot().revision, revision);
        QCOMPARE(f.document("b")->edge, DockEdge::Right);

        // A release with no valid destination restores.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, middleOfCenter);
        QVERIFY(!f.area->dropPreviewVisible());
        release(header, middleOfCenter);
        settle();
        QCOMPARE(f.store.snapshot().revision, revision);
        QCOMPARE(f.stripIds(DockEdge::Right), (QStringList{"main", "b"}));

        // A release on the preview commits.
        press(header, grab);
        move(header, grab + QPoint(0, 40));
        move(header, nearTop);
        QVERIFY(f.area->dropPreviewVisible());
        release(header, nearTop);
        settle();
        QVERIFY(!f.area->dropPreviewVisible());
        QCOMPARE(f.document("b")->edge, DockEdge::Top);
        QCOMPARE(f.stripIds(DockEdge::Top), QStringList{"b"});

        // Pulled out of the window: a floating shell.
        header = f.manager->container("b")->workspaceHeader();
        const QPoint grabTop = header->mapToGlobal(QPoint(header->width() / 2, header->height() / 2));
        const QPoint outside = f.window.mapToGlobal(QPoint(f.window.width() + 300, 100));
        press(header, grabTop);
        move(header, grabTop + QPoint(40, 0));
        move(header, outside);
        release(header, outside);
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::Floating);
        QVERIFY(f.area->strip(DockEdge::Top)->isHidden());
        // Move to Edge brings it back to a strip.
        f.manager->moveContainerToEdge("b", DockEdge::Left);
        settle();
        QCOMPARE(f.document("b")->dockMode, DockMode::PanelDocked);
        QCOMPARE(f.stripIds(DockEdge::Left), QStringList{"b"});
    }

    void gridAlignmentIsSavedAndSnaps()
    {
        EdgeFixture f;
        QVERIFY(f.commit({f.container("main", DockEdge::Right)}));
        QVERIFY(!f.manager->gridAlignment());
        QSignalSpy changed(f.manager.get(), &ContainerManager::gridAlignmentChanged);
        f.manager->setGridAlignment(true);
        QCOMPARE(changed.count(), 1);
        QVERIFY(f.store.snapshot().arrangement.gridAlignment);
        QCOMPARE(f.manager->container("main")->gridAlignment(), kWorkspaceSnapGridPx);
        QCOMPARE(workspaceSnappedTopLeft(QPoint(13, 21)), QPoint(16, 24));
        QCOMPARE(workspaceSnappedTopLeft(QPoint(11, 3)), QPoint(8, 0));
        QCOMPARE(workspaceSnappedTopLeft(QPoint(13, 21), 1), QPoint(13, 21));
    }

    void switchOffKeepsTheSplitter()
    {
        WorkspacePreview::setEnabledForTest(false);
        {
            EdgeFixture f;
            QVERIFY(f.commit({f.container("main", DockEdge::Left)}));
            QCOMPARE(f.manager->container("main")->parentWidget(), f.middle);
            QCOMPARE(f.stripIds(DockEdge::Left), QStringList{});
        }
        WorkspacePreview::setEnabledForTest(true);
    }
};

QTEST_MAIN(TstWorkspaceEdgeStrips)
#include "tst_workspace_edge_strips.moc"
