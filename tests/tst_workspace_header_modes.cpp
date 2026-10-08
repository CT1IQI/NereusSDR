// no-port-check: NereusSDR-original workspace preview header mode regressions.
// Modification history (NereusSDR):
//   2026-10-07 - Reveal, Arrange and header button checks for the workspace
//                 preview by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>
#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

// Offline fixture: a store, a registry and a manager over a plain dock
// widget. No MainWindow, discovery, radio model or poller.
struct WorkspaceFixture {
    QTemporaryDir directory;
    AppSettings settings{directory.filePath("header-modes.xml")};
    ContainerWorkspaceStore store{settings};
    QWidget dock;
    QSplitter splitter;
    ContainerContentRegistry registry;
    ContainerManager manager{&dock, &splitter};

    WorkspaceFixture()
    {
        dock.resize(1200, 900);
        manager.setWorkspaceAdapter(&store, &registry);
    }
    ContainerDocument container(const QString& id, HeaderMode mode, int x)
    {
        ContainerDocument result;
        result.id = id;
        result.name = id;
        result.layout = ContentLayout::VerticalStack;
        result.dockMode = DockMode::OverlayDocked;
        result.header = mode;
        result.geometry = QRect(x, 20, 360, 420);
        for (const QString& variant : {QStringLiteral("signalMaxBin"), QStringLiteral("comp")}) {
            ContentEntry entry = registry.makeEntry("meter." + variant);
            entry.id = id + "-" + variant;
            entry.context["sliceId"] = 0;
            result.contents.append(entry);
        }
        return result;
    }
};

void settle()
{
    for (int pass = 0; pass < 6; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

void hover(QWidget* target, const QPoint& position)
{
    QMouseEvent move(QEvent::MouseMove, position, target->mapToGlobal(position), Qt::NoButton,
                     Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(target, &move);
}

} // namespace

class TstWorkspaceHeaderModes : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { WorkspacePreview::setEnabledForTest(true); }
    void cleanupTestCase() { WorkspacePreview::setEnabledForTest(false); }

    void revealLeavesContentGeometryUnchanged()
    {
        QWidget parent;
        parent.resize(800, 600);
        auto* c = new ContainerWidget(&parent);
        QPushButton content("An ordinary control");
        c->setContent(&content);
        c->setDockMode(DockMode::OverlayDocked);
        c->setHeaderMode(HeaderMode::Always);
        c->setGeometry(20, 20, 420, 260);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        settle();
        QVERIFY(c->workspaceHeader());
        QCOMPARE(c->workspaceHeader()->height(), WorkspaceGroupHeader::kHeight);
        QCOMPARE(c->headerExtent(), WorkspaceGroupHeader::kHeight);
        const int alwaysTop = c->contentHolder()->mapTo(c, QPoint()).y();

        c->setHeaderMode(HeaderMode::Reveal);
        settle();
        QCOMPARE(c->headerExtent(), 0);
        QVERIFY(!c->chromeVisible());
        const QPoint idle = content.mapTo(c, QPoint());
        const QSize idleSize = content.size();
        // Zero extent: the content area starts the header's height higher,
        // at the container's top edge.
        QCOMPARE(c->contentHolder()->mapTo(c, QPoint()).y(), alwaysTop - WorkspaceGroupHeader::kHeight);
        QCOMPARE(c->contentHolder()->mapTo(c, QPoint()).y(), 0);

        // Hover elsewhere never reveals it.
        hover(c, QPoint(40, 120));
        QTest::qWait(ContainerWidget::kRevealDelayMs * 2);
        QVERIFY(!c->chromeVisible());

        // 120 ms inside the top band reveals it, over the content.
        hover(c, QPoint(40, 6));
        QVERIFY(!c->chromeVisible());
        QTRY_VERIFY_WITH_TIMEOUT(c->chromeVisible(), 1000);
        settle();
        QCOMPARE(content.mapTo(c, QPoint()), idle);
        QCOMPARE(content.size(), idleSize);
        QCOMPARE(c->workspaceHeader()->geometry(), QRect(0, 0, c->width(), WorkspaceGroupHeader::kHeight));

        // Leaving the band hides it after the hide delay.
        hover(c, QPoint(40, 120));
        QVERIFY(c->chromeVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!c->chromeVisible(), 2000);
        QCOMPARE(content.mapTo(c, QPoint()), idle);

        // Keyboard focus reveals it at once; a mouse focus does not.
        QApplication::setActiveWindow(&parent);
        c->setFocus(Qt::MouseFocusReason);
        settle();
        QVERIFY(!c->chromeVisible());
        content.setFocus(Qt::OtherFocusReason);
        c->setFocus(Qt::TabFocusReason);
        QTRY_VERIFY_WITH_TIMEOUT(c->chromeVisible(), 1000);
        QCOMPARE(content.mapTo(c, QPoint()), idle);
        content.setFocus(Qt::OtherFocusReason);
        hover(c, QPoint(40, 120));
        QTRY_VERIFY_WITH_TIMEOUT(!c->chromeVisible(), 2000);

        // The preview seam reveals it too, without moving content.
        c->setHeaderRevealedForPreview(true);
        settle();
        QVERIFY(c->chromeVisible());
        QCOMPARE(content.mapTo(c, QPoint()), idle);
        QCOMPARE(content.size(), idleSize);
        content.setParent(nullptr);
    }

    void arrangeOnThenOffRestoresEachMode()
    {
        WorkspaceFixture fixture;
        WorkspaceDocument document;
        document.containers.append(fixture.container("always", HeaderMode::Always, 10));
        document.containers.append(fixture.container("reveal", HeaderMode::Reveal, 400));
        document.containers.append(fixture.container("hidden", HeaderMode::Hidden, 790));
        document.mainContainerId = "always";
        const auto committed = fixture.manager.commitWorkspace(document, 0);
        QVERIFY2(committed.status == CommitStatus::Saved, qPrintable(committed.error));
        fixture.dock.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fixture.dock));
        settle();

        const QStringList ids{"always", "reveal", "hidden"};
        QHash<QString, bool> header;
        QHash<QString, bool> entries;
        QHash<QString, QPoint> contentTop;
        const auto entryHeaderShown = [&](const QString& id) {
            auto* entryHeader = fixture.manager.contentHost(id)->entryHeader(id + "-comp");
            return entryHeader && entryHeader->isVisible();
        };
        for (const QString& id : ids) {
            auto* c = fixture.manager.container(id);
            QVERIFY(c);
            header[id] = c->chromeVisible();
            entries[id] = entryHeaderShown(id);
            contentTop[id] = fixture.manager.contentHost(id)->mapTo(c, QPoint());
            QVERIFY(!c->arrangeActive());
        }
        QVERIFY(header["always"]);
        QVERIFY(!header["reveal"]);
        QVERIFY(!header["hidden"]);
        QVERIFY(entries["always"]);
        QVERIFY(!entries["reveal"]);
        QVERIFY(!entries["hidden"]);

        QSignalSpy changed(&fixture.manager, &ContainerManager::arrangeModeChanged);
        fixture.manager.setArrangeMode(true);
        settle();
        QCOMPARE(changed.count(), 1);
        for (const QString& id : ids) {
            auto* c = fixture.manager.container(id);
            QVERIFY2(c->chromeVisible(), qPrintable(id));
            QVERIFY2(c->workspaceHeader()->arrangeAccent(), qPrintable(id));
            QVERIFY2(entryHeaderShown(id), qPrintable(id));
            // Arrange never reflows content.
            QCOMPARE(fixture.manager.contentHost(id)->mapTo(c, QPoint()), contentTop[id]);
            QVERIFY(c->findChild<QWidget*>("containerResizeGrip")->isVisible());
        }

        fixture.manager.setArrangeMode(false);
        settle();
        QCOMPARE(changed.count(), 2);
        for (const QString& id : ids) {
            auto* c = fixture.manager.container(id);
            QCOMPARE(c->chromeVisible(), header[id]);
            QCOMPARE(entryHeaderShown(id), entries[id]);
            QVERIFY(!c->workspaceHeader()->arrangeAccent());
            QVERIFY(!c->findChild<QWidget*>("containerResizeGrip")->isVisible());
            QCOMPARE(fixture.manager.contentHost(id)->mapTo(c, QPoint()), contentTop[id]);
            QCOMPARE(c->headerMode(), fixture.store.snapshot().containers[ids.indexOf(id)].header);
        }
        fixture.dock.hide();
    }

    void headerButtonNeverStartsADrag()
    {
        QWidget parent;
        parent.resize(800, 600);
        auto* c = new ContainerWidget(&parent);
        QPushButton content("An ordinary control");
        c->setContent(&content);
        c->setDockMode(DockMode::OverlayDocked);
        c->setHeaderMode(HeaderMode::Always);
        c->setGeometry(20, 20, 420, 260);
        parent.show();
        QVERIFY(QTest::qWaitForWindowExposed(&parent));
        settle();
        auto* header = c->workspaceHeader();
        QVERIFY(header && header->isVisible());
        QSignalSpy started(c, &ContainerWidget::geometryInteractionStarted);
        QSignalSpy floated(c, &ContainerWidget::floatRequested);
        const QPoint position = c->pos();

        // A click on the pop-out button pops out; it never drags.
        QTest::mouseClick(header->popOutButton(), Qt::LeftButton);
        QCOMPARE(floated.count(), 1);
        QCOMPARE(started.count(), 0);
        QCOMPARE(c->pos(), position);

        // Press on the options button and pull away: no drag, no move.
        QPushButton* options = header->optionsButton();
        QTest::mousePress(options, Qt::LeftButton, {}, options->rect().center());
        QTest::mouseMove(options, options->rect().center() + QPoint(-60, 40));
        QTest::mouseRelease(options, Qt::LeftButton, {}, options->rect().center() + QPoint(-60, 40));
        QCOMPARE(started.count(), 0);
        QCOMPARE(c->pos(), position);

        // The header itself does drag.
        const QPoint grab(header->grip()->geometry().right() + 60, WorkspaceGroupHeader::kHeight / 2);
        QTest::mousePress(header, Qt::LeftButton, {}, grab);
        QCOMPARE(started.count(), 1);
        QTest::mouseRelease(header, Qt::LeftButton, {}, grab);
        content.setParent(nullptr);
    }
};

QTEST_MAIN(TstWorkspaceHeaderModes)
#include "tst_workspace_header_modes.moc"
