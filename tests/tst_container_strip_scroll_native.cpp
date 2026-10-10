// no-port-check: NereusSDR-original native strip scroll characterization.
// Modification history (NereusSDR):
//   2026-10-09 - A docked stack (an applet over GPU meters) scrolled part
//                 way shows no black band over its meters on the composited
//                 window by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//
// Native only: QWidget::grab() renders a QRhiWidget through Qt's readback,
// not through the window server, so an offscreen or grabbed image cannot
// show a compositing fault. This test captures its own window from the
// screen (never the desktop) after each scroll step, before any forced
// redraw, and compares it with a capture taken after every meter has
// rendered a fresh frame. The two must match inside the scroll viewport,
// and no row of the viewport may turn black where the fresh capture has
// content.
#include <QtTest>
#include <QDir>
#include <QFrame>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QWindow>
#include <memory>

#include "core/AppSettings.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {
constexpr int kFrameTimeoutMs = 3000;
// How long the window server gets to show a submitted frame.
constexpr int kCompositeSettleMs = 120;
// A row is black when nearly all of it is darker than this.
constexpr int kBlackLuma = 10;
constexpr double kBlackRowShare = 0.95;

QImage ownWindowCapture(QWidget& window)
{
    QWindow* handle = window.windowHandle();
    if (handle == nullptr || handle->screen() == nullptr || !handle->isExposed() || window.internalWinId() == 0) {
        return {};
    }
    // Nonzero, own fixture window only; never grabWindow(0), which would
    // capture the operator's desktop.
    return handle->screen()->grabWindow(window.internalWinId(), 0, 0, window.width(), window.height()).toImage();
}

QRect pixelRect(const QRect& logical, const QImage& image, const QSize& logicalSize)
{
    const qreal xScale = qreal(image.width()) / logicalSize.width();
    const qreal yScale = qreal(image.height()) / logicalSize.height();
    return QRect(qRound(logical.x() * xScale), qRound(logical.y() * yScale),
                 qRound(logical.width() * xScale), qRound(logical.height() * yScale));
}

bool rowIsBlack(const QImage& image, int y, const QRect& pixels)
{
    int dark = 0;
    for (int x = pixels.left(); x <= pixels.right(); ++x) {
        if (qGray(image.pixel(x, y)) < kBlackLuma) {
            ++dark;
        }
    }
    return dark >= kBlackRowShare * pixels.width();
}

// Rows black in the first capture where the fresh capture has content.
int blackBandRows(const QImage& stale, const QImage& fresh, const QRect& pixels)
{
    int rows = 0;
    for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
        if (rowIsBlack(stale, y, pixels) && !rowIsBlack(fresh, y, pixels)) {
            ++rows;
        }
    }
    return rows;
}

qsizetype changedPixels(const QImage& a, const QImage& b, const QRect& pixels)
{
    qsizetype count = 0;
    for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
        for (int x = pixels.left(); x <= pixels.right(); ++x) {
            if (a.pixel(x, y) != b.pixel(x, y)) {
                ++count;
            }
        }
    }
    return count;
}
} // namespace

class TstContainerStripScrollNative : public QObject {
    Q_OBJECT
private slots:
    void scrolledStripShowsNoBlackBand_data()
    {
        QTest::addColumn<bool>("look");
        QTest::newRow("today") << false;
        QTest::newRow("workspace") << true;
    }

    void scrolledStripShowsNoBlackBand()
    {
        QFETCH(bool, look);
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")
            || QGuiApplication::platformName() == QStringLiteral("minimal")) {
            QSKIP("Needs NATIVE_WINDOW registration and a capture of its own window.");
        }
        WorkspacePreview::setEnabledForTest(look);
        const auto restoreLook = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        QTemporaryDir settingsDirectory;
        QVERIFY(settingsDirectory.isValid());
        const QString captureDirectory = qEnvironmentVariable("STRIP_SCROLL_NATIVE_CAPTURE_DIR");
        if (!captureDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(captureDirectory));
        }
        AppSettings settings(settingsDirectory.filePath("settings"));
        ContainerWorkspaceStore store(settings);
        ContainerContentRegistry registry;
        MeterPoller poller;
        QWidget root;
        root.setWindowFlag(Qt::WindowDoesNotAcceptFocus);
        root.setAttribute(Qt::WA_ShowWithoutActivating);
        auto* rootLayout = new QVBoxLayout(&root);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        auto* splitter = new QSplitter(&root);
        rootLayout->addWidget(splitter);
        ContainerManager manager(&root, splitter);
        manager.setWorkspaceAdapter(&store, &registry);
        manager.setPreviewPoller(&poller);

        // Stands in for the RX applet: an ordinary raster widget with
        // content in every row, above the meters in the same stack.
        auto* applet = new QFrame(&root);
        applet->setObjectName(QStringLiteral("stripScrollApplet"));
        applet->setFixedHeight(320);
        applet->setStyleSheet(QStringLiteral("QFrame#stripScrollApplet { background: #5a6470; }"));
        auto* appletLayout = new QVBoxLayout(applet);
        for (int i = 0; i < 8; ++i) {
            appletLayout->addWidget(new QLabel(QStringLiteral("Applet row %1").arg(i + 1), applet));
        }
        registry.attachSingleton(QStringLiteral("applet:rx"), applet);

        WorkspaceDocument document;
        document.mainContainerId = QStringLiteral("strip-scroll");
        ContainerDocument container;
        container.id = document.mainContainerId;
        container.name = QStringLiteral("Receive");
        container.layout = ContentLayout::VerticalStack;
        container.contents.append(registry.makeEntry(QStringLiteral("applet:rx")));
        for (const QString& type : {QStringLiteral("meter.leveler"), QStringLiteral("meter.alcGain"),
                                    QStringLiteral("meter.comp"), QStringLiteral("meter.cfc")}) {
            ContentEntry entry = registry.makeEntry(type);
            QVERIFY2(registry.validateEntry(entry).isEmpty(), qPrintable(type));
            container.contents.append(entry);
        }
        document.containers = {container};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
        ContainerContentHost* host = manager.contentHost(container.id);
        QVERIFY(host);
        QScrollArea* scroll = host->findChild<QScrollArea*>();
        QVERIFY(scroll);
        const QList<MeterWidget*> meters = scroll->findChildren<MeterWidget*>();
        QVERIFY(!meters.isEmpty());

        root.resize(360, 520);
        root.show();
        QVERIFY(QTest::qWaitForWindowExposed(&root));
        QTRY_VERIFY_WITH_TIMEOUT(scroll->verticalScrollBar()->maximum() > 0, kFrameTimeoutMs);
        QTest::qWait(kCompositeSettleMs);
        if (ownWindowCapture(root).isNull()) {
            QSKIP("A capture of the test's own window is unavailable (screen recording not granted).");
        }

        const QRect viewport(scroll->viewport()->mapTo(&root, QPoint()), scroll->viewport()->size());
        QScrollBar* bar = scroll->verticalScrollBar();
        const int maximum = bar->maximum();
        const QList<int> offsets{1, 19, maximum / 3, maximum, maximum - 17, maximum / 2, 0, 31, maximum, 3, 0};
        QStringList failures;
        for (qsizetype step = 0; step < offsets.size(); ++step) {
            const QString stage = QStringLiteral("%1-step-%2-offset-%3")
                .arg(QLatin1String(QTest::currentDataTag())).arg(step, 2, 10, QLatin1Char('0')).arg(offsets[step]);
            bar->setValue(offsets[step]);
            QCOMPARE(bar->value(), offsets[step]);
            QCoreApplication::processEvents();
            QTest::qWait(kCompositeSettleMs);
            // The first capture precedes any forced redraw: it is what the
            // operator sees after scrolling.
            const QImage stale = ownWindowCapture(root);
            QVERIFY2(!stale.isNull(), qPrintable(stage + ": the window capture became unavailable"));
#ifdef NEREUS_GPU_SPECTRUM
            for (MeterWidget* meter : meters) {
                QSignalSpy frames(meter, &QRhiWidget::frameSubmitted);
                meter->update();
                if (meter->visibleRegion().isEmpty()) {
                    continue;
                }
                QTRY_VERIFY_WITH_TIMEOUT(frames.count() > 0, kFrameTimeoutMs);
            }
#endif
            root.repaint();
            QCoreApplication::processEvents();
            QTest::qWait(kCompositeSettleMs);
            const QImage fresh = ownWindowCapture(root);
            QVERIFY2(!fresh.isNull(), qPrintable(stage + ": the window capture became unavailable"));
            QCOMPARE(fresh.size(), stale.size());
            if (!captureDirectory.isEmpty()) {
                QVERIFY(stale.save(captureDirectory + QLatin1Char('/') + stage + QStringLiteral("-after-scroll.png")));
                QVERIFY(fresh.save(captureDirectory + QLatin1Char('/') + stage + QStringLiteral("-after-redraw.png")));
            }
            const QRect pixels = pixelRect(viewport, stale, root.size()).intersected(stale.rect());
            const int band = blackBandRows(stale, fresh, pixels);
            const qsizetype changed = changedPixels(stale, fresh, pixels);
            qInfo().noquote() << stage << "black rows:" << band << "changed pixels:" << changed;
            if (band > 0) {
                failures.append(QStringLiteral("%1: %2 viewport rows black after scrolling, filled after a redraw")
                                    .arg(stage).arg(band));
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TstContainerStripScrollNative)
#include "tst_container_strip_scroll_native.moc"
