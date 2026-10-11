// no-port-check: NereusSDR-original. The GPU waterfall draws nothing across
// the seam of its ring texture, and a paused waterfall stays where it is.
//
// 2026-10-11, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
// JJ saw the lines flicker at the bottom of the waterfall as they scrolled
// out of view, on Windows and on the Mac. The waterfall is a ring texture
// that repeats down its height, drawn through a linear filter at a position
// that glides one row between two rows (the 2026-05-25 sub-row scroll). The
// glide needs one more row below the oldest than the ring holds, and the
// repeat handed it the newest row instead: right after each new row the
// bottom line of the waterfall showed that new row, then faded back to the
// oldest one. Where a row is two screen pixels tall the top line took a
// quarter of the oldest row the same way, and the bottom line a quarter of
// the newest, with or without the glide.
//
// The same glide ran while the waterfall was paused, restarted by every row
// that arrived underneath, so the paused picture jumped up a row and slid
// back each time.
//
// A third fault showed up while writing this: the texture upload walks from
// the last row it sent to the newest one, so after a ring's worth of rows or
// more with no frame drawn between (a hidden, covered or minimized window)
// it sent only the remainder and left the rest of the picture as it was
// before.
//
// Needs a real QRhi: the offscreen platform gives the widget no GPU backend,
// so renderGpuFrame() never runs there. Registered NATIVE_WINDOW with the
// extra `gpu` label; it skips where no QRhi comes up.

#include <QImage>
#include <QSignalSpy>
#include <QTest>
#include <QVector>
#include <cmath>

#include "NativeWindowFrames.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;
using NereusSDR::NativeWindowFrames::FrameWait;

namespace {

// Fixed waterfall levels, and a dark and a bright row well inside them.
constexpr float kLowDbm = -130.0f;
constexpr float kHighDbm = -50.0f;
constexpr float kDarkDbm = -126.0f;
constexpr float kBrightDbm = -54.0f;
constexpr int kRowPixels = 256;
// The longest row period the widget takes. A frame drawn at once after a
// row lands near the start of the glide; one drawn after kSettleMs lands
// after its end.
constexpr int kRowPeriodMs = 500;
constexpr int kSettleMs = 650;
constexpr int kFrameTimeoutMs = 2000;
// setWfUpdatePeriodMs() re-sizes the rewind history 250 ms later and
// rebuilds the picture from it; let that pass before any row is pushed.
constexpr int kHistoryResizeMs = 350;
// Rows marked bright in a block, so a few of them are bright in full
// wherever between two rows the frame was drawn.
constexpr int kBlockRows = 8;
// A row "shows" a block at or above kPresent of its brightness and is
// "free" of it at or below kAbsent.
constexpr double kPresent = 0.97;
constexpr double kAbsent = 0.03;
// The dark and bright rows must differ by at least this much (summed over
// red, green and blue, 0..765) for a share to mean anything.
constexpr double kMinContrast = 90.0;

// Where the waterfall lies in a grabbed frame, in device pixels.
struct WaterfallArea {
    int top{0};
    int bottom{0};
    int left{0};
    int right{0};
    double dpr{1.0};

    int rowsPerLine() const { return qMax(1, static_cast<int>(std::lround(dpr))); }
};

WaterfallArea waterfallArea(const SpectrumWidget& w, const QImage& frame)
{
    WaterfallArea area;
    area.dpr = static_cast<double>(frame.height()) / qMax(1, w.height());
    const QRect wf = w.waterfallRectForTest();
    area.top = static_cast<int>(std::lround(wf.top() * area.dpr));
    area.bottom = static_cast<int>(std::lround((wf.bottom() + 1) * area.dpr)) - 1;
    // A band in the left half, clear of the strip on the right edge.
    area.left = static_cast<int>(std::lround((wf.left() + wf.width() * 0.10) * area.dpr));
    area.right = static_cast<int>(std::lround((wf.left() + wf.width() * 0.40) * area.dpr));
    return area;
}

// Mean brightness of one device row over the sampled columns: red, green
// and blue summed, so every channel counts.
double rowLevel(const QImage& rgb, const WaterfallArea& area, int y)
{
    const QRgb* line = reinterpret_cast<const QRgb*>(rgb.constScanLine(y));
    qint64 sum = 0;
    for (int x = area.left; x < area.right; ++x) {
        sum += qRed(line[x]) + qGreen(line[x]) + qBlue(line[x]);
    }
    return static_cast<double>(sum) / qMax(1, area.right - area.left);
}

// Mean brightness of waterfall lines [firstLine, lastLine], counted from the
// top of the waterfall in logical pixels.
double linesLevel(const QImage& rgb, const WaterfallArea& area, int firstLine, int lastLine)
{
    const int first = area.top + static_cast<int>(std::lround(firstLine * area.dpr));
    const int last = area.top + static_cast<int>(std::lround((lastLine + 1) * area.dpr)) - 1;
    double sum = 0.0;
    int count = 0;
    for (int y = first; y <= last && y <= area.bottom; ++y) {
        sum += rowLevel(rgb, area, y);
        ++count;
    }
    return count > 0 ? sum / count : 0.0;
}

void pushRows(SpectrumWidget& w, int count, float dbm)
{
    const QVector<float> row(kRowPixels, dbm);
    for (int i = 0; i < count; ++i) {
        w.pushWaterfallRowForTest(row);
    }
}

// As pushRows(), drawing a frame at least every half ring, so the rows
// reach the texture the way they do in a window that is being drawn.
void pushRowsDrawn(SpectrumWidget& w, int count, float dbm)
{
    const int chunk = qMax(1, w.waterfallHeightForTest() / 2);
    while (count > 0) {
        const int rows = qMin(count, chunk);
        pushRows(w, rows, dbm);
        static_cast<void>(w.grabFramebuffer());
        count -= rows;
    }
}

}  // namespace

class TestSpectrumGpuWaterfallRingSeam : public QObject {
    Q_OBJECT

private:
    enum class Stage { Ready, NoGpu, Failed };

    // Shows the widget with fixed waterfall levels, its own row ticker
    // stopped, and waits for the first GPU frame.
    static Stage start(SpectrumWidget& w, QSignalSpy& submitted, QString* why)
    {
        w.resize(640, 480);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w.setWaterfallTickerPausedForTest(true);
        w.setWfAgcEnabled(false);
        w.setClarityActive(false);
        w.setWfLowThreshold(kLowDbm);
        w.setWfHighThreshold(kHighDbm);
        w.setWfOpacity(100);
        w.setShowRxFilterOnWaterfall(false);
        w.setWfUpdatePeriodMs(kRowPeriodMs);
        w.show();
        if (!QTest::qWaitForWindowExposed(&w)) {
            *why = QStringLiteral("the test window was never exposed");
            return Stage::Failed;
        }
        const FrameWait wait = NativeWindowFrames::requestFrame(w, submitted, kFrameTimeoutMs);
        if (wait == FrameWait::Hidden) {
            *why = QString::fromLatin1(NativeWindowFrames::kHiddenFailure);
            return Stage::Failed;
        }
        if (wait == FrameWait::Stalled) {
            if (submitted.isEmpty()) {
                return Stage::NoGpu;
            }
            *why = QString::fromLatin1(NativeWindowFrames::kStalledFailure);
            return Stage::Failed;
        }
        QTest::qWait(kHistoryResizeMs);
        if (w.waterfallHeightForTest() < 4 * kBlockRows) {
            *why = QStringLiteral("the waterfall is only %1 rows tall")
                       .arg(w.waterfallHeightForTest());
            return Stage::Failed;
        }
        return Stage::Ready;
    }

    // What a frame shows at the waterfall's two edges, as shares of the
    // bright block's brightness: 0 is a dark row, 1 a bright one.
    struct Edges {
        double topRow{0.0};     // first device row of the waterfall
        double bottomRow{0.0};  // last device row
        double contrast{0.0};
        QString text;
    };

    // Grabs a frame and reads its edges. `brightFirstLine` is the first of
    // three waterfall lines (logical, from the top) that are bright in full.
    static Edges grabEdges(SpectrumWidget& w, int brightFirstLine)
    {
        Edges edges;
        const QImage frame = w.grabFramebuffer();
        if (frame.isNull()) {
            edges.text = QStringLiteral("the framebuffer grab came back empty");
            return edges;
        }
        const QImage rgb = frame.convertToFormat(QImage::Format_RGB32);
        const WaterfallArea area = waterfallArea(w, rgb);
        const int lines = w.waterfallHeightForTest();
        const double dark = linesLevel(rgb, area, lines / 2, lines / 2 + 7);
        const double bright = linesLevel(rgb, area, brightFirstLine, brightFirstLine + 2);
        edges.contrast = bright - dark;
        if (edges.contrast >= kMinContrast) {
            edges.topRow = (rowLevel(rgb, area, area.top) - dark) / edges.contrast;
            edges.bottomRow = (rowLevel(rgb, area, area.bottom) - dark) / edges.contrast;
        }
        edges.text = QStringLiteral(
            "top row %1, bottom row %2 of the bright block (dark %3, bright %4, "
            "%5 screen pixels to a row, %6 rows)")
            .arg(edges.topRow, 0, 'f', 3).arg(edges.bottomRow, 0, 'f', 3)
            .arg(dark, 0, 'f', 1).arg(bright, 0, 'f', 1)
            .arg(area.dpr, 0, 'f', 2).arg(lines);
        return edges;
    }

    // Where the bright block's brightness sits in a grabbed frame, as a
    // waterfall line counted from the top (its centroid).
    static bool blockCenter(SpectrumWidget& w, double* center, QString* text)
    {
        const QImage frame = w.grabFramebuffer();
        if (frame.isNull()) {
            *text = QStringLiteral("the framebuffer grab came back empty");
            return false;
        }
        const QImage rgb = frame.convertToFormat(QImage::Format_RGB32);
        const WaterfallArea area = waterfallArea(w, rgb);
        const double dark = rowLevel(rgb, area, area.top + 2 * area.rowsPerLine());
        double weight = 0.0;
        double moment = 0.0;
        double peak = 0.0;
        for (int y = area.top; y <= area.bottom; ++y) {
            const double level = qMax(0.0, rowLevel(rgb, area, y) - dark);
            weight += level;
            moment += level * (y - area.top);
            peak = qMax(peak, level);
        }
        if (peak < kMinContrast) {
            *text = QStringLiteral("the bright block is not in the picture (peak %1)")
                        .arg(peak, 0, 'f', 1);
            return false;
        }
        *center = moment / weight / area.dpr;
        return true;
    }

private slots:
    // The newest rows are the bright ones. Right after a row arrives, and
    // again once its glide has ended, the bottom row shows none of them and
    // the top row ends up showing nothing else.
    void newestRow_staysOutOfTheBottomEdge()
    {
        SpectrumWidget w;
        QSignalSpy submitted(&w, &QRhiWidget::frameSubmitted);
        QString why;
        const Stage stage = start(w, submitted, &why);
        if (stage == Stage::NoGpu) {
            QSKIP("no QRhi on this platform; the GPU frame path did not run");
        }
        QVERIFY2(stage == Stage::Ready, qPrintable(why));

        const int lines = w.waterfallHeightForTest();
        pushRowsDrawn(w, lines + 4, kDarkDbm);
        pushRows(w, kBlockRows, kBrightDbm);

        // Lines 3 to 5 lie inside the block wherever the glide stands.
        const Edges arriving = grabEdges(w, 3);
        qInfo().noquote() << "just after a new row:" << arriving.text;
        QVERIFY2(arriving.contrast >= kMinContrast, qPrintable(arriving.text));
        QVERIFY2(arriving.bottomRow <= kAbsent,
                 qPrintable(QStringLiteral("the newest row shows at the bottom edge just "
                                           "after it arrives: ") + arriving.text));

        QTest::qWait(kSettleMs);
        const Edges settled = grabEdges(w, 3);
        qInfo().noquote() << "after the glide:" << settled.text;
        QVERIFY2(settled.contrast >= kMinContrast, qPrintable(settled.text));
        QVERIFY2(settled.bottomRow <= kAbsent,
                 qPrintable(QStringLiteral("the newest row shows at the bottom edge: ")
                            + settled.text));
        QVERIFY2(settled.topRow >= kPresent,
                 qPrintable(QStringLiteral("the top edge is not the newest row alone: ")
                            + settled.text));
    }

    // The oldest rows are the bright ones. The bottom row shows them, just
    // after a row arrives as well, and the top row shows none of them.
    void oldestRow_holdsTheBottomEdge()
    {
        SpectrumWidget w;
        QSignalSpy submitted(&w, &QRhiWidget::frameSubmitted);
        QString why;
        const Stage stage = start(w, submitted, &why);
        if (stage == Stage::NoGpu) {
            QSKIP("no QRhi on this platform; the GPU frame path did not run");
        }
        QVERIFY2(stage == Stage::Ready, qPrintable(why));

        const int lines = w.waterfallHeightForTest();
        pushRowsDrawn(w, lines + 4, kDarkDbm);
        pushRows(w, kBlockRows, kBrightDbm);
        // The block is now the oldest kBlockRows rows; the row pushed
        // further down leaves kBlockRows - 1 of them.
        pushRowsDrawn(w, lines - kBlockRows, kDarkDbm);
        QTest::qWait(kSettleMs);

        const Edges settled = grabEdges(w, lines - 5);
        qInfo().noquote() << "after the glide:" << settled.text;
        QVERIFY2(settled.contrast >= kMinContrast, qPrintable(settled.text));
        QVERIFY2(settled.bottomRow >= kPresent,
                 qPrintable(QStringLiteral("the bottom edge is not the oldest row alone: ")
                            + settled.text));
        QVERIFY2(settled.topRow <= kAbsent,
                 qPrintable(QStringLiteral("the oldest row shows at the top edge: ")
                            + settled.text));

        pushRows(w, 1, kDarkDbm);
        const Edges arriving = grabEdges(w, lines - 5);
        qInfo().noquote() << "just after a new row:" << arriving.text;
        QVERIFY2(arriving.contrast >= kMinContrast, qPrintable(arriving.text));
        QVERIFY2(arriving.bottomRow >= kPresent,
                 qPrintable(QStringLiteral("the bottom edge lost the oldest row when a "
                                           "new row arrived: ") + arriving.text));
        QVERIFY2(arriving.topRow <= kAbsent,
                 qPrintable(QStringLiteral("the oldest row shows at the top edge: ")
                            + arriving.text));
    }

    // A paused waterfall does not move when a row arrives underneath it.
    void pausedWaterfall_staysStillWhenARowArrives()
    {
        SpectrumWidget w;
        QSignalSpy submitted(&w, &QRhiWidget::frameSubmitted);
        QString why;
        const Stage stage = start(w, submitted, &why);
        if (stage == Stage::NoGpu) {
            QSKIP("no QRhi on this platform; the GPU frame path did not run");
        }
        QVERIFY2(stage == Stage::Ready, qPrintable(why));

        // A bright block in the middle of the picture, with more rows of
        // history behind it than the waterfall shows.
        const int lines = w.waterfallHeightForTest();
        pushRowsDrawn(w, lines + 40, kDarkDbm);
        pushRows(w, kBlockRows, kBrightDbm);
        pushRowsDrawn(w, lines / 2, kDarkDbm);
        w.setWaterfallLiveForTest(false);
        QVERIFY(!w.wfLive());
        QTest::qWait(kSettleMs);

        double before = 0.0;
        QVERIFY2(blockCenter(w, &before, &why), qPrintable(why));
        // The check can fail: the block is where it was pushed, about half
        // way down, and not at an edge.
        QVERIFY2(before > lines * 0.3 && before < lines * 0.7,
                 qPrintable(QStringLiteral("the block sits at line %1 of %2")
                                .arg(before, 0, 'f', 2).arg(lines)));

        pushRows(w, 1, kDarkDbm);
        QVERIFY(!w.wfLive());
        double after = 0.0;
        QVERIFY2(blockCenter(w, &after, &why), qPrintable(why));
        const QString moved = QStringLiteral(
            "the paused picture's block sat at line %1 and at line %2 just after a row arrived")
            .arg(before, 0, 'f', 3).arg(after, 0, 'f', 3);
        qInfo().noquote() << moved;
        QVERIFY2(std::abs(after - before) < 0.05, qPrintable(moved));
    }

    // Rows that arrive while nothing is drawn (a hidden, covered or
    // minimized window) are all in the picture at the next frame, however
    // many they were.
    void rowsPushedWhileNothingWasDrawn_allReachTheScreen()
    {
        SpectrumWidget w;
        QSignalSpy submitted(&w, &QRhiWidget::frameSubmitted);
        QString why;
        const Stage stage = start(w, submitted, &why);
        if (stage == Stage::NoGpu) {
            QSKIP("no QRhi on this platform; the GPU frame path did not run");
        }
        QVERIFY2(stage == Stage::Ready, qPrintable(why));

        // Two rings' worth and four rows, with a bright block left half way
        // down. The write row ends four rows from where the last frame saw
        // it, so a walk between the two covers four rows and misses the
        // block.
        const int lines = w.waterfallHeightForTest();
        pushRows(w, 2 * lines + 4 - kBlockRows - lines / 2, kDarkDbm);
        pushRows(w, kBlockRows, kBrightDbm);
        pushRows(w, lines / 2, kDarkDbm);
        QTest::qWait(kSettleMs);

        double center = 0.0;
        QVERIFY2(blockCenter(w, &center, &why), qPrintable(why));
        const QString where = QStringLiteral("the block sits at line %1 of %2")
                                  .arg(center, 0, 'f', 2).arg(lines);
        qInfo().noquote() << where;
        // Its eight rows start lines / 2 rows down.
        QVERIFY2(std::abs(center - (lines / 2 + kBlockRows / 2.0)) < 1.0, qPrintable(where));
    }
};

QTEST_MAIN(TestSpectrumGpuWaterfallRingSeam)
#include "tst_spectrum_gpu_waterfall_ring_seam.moc"
