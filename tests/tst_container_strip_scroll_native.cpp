// no-port-check: NereusSDR-original native strip scroll characterization.
// Modification history (NereusSDR):
//   2026-10-09 - A docked stack (an applet over GPU meters) scrolled part
//                 way shows no black band over its meters on the composited
//                 window by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - The window is shortened until the stack overflows it: under
//                 Fusion with the look off the stack fits 520 px whole
//                 (Linux CI). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-11 - The scroll range is used only once it has held one value:
//                 a resize changes it at once, and the host's deferred
//                 projection of the bar rows changes it again a turn later
//                 (Linux CI read 40, then scrolled a range of 2). The layout
//                 half also runs offscreen. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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
#include <QElapsedTimer>
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
#include <optional>

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
// The scroll steps below go to fixed offsets up to 31, so the stack must
// overflow its viewport by more than that.
constexpr int kLeastScrollRange = 32;
// How the window is shortened where the stack fits it whole.
constexpr int kShortenStep = 40;
constexpr int kShortestWindow = 200;
// The scroll range counts as settled once it has held one value this long,
// and must get there within the timeout.
constexpr int kRangeSettleMs = 250;
constexpr int kRangeSettleTimeoutMs = 5000;
constexpr int kRangePollMs = 10;

// The scroll range once it has stopped changing; nothing when it never held
// one value for kRangeSettleMs. A resize moves the range twice: the scroll
// area re-ranges at once against the stack's old height, then the host's
// deferred projection (ContainerContentHost::scheduleStackProjection, a
// zero timer started by the resize) fits the bar rows to the new viewport
// and the range changes again. Bar rows stretch to fill a viewport taller
// than the stack and stop shrinking at their saved height, so a range read
// straight after shortening the window is the old stretch, not an overflow.
std::optional<int> settledScrollRange(const QScrollBar* bar)
{
    QElapsedTimer total;
    total.start();
    QElapsedTimer held;
    held.start();
    int range = bar->maximum();
    while (held.elapsed() < kRangeSettleMs) {
        if (total.elapsed() >= kRangeSettleTimeoutMs) {
            return std::nullopt;
        }
        QTest::qWait(kRangePollMs);
        if (bar->maximum() != range) {
            range = bar->maximum();
            held.restart();
        }
    }
    return range;
}

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
        if (QGuiApplication::platformName() == QStringLiteral("minimal")) {
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
        // The stack must overflow its viewport by at least the largest fixed
        // offset below. How tall the stack is follows the platform's style
        // and fonts and the look's entry headers: with the look off under
        // Fusion (Linux) it fits a 520 px window whole and nothing scrolls,
        // so the window is shortened until it does. Only a settled range
        // counts (see settledScrollRange).
        QScrollBar* bar = scroll->verticalScrollBar();
        const auto geometry = [&root, scroll, bar] {
            return QStringLiteral("window %1 x %2, viewport %3 px high, stack %4 px high, scroll range %5")
                .arg(root.width()).arg(root.height()).arg(scroll->viewport()->height())
                .arg(scroll->widget() != nullptr ? scroll->widget()->height() : -1).arg(bar->maximum());
        };
        std::optional<int> settled = settledScrollRange(bar);
        qInfo().noquote() << QTest::currentDataTag() << "settled:" << geometry();
        QVERIFY2(settled.has_value(), qPrintable(QStringLiteral("the scroll range never settled: ") + geometry()));
        while (*settled < kLeastScrollRange && root.height() - kShortenStep >= kShortestWindow) {
            root.resize(root.width(), root.height() - kShortenStep);
            qInfo().noquote() << QTest::currentDataTag() << "shortened, before settling:" << geometry();
            settled = settledScrollRange(bar);
            qInfo().noquote() << QTest::currentDataTag() << "settled:" << geometry();
            QVERIFY2(settled.has_value(), qPrintable(QStringLiteral("the scroll range never settled: ") + geometry()));
        }
        QVERIFY2(*settled >= kLeastScrollRange, qPrintable(geometry()));
        const int maximum = *settled;
        QCOMPARE(bar->maximum(), maximum);
        if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
            QSKIP("Needs NATIVE_WINDOW registration and a capture of its own window.");
        }
        QTest::qWait(kCompositeSettleMs);
        if (ownWindowCapture(root).isNull()) {
            QSKIP("A capture of the test's own window is unavailable (screen recording not granted).");
        }

        const QRect viewport(scroll->viewport()->mapTo(&root, QPoint()), scroll->viewport()->size());
        const QList<int> offsets{1, 19, maximum / 3, maximum, maximum - 17, maximum / 2, 0, 31, maximum, 3, 0};
        QStringList failures;
        for (qsizetype step = 0; step < offsets.size(); ++step) {
            const QString stage = QStringLiteral("%1-step-%2-offset-%3")
                .arg(QLatin1String(QTest::currentDataTag())).arg(step, 2, 10, QLatin1Char('0')).arg(offsets[step]);
            // The strip's content height must not move under the scroll:
            // the range is the settled one before and after every step.
            QVERIFY2(bar->maximum() == maximum,
                     qPrintable(QStringLiteral("%1: the scroll range left its settled %2 before this step: %3")
                                    .arg(stage).arg(maximum).arg(geometry())));
            bar->setValue(offsets[step]);
            QVERIFY2(bar->value() == offsets[step],
                     qPrintable(QStringLiteral("%1: the bar went to %2: %3").arg(stage).arg(bar->value()).arg(geometry())));
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
            qInfo().noquote() << stage << "black rows:" << band << "changed pixels:" << changed
                              << "scroll range:" << bar->maximum();
            QVERIFY2(bar->maximum() == maximum,
                     qPrintable(QStringLiteral("%1: the scroll range left its settled %2 during this step: %3")
                                    .arg(stage).arg(maximum).arg(geometry())));
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
