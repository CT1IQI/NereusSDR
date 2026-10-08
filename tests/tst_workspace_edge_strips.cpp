// no-port-check: NereusSDR-original workspace preview four-edge strip regressions.
// Modification history (NereusSDR):
//   2026-10-07 - Strip placement, Move to Edge, the drop preview and the
//                 edge drag commit and cancel for the workspace preview by
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QTemporaryDir>
#include <memory>
#include "core/AppSettings.h"
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
// area around a plain centre pane. No radio, discovery or poller.
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
        middle = new QSplitter(Qt::Horizontal, &window);
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
        // Only the right strip and its handle sit beside the centre.
        QCOMPARE(f.center->height(), f.area->height());
        QCOMPARE(f.center->width() + f.area->strip(DockEdge::Right)->width()
                     + f.middle->handleWidth(), f.area->width());
        // Today's share: 256 of 1280.
        QVERIFY(qAbs(f.area->strip(DockEdge::Right)->width() - f.area->width() / 5) <= 2);
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
