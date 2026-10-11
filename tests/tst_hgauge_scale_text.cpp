// no-port-check: test-only. NereusSDR's own bar gauge (src/gui/HGauge.cpp).
//
// The gauge draws its scale numbers at 7 points. That is a 7 px font on the
// Mac and a 9 px font at 96 dpi (Windows at 100%, Linux). The widget was a
// fixed 30 px tall, which fits the 7 px numbers and cut the bottom two rows
// off the 9 px ones ("500" read like "5nn" in the amplifier panels).
//
// These cases render the gauge with grab(), never shown, at both sizes
// through the gauge's scale font seam, so the 96 dpi case runs on any
// platform:
//   - at 9 px every scale number is drawn whole, with a clear row under it;
//   - at 7 px the gauge keeps its 30 px height and the numbers fit as before;
//   - at the platform's own 7 point size the numbers are whole as well;
//   - a gauge without scale numbers stays 30 px at either size.

#include <QtTest/QtTest>

#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPixmap>

#include "gui/HGauge.h"

using namespace NereusSDR;

namespace {

constexpr int kGaugeWidth = 252;   // the gauge's width in a 260 px applet
constexpr int kScaleTop = 22;      // first row under the bar and its gap
constexpr int kMacScalePx = 7;     // 7 points at the Mac's 72 dpi
constexpr int kWindowsScalePx = 9; // 7 points at 96 dpi

// The scale numbers of every gauge the app builds with a scale: the two
// amplifier panels (AmpApplet.cpp, Rf2ksApplet.cpp), TxApplet, PhoneCwApplet
// and ModMonitorApplet.
QList<QStringList> appLabelSets()
{
    return {
        {"0", "500", "1000", "1.5k", "2k"},
        {"1", "1.5", "2", "2.5", "3"},
        {"0", "30", "55", "80", "100"},
        {"1", "1.5", "2.5", "3"},
        {"0", "33", "67", "100", "120"},
        {"0.0", "1.7", "3.3", "5.0", "6.0"},
        {"-40dB", "-30", "-20", "-10", "0", "+5", "+10"},
        {"-25dB", "-20", "-15", "-10", "-5", "0"},
        {"0", "50", "100", "125", "160"},
        {"0", "25", "50", "75", "100"},
    };
}

QImage render(HGauge& gauge)
{
    gauge.resize(kGaugeWidth, gauge.height());
    return gauge.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

// Ink is a pixel that differs from the background by more than the faint
// haze antialiasing leaves on the row under a glyph (a few levels of 255).
constexpr int kInkThreshold = 32;

bool isInk(QRgb pixel, QRgb background)
{
    return qAbs(qRed(pixel) - qRed(background)) > kInkThreshold
        || qAbs(qGreen(pixel) - qGreen(background)) > kInkThreshold
        || qAbs(qBlue(pixel) - qBlue(background)) > kInkThreshold;
}

bool rowHasInk(const QImage& image, int y)
{
    // The top-left pixel is background: the bar starts two pixels in and
    // nine rows down.
    const QRgb background = image.pixel(0, 0);
    for (int x = 0; x < image.width(); ++x) {
        if (isInk(image.pixel(x, y), background)) { return true; }
    }
    return false;
}

// The last row at or below fromRow that holds ink, or -1 when none does.
int lastInkRow(const QImage& image, int fromRow)
{
    for (int y = image.height() - 1; y >= fromRow; --y) {
        if (rowHasInk(image, y)) { return y; }
    }
    return -1;
}

int firstInkRow(const QImage& image, int fromRow)
{
    for (int y = fromRow; y < image.height(); ++y) {
        if (rowHasInk(image, y)) { return y; }
    }
    return -1;
}

// How many rows the labels' ink covers when nothing can cut it: the same
// text in the same font on a canvas with room to spare. The canvas takes
// the gauge's dpi so a font sized in points comes out the same pixel size.
int wholeInkRows(const QStringList& labels, const QFont& font, int dpi)
{
    QImage canvas(400, 60, QImage::Format_ARGB32);
    const int dotsPerMeter = qRound(dpi / 0.0254);
    canvas.setDotsPerMeterX(dotsPerMeter);
    canvas.setDotsPerMeterY(dotsPerMeter);
    canvas.fill(Qt::black);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing);
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRect(4, 20, 392, 40), Qt::AlignLeft | Qt::AlignTop,
               labels.join(QLatin1Char(' ')));
    p.end();
    const int first = firstInkRow(canvas, 1);
    const int last = lastInkRow(canvas, 1);
    return (first < 0) ? 0 : last - first + 1;
}

void setUpGauge(HGauge& gauge, const QStringList& labels)
{
    gauge.setRange(0.0, 2000.0);
    gauge.setYellowStart(1000.0);
    gauge.setRedStart(1500.0);
    gauge.setTitle(QStringLiteral("Fwd Pwr"));
    gauge.setTickLabels(labels);
}

} // namespace

class TestHGaugeScaleText : public QObject
{
    Q_OBJECT

private:
    // Every label set: the numbers are whole and one row under them is clear.
    void scaleNumbersAreWhole(int pixelSize);

private slots:
    void windowsSizeNumbersAreWholeWithAClearRowUnder();
    void macSizeNumbersAreWholeWithAClearRowUnder();
    void platformSizeNumbersAreWholeWithAClearRowUnder();
    void macSizeGaugeKeepsItsThirtyPixels();
    void windowsSizeGaugeGrowsOnlyByWhatTheNumbersNeed();
    void gaugeWithoutScaleStaysThirtyPixels();
    void barDoesNotMoveWhenTheGaugeGrows();
};
void TestHGaugeScaleText::scaleNumbersAreWhole(int pixelSize)
{
    const QList<QStringList> sets = appLabelSets();
    for (const QStringList& labels : sets) {
        HGauge gauge;
        setUpGauge(gauge, labels);
        gauge.setScaleFontPixelSize(pixelSize);
        const QImage image = render(gauge);
        const QByteArray what = labels.join(QLatin1Char(' ')).toUtf8();

        const int first = firstInkRow(image, kScaleTop);
        const int last = lastInkRow(image, kScaleTop);
        QVERIFY2(first >= 0, what.constData());
        // Nothing in the widget's last row, so the ink ends at least one
        // row above the bottom edge.
        QVERIFY2(!rowHasInk(image, image.height() - 1),
                 qPrintable(QStringLiteral("%1: scale ink in the last row of a %2 px gauge")
                                .arg(QString::fromUtf8(what)).arg(image.height())));
        QVERIFY2(last <= image.height() - 2,
                 qPrintable(QStringLiteral("%1: scale ink reaches row %2 of a %3 px gauge")
                                .arg(QString::fromUtf8(what)).arg(last).arg(image.height())));

        // Whole: as many rows of ink as the same text has with nothing to
        // cut it.
        QFont font(gauge.font().family(), 7);
        if (pixelSize > 0) { font.setPixelSize(pixelSize); }
        QCOMPARE(last - first + 1, wholeInkRows(labels, font, gauge.logicalDpiY()));
    }
}

void TestHGaugeScaleText::windowsSizeNumbersAreWholeWithAClearRowUnder()
{
    scaleNumbersAreWhole(kWindowsScalePx);
}

void TestHGaugeScaleText::macSizeNumbersAreWholeWithAClearRowUnder()
{
    scaleNumbersAreWhole(kMacScalePx);
}

void TestHGaugeScaleText::platformSizeNumbersAreWholeWithAClearRowUnder()
{
    // 0 is the gauge's default: 7 points at this platform's dpi.
    scaleNumbersAreWhole(0);
}

void TestHGaugeScaleText::macSizeGaugeKeepsItsThirtyPixels()
{
    const QList<QStringList> sets = appLabelSets();
    for (const QStringList& labels : sets) {
        HGauge gauge;
        setUpGauge(gauge, labels);
        gauge.setScaleFontPixelSize(kMacScalePx);
        QCOMPARE(gauge.height(), 30);
        QCOMPARE(gauge.minimumHeight(), 30);
        QCOMPARE(gauge.maximumHeight(), 30);
        QCOMPARE(gauge.sizeHint().height(), 30);
        QCOMPARE(render(gauge).height(), 30);
    }
}

void TestHGaugeScaleText::windowsSizeGaugeGrowsOnlyByWhatTheNumbersNeed()
{
    HGauge gauge;
    setUpGauge(gauge, {"0", "500", "1000", "1.5k", "2k"});
    gauge.setScaleFontPixelSize(kWindowsScalePx);
    const QImage image = render(gauge);
    QVERIFY(gauge.height() > 30);
    QCOMPARE(gauge.minimumHeight(), gauge.height());
    QCOMPARE(gauge.maximumHeight(), gauge.height());
    QCOMPARE(gauge.sizeHint().height(), gauge.height());
    // No more than the font's own headroom under the digits: the ink ends
    // within three rows of the bottom edge.
    QVERIFY(lastInkRow(image, kScaleTop) >= image.height() - 4);

    // Back at the Mac's size the same gauge is 30 px again.
    gauge.setScaleFontPixelSize(kMacScalePx);
    QCOMPARE(gauge.height(), 30);
}

void TestHGaugeScaleText::gaugeWithoutScaleStaysThirtyPixels()
{
    HGauge gauge;
    gauge.setTitle(QStringLiteral("Level"));
    gauge.setScaleFontPixelSize(kWindowsScalePx);
    QCOMPARE(gauge.height(), 30);
    gauge.setScaleFontPixelSize(kMacScalePx);
    QCOMPARE(gauge.height(), 30);
}

void TestHGaugeScaleText::barDoesNotMoveWhenTheGaugeGrows()
{
    HGauge small;
    setUpGauge(small, {"0", "500", "1000", "1.5k", "2k"});
    small.setScaleFontPixelSize(kMacScalePx);
    HGauge tall;
    setUpGauge(tall, {"0", "500", "1000", "1.5k", "2k"});
    tall.setScaleFontPixelSize(kWindowsScalePx);

    const QImage a = render(small);
    const QImage b = render(tall);
    QVERIFY(b.height() > a.height());
    // The rows above the scale (the bar and its title) are the same picture.
    QCOMPARE(b.copy(0, 0, b.width(), kScaleTop), a.copy(0, 0, a.width(), kScaleTop));
}

QTEST_MAIN(TestHGaugeScaleText)
#include "tst_hgauge_scale_text.moc"
