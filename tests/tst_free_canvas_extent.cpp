// no-port-check: NereusSDR-original offscreen regression for the free canvas extent.
// Modification history (NereusSDR):
//   2026-10-09 - The free canvas minimum follows where its views are now:
//                a view's earlier, larger boundary (a native window's
//                geometry before it settles) no longer keeps the canvas
//                larger until the next projection, which made the
//                profile-contest strips' minimum vary between launches.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>

#include <QPointer>
#include <QWidget>

#include "gui/containers/ContainerDocument.h"
#include "gui/containers/FreeCanvasSurface.h"

using namespace NereusSDR;

namespace {

// The surface's margin around its content (FreeCanvasSurface.cpp kMargin).
constexpr int kMargin = 20;

void settle()
{
    for (int pass = 0; pass < 4; ++pass) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

} // namespace

class TstFreeCanvasExtent : public QObject
{
    Q_OBJECT

private slots:
    void minimumFollowsTheViewsWhereTheyAreNow()
    {
        QWidget host;
        host.resize(1600, 400);
        auto* surface = new FreeCanvasSurface(&host);
        surface->resize(1600, 400);
        auto* view = new QWidget(surface);

        ContainerDocument document;
        document.id = QStringLiteral("extent");
        document.layout = ContentLayout::FreeCanvas;
        ContentEntry entry;
        entry.id = QStringLiteral("meter");
        entry.typeId = QStringLiteral("meter.sMeter");
        entry.setFreeCanvasRect(QRectF(0, 0, 420, 49));
        document.contents = {entry};

        host.show();
        surface->project(document, {{entry.id, QPointer<QWidget>(view)}});
        settle();
        const QSize placed = surface->minimumSize();
        // The view at its rect, with the margin beyond it.
        QCOMPARE(view->size(), QSize(420, 49));
        QVERIFY(placed.width() >= view->geometry().right() + 1 + kMargin);
        QVERIFY(placed.height() >= view->geometry().bottom() + 1 + kMargin);

        // A boundary reported larger for a while (a native child window
        // before its geometry settles), then back where the view belongs.
        const QRect home = view->geometry();
        view->setGeometry(home.adjusted(0, 0, 600, 30));
        settle();
        QVERIFY(surface->minimumHeight() > placed.height());
        view->setGeometry(home);
        settle();
        QVERIFY2(surface->minimumSize() == placed,
                 qPrintable(QStringLiteral("the canvas minimum stays %1x%2 after the view returned to %3x%4 "
                                           "(placed %5x%6)")
                                .arg(surface->minimumWidth()).arg(surface->minimumHeight())
                                .arg(home.width()).arg(home.height())
                                .arg(placed.width()).arg(placed.height())));
    }
};

QTEST_MAIN(TstFreeCanvasExtent)
#include "tst_free_canvas_extent.moc"
