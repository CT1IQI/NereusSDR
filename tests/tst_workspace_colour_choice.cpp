// no-port-check: NereusSDR-original test for the workspace preview's two
// colour choices (src/gui/styles/WorkspacePreview StatusTones and
// DataColours) and the call sites that read them.
//
// Covers: both choices default to A (Muted, Navy); with the switch off
// every choice gives the legacy colour byte for byte, HTML text included.
// With it on, Muted gives the semantic roles, Bright the brighter
// indicator roles, Legacy the legacy colours; Navy keeps the data
// backgrounds, Neutral and NeutralInstrument give the container role.
// The title bar dot and latency number, Network Diagnostics' status and
// a status badge follow the status choice; the filter passband, the time
// series graph, the SIGNAL STRENGTH dial's surround and the parametric EQ
// follow the data choice. Under NeutralInstrument a bar meter item's
// factory ink takes the instrument tones, and a colour the user set is
// kept (the user-setting rule).

#include <QtTest/QtTest>

#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>

#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "gui/NetworkDiagnosticsDialog.h"
#include "gui/SMeterWidget.h"
#include "gui/TimeSeriesGraphWidget.h"
#include "gui/TitleBar.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "gui/widgets/StatusBadge.h"

using namespace NereusSDR;
using WorkspacePreview::DataColours;
using WorkspacePreview::StatusTones;

Q_DECLARE_METATYPE(NereusSDR::WorkspacePreview::StatusTones)
Q_DECLARE_METATYPE(NereusSDR::WorkspacePreview::DataColours)

namespace {

QColor role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name));
}

QImage render(QWidget& w)
{
    w.ensurePolished();
    return w.grab().toImage().convertToFormat(QImage::Format_ARGB32);
}

QColor firstOpaquePixel(const QPixmap& pm)
{
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() == 255) {
                return c;
            }
        }
    }
    return {};
}

QPixmap badgeIcon(StatusBadge& badge)
{
    for (QLabel* l : badge.findChildren<QLabel*>()) {
        if (!l->pixmap().isNull()) {
            return l->pixmap();
        }
    }
    return {};
}

int countPixels(const QImage& img, const QRect& r, const QColor& c)
{
    int n = 0;
    const QRect clip = r.intersected(img.rect());
    for (int y = clip.top(); y <= clip.bottom(); ++y) {
        for (int x = clip.left(); x <= clip.right(); ++x) {
            if ((img.pixel(x, y) & 0x00ffffffu) == (c.rgb() & 0x00ffffffu)) {
                ++n;
            }
        }
    }
    return n;
}

QString statusText(NetworkDiagnosticsDialog& dlg)
{
    for (QLabel* l : dlg.findChildren<QLabel*>()) {
        if (l->text().contains(QStringLiteral("Disconnected"))
            && l->text().contains(QStringLiteral("<span"))) {
            return l->text();
        }
    }
    return {};
}

// The colour a status site shows: legacy with the switch off or under
// Legacy, the role under Muted, the bright role under Bright.
QColor expectedStatus(bool on, StatusTones tones, const QColor& legacy, const char* muted,
                      const char* bright)
{
    if (!on || tones == StatusTones::Legacy) {
        return legacy;
    }
    return role(tones == StatusTones::Bright ? bright : muted);
}

QColor expectedData(bool on, DataColours colours, const QColor& legacy, const char* roleName)
{
    if (!on || colours == DataColours::Navy) {
        return legacy;
    }
    return role(roleName);
}

void addToneRows()
{
    QTest::addColumn<bool>("on");
    QTest::addColumn<StatusTones>("tones");
    for (bool on : {false, true}) {
        for (StatusTones t : {StatusTones::Muted, StatusTones::Bright, StatusTones::Legacy}) {
            const char* name = t == StatusTones::Muted ? "muted"
                             : t == StatusTones::Bright ? "bright" : "legacy";
            QTest::addRow("%s %s", on ? "on" : "off", name) << on << t;
        }
    }
}

void addDataRows()
{
    QTest::addColumn<bool>("on");
    QTest::addColumn<DataColours>("colours");
    for (bool on : {false, true}) {
        for (DataColours c : {DataColours::Navy, DataColours::Neutral,
                              DataColours::NeutralInstrument}) {
            const char* name = c == DataColours::Navy ? "navy"
                             : c == DataColours::Neutral ? "neutral" : "instrument";
            QTest::addRow("%s %s", on ? "on" : "off", name) << on << c;
        }
    }
}

// A bar row's BarItem as ItemGroup::buildBarRow builds it, at -10 with
// the red threshold at 0, painted into an image.
QColor barBaseline(const QColor& barColour)
{
    BarItem bar;
    bar.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bar.setOrientation(BarItem::Orientation::Horizontal);
    bar.setRange(-30.0, 12.0);
    bar.setBarStyle(BarItem::BarStyle::Line);
    bar.setBarColor(barColour);
    bar.setBarRedColor(QColor(Qt::red));
    bar.setRedThreshold(0.0);
    bar.clearSmoothing(-10.0);
    QImage img(300, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    bar.paint(p, img.width(), img.height());
    p.end();
    return img.pixelColor(5, 37);
}

QColor scaleTick(const QColor& tickColour)
{
    ScaleItem scale;
    scale.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    scale.setRange(-30.0, 12.0);
    scale.setMajorTicks(2);
    scale.setMinorTicks(0);
    if (tickColour.isValid()) {
        scale.setTickColor(tickColour);
    }
    QImage img(300, 40, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    scale.paint(p, img.width(), img.height());
    p.end();
    return img.pixelColor(0, 2);
}

// A Thetis bar strip (BarPresetItem, the SIGNALAVG variant) with no
// reading, painted on black: the backdrop, the scale (white to the red
// threshold, red past it), the "--" value text in the marker colour and the
// "--" peak text in red. `config` is applied first (user colours).
QImage presetStrip(const QJsonObject& config = {})
{
    BarPresetItem strip;
    strip.configureVariant(QStringLiteral("SignalAvg"));
    strip.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    if (!config.isEmpty()) {
        strip.applyConfiguration(config);
    }
    QImage img(400, 100, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    strip.paint(p, img.width(), img.height());
    p.end();
    return img;
}

} // namespace

class TestWorkspaceColourChoice : public QObject
{
    Q_OBJECT

private slots:
    void cleanup()
    {
        WorkspacePreview::setEnabledForTest(false);
        WorkspacePreview::setStatusTonesForTest(StatusTones::Muted);
        WorkspacePreview::setDataColoursForTest(DataColours::Navy);
    }

    // ── The helpers ─────────────────────────────────────────────────────

    void defaultsAreChoiceA()
    {
        QCOMPARE(WorkspacePreview::statusTones(), StatusTones::Muted);
        QCOMPARE(WorkspacePreview::dataColours(), DataColours::Navy);
    }

    void statusColour_data() { addToneRows(); }
    void statusColour()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        struct Case { QColor legacy; const char* muted; const char* bright; };
        const Case cases[] = {
            {QColor(0x5f, 0xff, 0x8a), "ok", "live"},
            {QColor(0xff, 0xd7, 0x00), "warn", "measured"},
            {QColor(0xff, 0x60, 0x60), "danger", "tx"},
            {QColor(0x5f, 0xa8, 0xff), "accent", "accent"},
        };
        for (const Case& c : cases) {
            const QColor out = WorkspacePreview::statusColour(c.legacy, c.muted);
            QCOMPARE(out.rgba(), expectedStatus(on, tones, c.legacy, c.muted, c.bright).rgba());
            if (!on || tones == StatusTones::Legacy) {
                QCOMPARE(out, c.legacy);
            }
        }
        // A translucent legacy colour keeps its alpha.
        const QColor translucent(0x5f, 0xff, 0x8a, 26);
        QCOMPARE(WorkspacePreview::statusColour(translucent, "ok").alpha(), 26);
    }

    void statusColourName_data() { addToneRows(); }
    void statusColourName()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        const QString out = WorkspacePreview::statusColourName("#3DD068", "ok");
        if (!on || tones == StatusTones::Legacy) {
            QCOMPARE(out, QStringLiteral("#3DD068"));
        } else {
            QCOMPARE(out, role(tones == StatusTones::Bright ? "live" : "ok").name());
        }
    }

    void dataBackground_data() { addDataRows(); }
    void dataBackground()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const QColor legacy(0x0f, 0x0f, 0x1a);
        const QColor out = WorkspacePreview::dataBackground(legacy, "panel");
        QCOMPARE(out.rgba(), expectedData(on, colours, legacy, "panel").rgba());
        const QColor veil(0x0f, 0x0f, 0x1a, 170);
        QCOMPARE(WorkspacePreview::dataBackground(veil, "panel").alpha(), 170);
    }

    void meterInk_data() { addDataRows(); }
    void meterInk()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const bool mapped = on && colours == DataColours::NeutralInstrument;
        const QColor white(Qt::white);
        const QColor out = WorkspacePreview::meterInk(white, {white, QColor(Qt::red)}, "measured");
        QCOMPARE(out.rgba(), mapped ? role("measured").rgba() : white.rgba());
        // The user-setting rule: a colour that is not a factory default
        // is never replaced.
        const QColor user(0x00, 0xff, 0x00);
        QCOMPARE(WorkspacePreview::meterInk(user, {white}, "measured"), user);
        // The history colour keeps its alpha.
        const QColor history(255, 250, 205, 128);
        const QColor h = WorkspacePreview::meterInk(history, {history}, "measured-dim");
        QCOMPARE(h.alpha(), 128);
    }

    // ── Status call sites ───────────────────────────────────────────────

    void connectionDot_data() { addToneRows(); }
    void connectionDot()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        struct Case { ConnectionState state; QColor legacy; const char* muted; const char* bright; };
        const Case cases[] = {
            {ConnectionState::Connected, QColor(0x3f, 0xcf, 0x6a), "ok", "live"},
            {ConnectionState::LinkLost, QColor(0xcf, 0x6c, 0x00), "warn", "measured"},
            {ConnectionState::Disconnected, QColor(0xff, 0x40, 0x40), "danger", "tx"},
        };
        for (const Case& c : cases) {
            ConnectionSegment seg;
            seg.resize(360, 24);
            seg.setState(c.state);
            seg.setPulsePausedForTest(true);
            const QImage img = render(seg);
            const QColor dot = img.pixelColor(13, 12);
            QCOMPARE(dot.rgb(), expectedStatus(on, tones, c.legacy, c.muted, c.bright).rgb());
        }
    }

    void latencyNumber_data() { addToneRows(); }
    void latencyNumber()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        struct Case { int rtt; QColor legacy; const char* muted; const char* bright; };
        const Case cases[] = {
            {12, QColor(0x5f, 0xff, 0x8a), "ok", "live"},
            {80, QColor(0xff, 0xd7, 0x00), "warn", "measured"},
            {250, QColor(0xff, 0x60, 0x60), "danger", "tx"},
        };
        for (const Case& c : cases) {
            ConnectionSegment seg;
            seg.resize(360, 24);
            seg.setState(ConnectionState::Connected);
            seg.setPulsePausedForTest(true);
            seg.setRates(18.4, 0.6);
            seg.setRttMs(c.rtt);
            const QImage img = render(seg);
            const QRect rtt = seg.rttRect();
            QVERIFY(rtt.width() > 0);
            const QColor expected = expectedStatus(on, tones, c.legacy, c.muted, c.bright);
            QVERIFY2(countPixels(img, rtt, expected) > 0,
                     qPrintable(QStringLiteral("rtt %1: no pixel in %2").arg(c.rtt)
                                    .arg(expected.name())));
            if (expected.rgb() != c.legacy.rgb()) {
                QCOMPARE(countPixels(img, rtt, c.legacy), 0);
            }
        }
    }

    void networkStatus_data() { addToneRows(); }
    void networkStatus()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        NetworkDiagnosticsDialog dlg(nullptr, nullptr);
        QTest::qWait(300);
        const QString text = statusText(dlg);
        QVERIFY2(!text.isEmpty(), "no Disconnected status label");
        const QString expected = (!on || tones == StatusTones::Legacy)
            ? QStringLiteral("#ff6060")
            : role(tones == StatusTones::Bright ? "tx" : "danger").name();
        QVERIFY2(text.contains(QStringLiteral("color:%1;").arg(expected)), qPrintable(text));
    }

    void statusBadge_data() { addToneRows(); }
    void statusBadge()
    {
        QFETCH(bool, on);
        QFETCH(StatusTones, tones);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setStatusTonesForTest(tones);
        StatusBadge badge;
        badge.setVariant(StatusBadge::Variant::On);
        badge.setSvgIcon(QStringLiteral(":/icons/badge-mode.svg"));
        const QColor legacy(0x5f, 0xff, 0x8a);
        const QColor expected = expectedStatus(on, tones, legacy, "ok", "live");
        QCOMPARE(firstOpaquePixel(badgeIcon(badge)).rgb(), expected.rgb());

        // Under Muted (and with the switch off) the badge writes today's
        // sheet and the stylesheet pass translates it; under Bright and
        // Legacy it writes its resolved colours and keeps the pass out.
        const bool resolved = on && tones != StatusTones::Muted;
        QCOMPARE(badge.property(WorkspacePreview::kSkipProperty).toBool(), resolved);
        const QString sheet = badge.styleSheet();
        if (resolved) {
            QVERIFY2(sheet.contains(QStringLiteral("color: %1;").arg(expected.name())),
                     qPrintable(sheet));
        } else {
            QVERIFY2(sheet.contains(QStringLiteral("color: #5fff8a;")), qPrintable(sheet));
            QVERIFY2(sheet.contains(QStringLiteral("rgba(95,255,138,26)")), qPrintable(sheet));
        }
    }

    // ── Data call sites ─────────────────────────────────────────────────

    void filterPassband_data() { addDataRows(); }
    void filterPassband()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        FilterPassbandWidget w;
        w.resize(200, 80);
        const QImage img = render(w);
        // Inside the 1 px border.
        QCOMPARE(img.pixelColor(3, 3).rgb(),
                 expectedData(on, colours, QColor(0x0a, 0x0a, 0x18), "panel").rgb());
    }

    void timeSeriesGraph_data() { addDataRows(); }
    void timeSeriesGraph()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        TimeSeriesGraphWidget w(QStringLiteral("RTT"), QStringLiteral("ms"));
        w.resize(300, 160);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(1, img.height() - 2).rgb(),
                 expectedData(on, colours, QColor(0x05, 0x0b, 0x13), "app-bg").rgb());
    }

    void signalStrengthSurround_data() { addDataRows(); }
    void signalStrengthSurround()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        SMeterWidget w;
        w.resize(300, 150);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(0, 0).rgb(),
                 expectedData(on, colours, QColor(0x0f, 0x0f, 0x1a), "panel").rgb());
    }

    void parametricEq_data() { addDataRows(); }
    void parametricEq()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        ParametricEqWidget w;
        w.setEditorPresentationEnabled(true);
        w.resize(500, 300);
        const QImage img = render(w);
        QCOMPARE(img.pixelColor(1, 1).rgb(),
                 expectedData(on, colours, QColor(0x0f, 0x0f, 0x1a), "app-bg").rgb());
    }

    void barMeterInk_data() { addDataRows(); }
    void barMeterInk()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const bool mapped = on && colours == DataColours::NeutralInstrument;
        QCOMPARE(barBaseline(QColor(Qt::white)).rgb(),
                 (mapped ? role("measured") : QColor(Qt::white)).rgb());
        QCOMPARE(scaleTick(QColor()).rgb(),
                 (mapped ? role("text-scale") : QColor(0xc8, 0xd8, 0xe8)).rgb());
    }

    void userColoursWin_data() { addDataRows(); }
    void userColoursWin()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const QColor userBar(0x00, 0xff, 0x00);
        QCOMPARE(barBaseline(userBar).rgb(), userBar.rgb());
        const QColor userTick(0x20, 0x80, 0xff);
        QCOMPARE(scaleTick(userTick).rgb(), userTick.rgb());
    }
    // The Thetis bar strips (BarPresetItem): under Neutral and
    // NeutralInstrument the default (32,32,32) backdrop takes the panel
    // role; under NeutralInstrument the factory ink takes the instrument
    // tones: scale and labels text-scale, over-range danger, the marker
    // colour measured, the peak text text-secondary.
    void barStrip_data() { addDataRows(); }
    void barStrip()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const QImage img = presetStrip();
        QCOMPARE(img.pixelColor(1, 1).rgb(),
                 expectedData(on, colours, QColor(32, 32, 32), "panel").rgb());
        const bool ink = on && colours == DataColours::NeutralInstrument;
        const QColor scaleInk = ink ? role("text-scale") : QColor(Qt::white);
        const QColor overInk = ink ? role("danger") : QColor(Qt::red);
        const QColor markerInk = ink ? role("measured") : QColor(Qt::yellow);
        QVERIFY2(countPixels(img, img.rect(), scaleInk) > 0, qPrintable(scaleInk.name()));
        QVERIFY2(countPixels(img, img.rect(), overInk) > 0, qPrintable(overInk.name()));
        QVERIFY2(countPixels(img, img.rect(), markerInk) > 0, qPrintable(markerInk.name()));
        if (ink) {
            QCOMPARE(countPixels(img, img.rect(), QColor(Qt::white)), 0);
            QCOMPARE(countPixels(img, img.rect(), QColor(Qt::red)), 0);
            QCOMPARE(countPixels(img, img.rect(), QColor(Qt::yellow)), 0);
        }
    }

    void barStripUserColours_data() { addDataRows(); }
    void barStripUserColours()
    {
        QFETCH(bool, on);
        QFETCH(DataColours, colours);
        WorkspacePreview::setEnabledForTest(on);
        WorkspacePreview::setDataColoursForTest(colours);
        const QColor userBackdrop(0x2a, 0x1e, 0x14);
        const QColor userBar(0x00, 0xff, 0x00);
        const QImage img = presetStrip({{QStringLiteral("backdropColor"), userBackdrop.name()},
                                        {QStringLiteral("barColor"), userBar.name()}});
        QCOMPARE(img.pixelColor(1, 1).rgb(), userBackdrop.rgb());
        QVERIFY(countPixels(img, img.rect(), userBar) > 0);
    }
};

QTEST_MAIN(TestWorkspaceColourChoice)
#include "tst_workspace_colour_choice.moc"
