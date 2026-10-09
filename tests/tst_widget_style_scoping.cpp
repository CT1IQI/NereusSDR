// no-port-check: NereusSDR-original offscreen regression for container stylesheets that boxed their children.
// Modification history (NereusSDR):
//   2026-10-09 - A container's own border stays on the container: the
//                antenna switch toast draws one bar on its left edge and
//                its check mark is U+2713, and the floating pan's title
//                bar and the VAX first-run rows draw their bottom line
//                once, not under every label. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>

#include <QColor>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QVector>

#include "core/audio/VirtualCableDetector.h"
#include "gui/PanadapterApplet.h"
#include "gui/StyleConstants.h"
#include "gui/VaxFirstRunDialog.h"
#include "gui/widgets/AntennaSwitchToast.h"

using namespace NereusSDR;

namespace {

// The antenna switch toast's accent (AntennaSwitchToast.cpp).
constexpr QRgb kToastAccent = 0xff00ff88;
// A border edge: most of the edge's pixels are the border colour. A glyph
// that touches an edge covers far less than this.
constexpr double kEdgeShare = 0.6;
constexpr int kColourTolerance = 8;

enum class Edge { Left, Right, Top, Bottom };

QImage imageOf(QWidget* w)
{
    w->ensurePolished();
    return w->grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

bool sameColour(QRgb a, QRgb b)
{
    return qAlpha(a) > 200 && std::abs(qRed(a) - qRed(b)) <= kColourTolerance
           && std::abs(qGreen(a) - qGreen(b)) <= kColourTolerance
           && std::abs(qBlue(a) - qBlue(b)) <= kColourTolerance;
}

// The share of the pixels along one edge line of `img` (`inset` pixels in)
// that are `colour`.
double edgeShare(const QImage& img, Edge edge, QRgb colour, int inset = 0)
{
    const bool vertical = edge == Edge::Left || edge == Edge::Right;
    const int length = vertical ? img.height() : img.width();
    if (length == 0 || img.width() <= inset || img.height() <= inset) {
        return 0.0;
    }
    int hits = 0;
    for (int i = 0; i < length; ++i) {
        int x = i;
        int y = i;
        switch (edge) {
        case Edge::Left: x = inset; break;
        case Edge::Right: x = img.width() - 1 - inset; break;
        case Edge::Top: y = inset; break;
        case Edge::Bottom: y = img.height() - 1 - inset; break;
        }
        if (sameColour(img.pixel(x, y), colour)) {
            ++hits;
        }
    }
    return double(hits) / double(length);
}

// The first edge of `w`'s own rendering that is drawn in `colour`, or an
// empty string.
QString borderEdgeIn(QWidget* w, QRgb colour)
{
    const QImage img = imageOf(w);
    const QList<QPair<Edge, const char*>> edges = {
        {Edge::Left, "left"}, {Edge::Right, "right"}, {Edge::Top, "top"}, {Edge::Bottom, "bottom"}};
    for (const auto& [edge, name] : edges) {
        if (edgeShare(img, edge, colour) >= kEdgeShare) {
            return QString::fromLatin1(name);
        }
    }
    return {};
}

QRgb rgbOf(const char* hex)
{
    return QColor(QString::fromLatin1(hex)).rgba();
}

QVector<DetectedCable> cablesPayload()
{
    QVector<DetectedCable> v;
    v.push_back({VirtualCableProduct::VbCableA, QStringLiteral("CABLE-A Input (VB-Audio Virtual Cable)"), true, 0});
    v.push_back({VirtualCableProduct::VbCableB, QStringLiteral("CABLE-B Input (VB-Audio Virtual Cable)"), true, 0});
    return v;
}

} // namespace

class TstWidgetStyleScoping : public QObject
{
    Q_OBJECT

private slots:
    void antennaToastCheckMarkIsU2713()
    {
        AntennaSwitchToast toast(QStringLiteral("Slice A moved from ANT1 to ANT2."));
        QStringList texts;
        bool found = false;
        for (QLabel* label : toast.findChildren<QLabel*>()) {
            texts << label->text();
            found = found || label->text() == QString(QChar(0x2713));
        }
        QVERIFY2(found, qPrintable(QStringLiteral("no label reads U+2713; labels: [%1]")
                                       .arg(texts.join(QStringLiteral("], [")))));
    }

    void antennaToastDrawsItsOwnBar()
    {
        AntennaSwitchToast toast(QStringLiteral("Slice A moved from ANT1 to ANT2."));
        toast.layout()->activate();
        const QImage img = imageOf(&toast);
        QCOMPARE(img.size(), QSize(380, 50));
        // The 3 px accent bar is the toast's left edge.
        for (int x = 0; x < 3; ++x) {
            const double share = edgeShare(img, Edge::Left, kToastAccent, x);
            QVERIFY2(share >= kEdgeShare,
                     qPrintable(QStringLiteral("column %1 of the toast is %2% accent").arg(x).arg(share * 100.0)));
        }
        // Nowhere else is there a bar: no other column is mostly accent.
        for (int x = 3; x < img.width(); ++x) {
            const double share = edgeShare(img, Edge::Left, kToastAccent, x);
            QVERIFY2(share < kEdgeShare,
                     qPrintable(QStringLiteral("column %1 of the toast is %2% accent").arg(x).arg(share * 100.0)));
        }
    }

    void antennaToastChildrenAreNotBoxed()
    {
        AntennaSwitchToast toast(QStringLiteral("Slice A moved from ANT1 to ANT2."));
        toast.layout()->activate();
        toast.ensurePolished();
        for (QLabel* label : toast.findChildren<QLabel*>()) {
            for (const QRgb colour : {kToastAccent, rgbOf(Style::kBorder)}) {
                const QString edge = borderEdgeIn(label, colour);
                QVERIFY2(edge.isEmpty(), qPrintable(QStringLiteral("label \"%1\" draws a %2 border on its %3 edge")
                                                        .arg(label->text(), QColor(colour).name(), edge)));
            }
        }
    }

    void floatingPanTitleLabelsHaveNoBottomLine()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        const QRgb line = rgbOf("#203040");
        QLabel* title = nullptr;
        for (QLabel* label : applet.findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("Pan pan-0")) {
                title = label;
            }
        }
        QVERIFY(title != nullptr);
        QWidget* bar = title->parentWidget();
        QVERIFY(bar != nullptr);
        bar->ensurePolished();
        // The bar keeps its own bottom line.
        QCOMPARE(borderEdgeIn(bar, line), QStringLiteral("bottom"));
        const QList<QLabel*> labels = bar->findChildren<QLabel*>();
        QVERIFY(labels.size() >= 2);
        for (QLabel* label : labels) {
            const QString edge = borderEdgeIn(label, line);
            QVERIFY2(edge.isEmpty(), qPrintable(QStringLiteral("label \"%1\" draws the bar's line on its %2 edge")
                                                    .arg(label->text(), edge)));
        }
    }

    void vaxFirstRunRowLabelsHaveNoBottomLine()
    {
        VaxFirstRunDialog dlg(FirstRunScenario::WindowsCablesFound, cablesPayload());
        dlg.ensurePolished();
        const QRgb line = rgbOf(Style::kInsetBorder);
        int rows = 0;
        for (QLabel* label : dlg.findChildren<QLabel*>()) {
            if (!label->text().startsWith(QStringLiteral("VAX "))) {
                continue;
            }
            // The row is the VAX label's parent; it keeps its own line, and
            // nothing in it draws a second one.
            QWidget* row = label->parentWidget();
            QCOMPARE(borderEdgeIn(row, line), QStringLiteral("bottom"));
            for (QLabel* inner : row->findChildren<QLabel*>()) {
                const QString edge = borderEdgeIn(inner, line);
                QVERIFY2(edge.isEmpty(), qPrintable(QStringLiteral("label \"%1\" draws the row's line on its %2 edge")
                                                        .arg(inner->text(), edge)));
            }
            ++rows;
        }
        QVERIFY(rows >= 2);
    }
};

QTEST_MAIN(TstWidgetStyleScoping)
#include "tst_widget_style_scoping.moc"
