// no-port-check: NereusSDR-original visual preview tool; scene table.
// =================================================================
// tests/visual/PreviewScenes.cpp  (NereusSDR)
// =================================================================
//
// See PreviewScenes.h. Every baseline scene shows the current app as it
// is; nothing here changes how a widget looks. Transmit appearance comes
// from display-only seams (MoxDisplayController, SpectrumWidget,
// SMeterWidget, MeterPoller); nothing calls MoxController::setMox or
// reaches a RadioConnection (a Deferred-startup window has none, and the
// transmit scene refuses to run if one exists).
//
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "PreviewScenes.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>

#include <cmath>

#include "PreviewData.h"
#include "core/WdspTypes.h"
#include "core/meters/SliceMeterPump.h"
#include "gui/DiversityDialog.h"
#include "gui/MainWindow.h"
#include "gui/MoxDisplayController.h"
#include "gui/PanLayoutDialog.h"
#include "gui/PanadapterApplet.h"
#include "gui/PsForm.h"
#include "gui/SMeterWidget.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerTypes.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

namespace NereusSDR::Preview {

namespace {

constexpr int kSettleMs = 900;
constexpr int kDialogSettleMs = 900;
constexpr int kFinalSettleMs = 400;
constexpr int kNeedleSettleMaxMs = 4000;

// Levels the transmit scene poses.
constexpr float kTxForwardWatts = 85.0f;
constexpr float kTxSwr = 1.25f;
constexpr float kTxMicDb = -12.0f;
// The transmit display on pan 1: slice A's carrier, the default transmit
// filter (TransmitModel 100..2900 Hz) and illustrative levels.
constexpr double kTwoBandCarrierHz = 7185000.0;
constexpr int kTxFilterLowHz = 100;
constexpr int kTxFilterHighHz = 2900;
constexpr double kTxPeakDbm = -12.0;
constexpr double kTxFloorDbm = -100.0;
constexpr quint32 kTxSeed = 0x7E11u;

// Displayed noise floors per band (the median of the settled trace), as a
// live pan reads them on the default -60 to -115 dBm scale.
constexpr double kNoise80mDbm = -100.0;
constexpr double kNoise40mDbm = -104.0;
constexpr double kNoise20mDbm = -110.0;
constexpr double kNoise15mDbm = -114.0;

// ---- small helpers ---------------------------------------------------------

PanadapterApplet* appletFor(MainWindow* window, const QString& panId)
{
    for (PanadapterApplet* applet : window->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == panId) {
            return applet;
        }
    }
    return nullptr;
}

SpectrumWidget* spectrumFor(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    return applet != nullptr ? applet->spectrumWidget() : nullptr;
}

SliceModel* sliceOnPan(MainWindow* window, const QString& panId)
{
    PanadapterApplet* applet = appletFor(window, panId);
    if (applet == nullptr || window->radioModel() == nullptr) {
        return nullptr;
    }
    return window->radioModel()->sliceById(applet->activeSliceIndex());
}

QString panIdAt(int index)
{
    return QStringLiteral("pan-%1").arg(index);
}

QImage grabWidget(QWidget* widget)
{
    return widget->grab().toImage();
}

QRect deviceRect(const QRect& logical, double dpr)
{
    return QRect(static_cast<int>(std::lround(logical.x() * dpr)),
                 static_cast<int>(std::lround(logical.y() * dpr)),
                 static_cast<int>(std::lround(logical.width() * dpr)),
                 static_cast<int>(std::lround(logical.height() * dpr)));
}

QString shapeName(SignalShape shape)
{
    switch (shape) {
    case SignalShape::Usb: return QStringLiteral("USB");
    case SignalShape::Lsb: return QStringLiteral("LSB");
    case SignalShape::Cw:  return QStringLiteral("CW");
    case SignalShape::Ft8: return QStringLiteral("FT8");
    }
    return QString();
}

// ---- pan and slice content ------------------------------------------------

struct PanPose {
    QString panId;
    double sliceHz{0.0};
    DSPMode mode{DSPMode::USB};
    int filterLo{150};
    int filterHi{2850};
    double sMeterDbm{-140.0};
    PanSpectrum spectrum;
};

PanSpectrum twentyMetres(const QString& panId)
{
    PanSpectrum s;
    s.panId = panId;
    s.noiseDbm = kNoise20mDbm;
    s.seed = 20;
    s.signalList = {
        {14195000.0, -88.0, SignalShape::Usb},
        {14210000.0, -95.0, SignalShape::Usb},
        {14225000.0, -81.0, SignalShape::Usb},
        {14243000.0, -79.0, SignalShape::Usb},
        {14262000.0, -100.0, SignalShape::Usb},
        {14290000.0, -90.0, SignalShape::Usb},
    };
    return s;
}

PanSpectrum fortyMetresPhone(const QString& panId)
{
    PanSpectrum s;
    s.panId = panId;
    s.noiseDbm = kNoise40mDbm;
    s.seed = 40;
    s.signalList = {
        {7160000.0, -78.0, SignalShape::Lsb},
        {7172000.0, -92.0, SignalShape::Lsb},
        {7185000.0, -73.0, SignalShape::Lsb},
        {7198000.0, -85.0, SignalShape::Lsb},
        {7215000.0, -101.0, SignalShape::Lsb},
    };
    return s;
}

QVector<PanPose> twoBandPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 7185000.0;
    a.mode = DSPMode::LSB;
    a.filterLo = -2850;
    a.filterHi = -150;
    a.sMeterDbm = -73.0;
    a.spectrum = fortyMetresPhone(a.panId);
    PanPose b;
    b.panId = panIdAt(1);
    b.sliceHz = 14225000.0;
    b.mode = DSPMode::USB;
    b.filterLo = 150;
    b.filterHi = 2850;
    b.sMeterDbm = -81.0;
    b.spectrum = twentyMetres(b.panId);
    return {a, b};
}

QVector<PanPose> fourRxPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 3790000.0;
    a.mode = DSPMode::LSB;
    a.filterLo = -2850;
    a.filterHi = -150;
    a.sMeterDbm = -84.0;
    a.spectrum.panId = a.panId;
    a.spectrum.noiseDbm = kNoise80mDbm;
    a.spectrum.seed = 80;
    a.spectrum.signalList = {
        {3760000.0, -90.0, SignalShape::Lsb},
        {3775000.0, -97.0, SignalShape::Lsb},
        {3790000.0, -84.0, SignalShape::Lsb},
        {3805000.0, -79.0, SignalShape::Lsb},
    };

    PanPose b;
    b.panId = panIdAt(1);
    b.sliceHz = 7074000.0;
    b.mode = DSPMode::DIGU;
    b.filterLo = 100;
    b.filterHi = 3100;
    b.spectrum.panId = b.panId;
    b.spectrum.noiseDbm = kNoise40mDbm;
    b.spectrum.seed = 74;
    // 14 FT8 tones between 7.0742 and 7.0768 MHz, -95 to -70 dBm.
    constexpr int kTones = 14;
    constexpr double kFirstToneHz = 7074200.0;
    constexpr double kLastToneHz = 7076800.0;
    const double levels[kTones] = {-88, -95, -76, -91, -70, -84, -93,
                                   -79, -86, -73, -90, -82, -94, -77};
    double strongest = -1000.0;
    for (int i = 0; i < kTones; ++i) {
        const double hz = kFirstToneHz + (kLastToneHz - kFirstToneHz) * i / (kTones - 1);
        b.spectrum.signalList.append({hz, levels[i], SignalShape::Ft8});
        strongest = std::max(strongest, levels[i]);
    }
    b.sMeterDbm = strongest;

    PanPose c;
    c.panId = panIdAt(2);
    c.sliceHz = 14030000.0;
    c.mode = DSPMode::CWU;
    c.filterLo = 400;
    c.filterHi = 900;
    c.sMeterDbm = -76.0;
    c.spectrum.panId = c.panId;
    c.spectrum.noiseDbm = kNoise20mDbm;
    c.spectrum.seed = 14;
    c.spectrum.signalList = {
        {14012000.0, -88.0, SignalShape::Cw},
        {14021000.0, -101.0, SignalShape::Cw},
        {14030000.0, -76.0, SignalShape::Cw},
        {14038000.0, -93.0, SignalShape::Cw},
    };

    PanPose d;
    d.panId = panIdAt(3);
    d.sliceHz = 21300000.0;
    d.mode = DSPMode::USB;
    d.filterLo = 150;
    d.filterHi = 2850;
    d.sMeterDbm = -83.0;
    d.spectrum.panId = d.panId;
    d.spectrum.noiseDbm = kNoise15mDbm;
    d.spectrum.seed = 21;
    d.spectrum.signalList = {
        {21285000.0, -96.0, SignalShape::Usb},
        {21300000.0, -83.0, SignalShape::Usb},
        {21318000.0, -91.0, SignalShape::Usb},
    };
    return {a, b, c, d};
}

QVector<PanPose> defaultPoses()
{
    PanPose a;
    a.panId = panIdAt(0);
    a.sliceHz = 14225000.0;
    a.mode = DSPMode::USB;
    a.filterLo = 150;
    a.filterHi = 2850;
    a.sMeterDbm = -81.0;
    a.spectrum = twentyMetres(a.panId);
    return {a};
}

// Tunes each pan's slice. Returns false (with ctx.failure) on a missing pan.
bool tuneSlices(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex)
{
    for (const PanPose& pose : poses) {
        SliceModel* slice = sliceOnPan(ctx.window, pose.panId);
        if (slice == nullptr) {
            ctx.failure = QStringLiteral("no slice on %1").arg(pose.panId);
            return false;
        }
        slice->setDspMode(pose.mode);
        slice->setFrequency(pose.sliceHz);
        slice->setFilter(pose.filterLo, pose.filterHi);
    }
    if (SliceModel* active = sliceOnPan(ctx.window, poses.value(activePanIndex).panId)) {
        ctx.model->setActiveSlice(active->sliceIndex());
    }
    pump(kSettleMs);
    return true;
}

// Holds every live readout and timer still. Runs after the scene's own
// state is set and before the data is fed.
void freeze(SceneContext& ctx)
{
    if (auto* poller = ctx.window->findChild<MeterPoller*>()) {
        poller->stop();
        ctx.frozen.append(QStringLiteral("meter-poller: stopped"));
    }
    // The RadioModel-owned pump writes every slice's meter readings (and
    // clears them to "no reading" with no DSP running).
    if (SliceMeterPump* pump = ctx.model->sliceMeterPump()) {
        pump->stop();
        ctx.frozen.append(QStringLiteral("slice-meter-pump: stopped"));
    }
    for (const QJsonValue& v : stopTimersUnder(ctx.window)) {
        ctx.stoppedTimers.append(v);
    }
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != ctx.window && top->isVisible()) {
            for (const QJsonValue& v : stopTimersUnder(top)) {
                ctx.stoppedTimers.append(v);
            }
        }
    }
    for (const QJsonValue& v : freezeReadouts(ctx.window)) {
        ctx.frozen.append(v);
    }
}

// Every S-meter: the main window's and any in a floating container.
QList<SMeterWidget*> allMeters(SceneContext& ctx)
{
    QList<SMeterWidget*> meters = ctx.window->findChildren<SMeterWidget*>();
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (top != ctx.window && top->isVisible()) {
            meters += top->findChildren<SMeterWidget*>();
        }
    }
    return meters;
}

// Sets every slice flag's meter and every S-meter needle (to the active
// pan's level).
void poseMeters(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex)
{
    for (const PanPose& pose : poses) {
        if (SliceModel* slice = sliceOnPan(ctx.window, pose.panId)) {
            slice->setSignalStrengthDbm(pose.sMeterDbm);
        }
    }
    for (SMeterWidget* meter : allMeters(ctx)) {
        meter->setLevel(static_cast<float>(poses.value(activePanIndex).sMeterDbm));
    }
}

// Waits for every S-meter needle to reach its target.
void waitForNeedle(SceneContext& ctx)
{
    const QList<SMeterWidget*> meters = allMeters(ctx);
    if (meters.isEmpty()) {
        ctx.notes.append(QStringLiteral("no SMeterWidget found"));
        return;
    }
    auto settled = [&meters]() {
        for (SMeterWidget* meter : meters) {
            if (std::abs(meter->testNeedleFraction() - meter->testNeedleTarget()) > 1e-6f) {
                return false;
            }
        }
        return true;
    };
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < kNeedleSettleMaxMs && !settled()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    if (!settled()) {
        ctx.unfrozen.append(QJsonObject{
            {QStringLiteral("item"), QStringLiteral("s-meter needle")},
            {QStringLiteral("region"), QStringLiteral("right panel, S-Meter")},
            {QStringLiteral("reason"), QStringLiteral("needle did not reach its target in time")}});
    }
}

void feedSpectra(SceneContext& ctx, const QVector<PanPose>& poses)
{
    for (const PanPose& pose : poses) {
        SpectrumWidget* sw = spectrumFor(ctx.window, pose.panId);
        if (sw == nullptr) {
            ctx.failure = QStringLiteral("no spectrum on %1").arg(pose.panId);
            return;
        }
        const SpectrumCheck c = feedPan(sw, pose.spectrum);
        QJsonArray sigs;
        for (const SignalSpec& s : pose.spectrum.signalList) {
            sigs.append(QJsonObject{{QStringLiteral("hz"), s.hz},
                                    {QStringLiteral("dbm"), s.dbm},
                                    {QStringLiteral("shape"), shapeName(s.shape)}});
        }
        const QJsonObject check{
            {QStringLiteral("pan"), c.panId},
            {QStringLiteral("target_hz"), c.targetHz},
            {QStringLiteral("target_dbm"), c.targetDbm},
            {QStringLiteral("displayed_dbm"), std::round(c.displayedDbm * 100.0) / 100.0},
            {QStringLiteral("within_1db"), std::abs(c.displayedDbm - c.targetDbm) <= 1.0},
            {QStringLiteral("centre_hz"), c.centreHz},
            {QStringLiteral("span_hz"), c.spanHz},
            {QStringLiteral("ddc_centre_hz"), c.ddcCentreHz},
            {QStringLiteral("sample_rate_hz"), c.sampleRateHz},
            {QStringLiteral("ref_level_dbm"), sw->refLevel()},
            {QStringLiteral("dynamic_range_db"), sw->dynamicRange()},
            {QStringLiteral("waterfall_rows_pushed"), c.waterfallRows},
            {QStringLiteral("waterfall_low_dbm"), sw->wfActiveLowThreshold()},
            {QStringLiteral("waterfall_high_dbm"), sw->wfActiveHighThreshold()},
            {QStringLiteral("waterfall_agc"), sw->wfAgcEnabled()},
            {QStringLiteral("noise_target_dbm"), c.noiseTargetDbm},
            {QStringLiteral("noise_displayed_median_dbm"), std::round(c.noiseDisplayedDbm * 100.0) / 100.0},
            {QStringLiteral("noise_bin_mean_offset_db"), std::round(c.noiseOffsetDb * 100.0) / 100.0},
            {QStringLiteral("waterfall_row_period_ms"), c.rowPeriodMs},
            {QStringLiteral("signals"), sigs}};
        ctx.checks.append(check);
        qInfo("check %s: target %.0f Hz %.1f dBm, displayed %.2f dBm (centre %.0f span %.0f ddc %.0f rate %.0f, "
              "waterfall %.1f..%.1f dBm, noise median %.1f for %.1f)",
              qPrintable(c.panId), c.targetHz, c.targetDbm, c.displayedDbm, c.centreHz,
              c.spanHz, c.ddcCentreHz, c.sampleRateHz, sw->wfActiveLowThreshold(),
              sw->wfActiveHighThreshold(), c.noiseDisplayedDbm, c.noiseTargetDbm);
    }
}

void addCrop(QVector<CropRect>& crops, const QString& name, QWidget* window, QWidget* w, double dpr)
{
    // A widget in another top-level (the floating Main Panel) is not in
    // this image.
    if (w == nullptr || !w->isVisible() || !window->isAncestorOf(w)) {
        return;
    }
    const QRect logical(w->mapTo(window, QPoint(0, 0)), w->size());
    const QRect clipped = logical.intersected(QRect(QPoint(0, 0), window->size()));
    if (clipped.isEmpty()) {
        return;
    }
    crops.append({name, clipped, deviceRect(clipped, dpr)});
}

VfoWidget* flagForSlice(MainWindow* window, SliceModel* slice)
{
    for (VfoWidget* flag : window->findChildren<VfoWidget*>()) {
        if (flag->sliceForTest() == slice && flag->isVisible()) {
            return flag;
        }
    }
    return nullptr;
}

// One line per visible widget under `root`: region, a path of class
// names with sibling indexes, and the rectangle relative to `root`.
void dumpGeometry(QWidget* root, const QString& region, QStringList& lines)
{
    if (root == nullptr) {
        lines << region + QStringLiteral(" <missing>");
        return;
    }
    std::function<void(QWidget*, const QString&)> walk = [&](QWidget* w, const QString& path) {
        if (!w->isVisible()) {
            return;
        }
        const QRect r(w->mapTo(root, QPoint(0, 0)), w->size());
        lines << QStringLiteral("%1 %2 %3,%4 %5x%6")
                     .arg(region, path)
                     .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
        int index = 0;
        for (QObject* o : w->children()) {
            if (!o->isWidgetType()) {
                continue;
            }
            auto* child = static_cast<QWidget*>(o);
            walk(child, path + QLatin1Char('/') + QString::fromLatin1(child->metaObject()->className())
                            + QLatin1Char('[') + QString::number(index) + QLatin1Char(']'));
            ++index;
        }
    };
    walk(root, QString::fromLatin1(root->metaObject()->className()));
}

// Grabs the whole window and, at the crop profile, its detail crops.
void captureWindow(SceneContext& ctx, const QString& name)
{
    if (ctx.beforeGrab) {
        ctx.beforeGrab(ctx.window);
        pump(kFinalSettleMs);
    }
    CapturedImage full;
    full.name = name;
    full.kind = QStringLiteral("window");
    full.image = grabWidget(ctx.window);
    full.logicalSize = ctx.window->size();
    if (!ctx.geometryDump.isEmpty()) {
        QStringList lines;
        auto* manager = ctx.window->findChild<ContainerManager*>();
        dumpGeometry(manager != nullptr ? manager->panelContainer() : nullptr,
                     QStringLiteral("right-panel"), lines);
        SpectrumWidget* pan = spectrumFor(ctx.window, panIdAt(0));
        dumpGeometry(pan != nullptr ? pan->findChild<SpectrumOverlayPanel*>() : nullptr,
                     QStringLiteral("pan1-toolbar"), lines);
        QFile f(ctx.geometryDump);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(lines.join(QLatin1Char('\n')).toUtf8());
            f.write("\n");
        } else {
            ctx.notes.append(QStringLiteral("could not write the geometry dump"));
        }
    }
    full.dpr = full.image.devicePixelRatio();
    if (ctx.crops) {
        SliceModel* sliceA = ctx.model->sliceById(0);
        addCrop(full.crops, QStringLiteral("flag"), ctx.window, flagForSlice(ctx.window, sliceA), full.dpr);
        auto* manager = ctx.window->findChild<ContainerManager*>();
        addCrop(full.crops, QStringLiteral("right-panel"), ctx.window,
                manager != nullptr ? manager->panelContainer() : nullptr, full.dpr);
        addCrop(full.crops, QStringLiteral("top-strip"), ctx.window,
                ctx.window->findChild<TitleBar*>(), full.dpr);
        addCrop(full.crops, QStringLiteral("status-bar"), ctx.window, ctx.window->statusBar(), full.dpr);
        SpectrumWidget* pan1 = spectrumFor(ctx.window, panIdAt(0));
        addCrop(full.crops, QStringLiteral("pan1-toolbar"), ctx.window,
                pan1 != nullptr ? pan1->findChild<SpectrumOverlayPanel*>() : nullptr, full.dpr);
    }
    ctx.images.append(full);
    for (const CropRect& crop : full.crops) {
        CapturedImage c;
        c.name = name + QStringLiteral("--") + crop.name;
        c.kind = QStringLiteral("crop");
        c.image = full.image.copy(crop.device);
        c.image.setDevicePixelRatio(full.dpr);
        c.logicalSize = crop.logical.size();
        c.dpr = full.dpr;
        c.crops = {crop};
        c.extra[QStringLiteral("source")] = name;
        ctx.images.append(c);
        CapturedImage big;
        big.name = c.name + QStringLiteral("@3x");
        big.kind = QStringLiteral("crop-3x");
        big.image = c.image.scaled(c.image.width() * 3, c.image.height() * 3,
                                   Qt::IgnoreAspectRatio, Qt::FastTransformation);
        big.image.setDevicePixelRatio(full.dpr);
        big.logicalSize = crop.logical.size() * 3;
        big.dpr = full.dpr;
        big.crops = {crop};
        big.extra[QStringLiteral("source")] = name;
        big.extra[QStringLiteral("enlargement")] = QStringLiteral("3x nearest-neighbour");
        ctx.images.append(big);
    }
}

void recordStandardNotShown(SceneContext& ctx)
{
    const QString reason = QStringLiteral(
        "needs a device setting that starts discovery or a device connection");
    for (const QString& item : {QStringLiteral("applet:amp (Power Genius / Amplifier)"),
                                QStringLiteral("applet:tuner (Tuner Genius)"),
                                QStringLiteral("applet:RfKit (RF-Kit)")}) {
        ctx.notShown.append(QJsonObject{{QStringLiteral("item"), item},
                                        {QStringLiteral("reason"), reason}});
    }
}

// The common main-window pose: tune, scene hook, freeze, meters, data.
void runWindowScene(SceneContext& ctx, const QVector<PanPose>& poses, int activePanIndex,
                    const std::function<void(SceneContext&)>& beforeFreeze = {},
                    const std::function<void(SceneContext&)>& afterFeed = {})
{
    recordStandardNotShown(ctx);
    if (!tuneSlices(ctx, poses, activePanIndex)) {
        return;
    }
    if (beforeFreeze) {
        beforeFreeze(ctx);
        if (!ctx.failure.isEmpty()) {
            return;
        }
        pump(kSettleMs);
    }
    freeze(ctx);
    poseMeters(ctx, poses, activePanIndex);
    feedSpectra(ctx, poses);
    if (!ctx.failure.isEmpty()) {
        return;
    }
    if (afterFeed) {
        afterFeed(ctx);
    }
    waitForNeedle(ctx);
    pump(kFinalSettleMs);
}

ContainerWidget* mainPanel(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    return manager != nullptr ? manager->panelContainer() : nullptr;
}

void sceneHeaderMode(SceneContext& ctx, HeaderMode mode, const QString& name)
{
    runWindowScene(ctx, twoBandPoses(), 0, [mode](SceneContext& c) {
        ContainerWidget* panel = mainPanel(c);
        if (panel == nullptr) {
            c.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The context menu's Header > Always / Reveal / Hidden path.
        emit panel->headerModeRequested(mode);
    });
    if (ctx.failure.isEmpty()) {
        captureWindow(ctx, name);
    }
}

// ---- dialogs ---------------------------------------------------------------

template <typename T>
T* visibleTopLevel()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* t = qobject_cast<T*>(w); t != nullptr && t->isVisible()) {
            return t;
        }
    }
    return nullptr;
}

void captureDialog(SceneContext& ctx, QWidget* dialog, const QString& name)
{
    if (dialog == nullptr) {
        ctx.failure = QStringLiteral("%1: the dialog did not open").arg(name);
        return;
    }
    pump(kDialogSettleMs);
    for (const QJsonValue& v : stopTimersUnder(dialog)) {
        ctx.stoppedTimers.append(v);
    }
    pump(kFinalSettleMs);
    if (ctx.beforeGrab) {
        ctx.beforeGrab(dialog);
        pump(kFinalSettleMs);
    }
    CapturedImage img;
    img.name = name;
    img.kind = QStringLiteral("dialog");
    img.image = grabWidget(dialog);
    img.logicalSize = dialog->size();
    img.dpr = img.image.devicePixelRatio();
    img.extra[QStringLiteral("widget_class")] = QString::fromLatin1(dialog->metaObject()->className());
    img.extra[QStringLiteral("window_title")] = dialog->windowTitle();
    ctx.images.append(img);
}

// Opens a modal dialog through `open` (which blocks in exec()), grabs it from
// a deferred timer, then rejects it so `open` returns.
template <typename T>
void captureModal(SceneContext& ctx, const QString& name, const std::function<void()>& open)
{
    bool done = false;
    QTimer::singleShot(0, ctx.window, [&ctx, &done, name] {
        QElapsedTimer t;
        t.start();
        T* dialog = nullptr;
        while (dialog == nullptr && t.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            dialog = visibleTopLevel<T>();
        }
        captureDialog(ctx, dialog, name);
        if (dialog != nullptr) {
            dialog->reject();
        }
        done = true;
    });
    open();
    if (!done) {
        ctx.failure = QStringLiteral("%1: the modal dialog never opened").arg(name);
    }
}

void prepareForDialog(SceneContext& ctx)
{
    // Dialog scenes run on the two-band window so slice-bound pages have a
    // tuned slice; the window itself is not captured.
    tuneSlices(ctx, twoBandPoses(), 0);
    if (auto* poller = ctx.window->findChild<MeterPoller*>()) {
        poller->stop();
    }
}

QString slug(const QString& text)
{
    QString s = text.toLower();
    s.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    s.remove(QRegularExpression(QStringLiteral("^-+|-+$")));
    return s;
}

struct SetupLeaf {
    int index{-1};
    QString label;
    QString category;
    bool hidden{false};
};

SetupDialog* openSetupDialog(MainWindow* window)
{
    SetupDialog* created = nullptr;
    const auto conn = QObject::connect(window, &MainWindow::setupDialogCreated,
                                       [&created](SetupDialog* d) { created = d; });
    QMetaObject::invokeMethod(window, "openSetupAtPage", Qt::DirectConnection,
                              Q_ARG(QString, QString()));
    QObject::disconnect(conn);
    if (created == nullptr) {
        created = visibleTopLevel<SetupDialog>();
    }
    return created;
}

QVector<SetupLeaf> setupLeaves(SetupDialog* dialog)
{
    QVector<SetupLeaf> leaves;
    auto* tree = dialog->findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return leaves;
    }
    const QStringList labels = dialog->pageLabelsForTest();
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = tree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem* leaf = category->child(j);
            const int index = leaf->data(0, Qt::UserRole).toInt();
            if (index < 0 || index >= labels.size()) {
                continue;
            }
            leaves.append({index, labels.at(index), category->text(0),
                           leaf->isHidden() || category->isHidden()});
        }
    }
    return leaves;
}

QTreeWidgetItem* treeItemForIndex(QTreeWidget* tree, int index)
{
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* category = tree->topLevelItem(i);
        for (int j = 0; j < category->childCount(); ++j) {
            if (category->child(j)->data(0, Qt::UserRole).toInt() == index) {
                return category->child(j);
            }
        }
    }
    return nullptr;
}

} // namespace

void pump(int ms)
{
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
}

QVector<Scene> fixedScenes()
{
    QVector<Scene> scenes;

    scenes.append({QStringLiteral("baseline-default"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, defaultPoses(), 0);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-default")); }
    }});

    scenes.append({QStringLiteral("baseline-two-band"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-two-band")); }
    }});

    scenes.append({QStringLiteral("baseline-four-rx"), QStringLiteral("2x2"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, fourRxPoses(), 2);
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-four-rx")); }
    }});

    scenes.append({QStringLiteral("baseline-tx"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // Display-only transmit state on slice A. Refuse to run if a
        // connection exists: these seams must never reach a radio.
        if (ctx.model->connection() != nullptr) {
            ctx.failure = QStringLiteral("a RadioConnection exists; refusing the transmit scene");
            return;
        }
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            SliceModel* sliceA = c.model->sliceById(0);
            if (auto* mox = c.window->findChild<MoxDisplayController*>()) {
                mox->setKeyed(true, sliceA != nullptr ? sliceA->sliceIndex() : 0);
                c.notes.append(QStringLiteral("MoxDisplayController::setKeyed(true, slice A)"));
            } else {
                c.notShown.append(QJsonObject{
                    {QStringLiteral("item"), QStringLiteral("MoxDisplayController keyed overlay")},
                    {QStringLiteral("reason"), QStringLiteral(
                        "a Deferred-startup local window builds no MoxDisplayController "
                        "(it needs the TX analyzer)")}});
            }
            if (SpectrumWidget* sw = spectrumFor(c.window, panIdAt(0))) {
                sw->setTxActiveForTest(true);
                c.notes.append(QStringLiteral("SpectrumWidget::setTxActiveForTest(true) on pan-0"));
            }
            if (auto* poller = c.window->findChild<MeterPoller*>()) {
                poller->stop();
                poller->setInTx(true);
                c.notes.append(QStringLiteral("MeterPoller::stop(), setInTx(true)"));
            }
        }, [](SceneContext& c) {
            // Pan 1 transmits: the transmit trace and waterfall a local
            // window's LocalTxDisplaySource would draw, slice A's LSB at
            // the default transmit filter 100..2900 Hz.
            if (SpectrumWidget* sw = spectrumFor(c.window, panIdAt(0))) {
                feedTxPan(sw, kTwoBandCarrierHz, false, kTxFilterLowHz, kTxFilterHighHz,
                          kTxPeakDbm, kTxFloorDbm, kTxSeed);
                c.notes.append(QStringLiteral(
                    "pan-0 transmit display: updateSpectrumFromTxPixels and pushTxWaterfallRow, "
                    "LSB voice 100..2900 Hz, peak %1 dBm over %2 dBm").arg(kTxPeakDbm).arg(kTxFloorDbm));
            }
            for (SMeterWidget* meter : allMeters(c)) {
                meter->setTransmitting(true);
                meter->setTxMeters(kTxForwardWatts, kTxSwr);
                meter->setMicMeters(kTxMicDb, 0.0f, kTxMicDb, -30.0f);
                c.notes.append(QStringLiteral("SMeterWidget: transmitting, 85 W, SWR 1.25, mic -12 dB"));
            }
            c.notShown.append(QJsonObject{
                {QStringLiteral("item"), QStringLiteral("ALC -2 dB")},
                {QStringLiteral("reason"), QStringLiteral(
                    "SMeterWidget has no ALC input and the default panel has no ALC meter")}});
        });
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-tx")); }
    }});

    scenes.append({QStringLiteral("baseline-arrange"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            if (manager == nullptr || manager->arrangeController() == nullptr) {
                c.failure = QStringLiteral("no ContainerArrangeController");
                return;
            }
            c.notes.append(QStringLiteral(
                "The current app has no separate arrange mode: ContainerManager sets its "
                "ContainerArrangeController on every content host whenever a workspace store "
                "exists, so the entry grips show in every baseline scene."));
        });
        if (ctx.failure.isEmpty()) { captureWindow(ctx, QStringLiteral("baseline-arrange")); }
    }});

    scenes.append({QStringLiteral("baseline-header-always"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Always, QStringLiteral("baseline-header-always"));
    }});
    scenes.append({QStringLiteral("baseline-header-reveal"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Reveal, QStringLiteral("baseline-header-reveal"));
    }});
    scenes.append({QStringLiteral("baseline-header-hidden"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneHeaderMode(ctx, HeaderMode::Hidden, QStringLiteral("baseline-header-hidden"));
    }});

    scenes.append({QStringLiteral("baseline-floating"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* panel = mainPanel(c);
            if (manager == nullptr || panel == nullptr) {
                c.failure = QStringLiteral("no Main Panel container");
                return;
            }
            manager->floatContainer(panel->id());
        });
        if (!ctx.failure.isEmpty()) { return; }
        captureWindow(ctx, QStringLiteral("baseline-floating"));
        auto* floating = visibleTopLevel<FloatingContainer>();
        if (floating == nullptr) {
            ctx.failure = QStringLiteral("the Main Panel did not float");
            return;
        }
        CapturedImage img;
        img.name = QStringLiteral("baseline-floating--floating");
        img.kind = QStringLiteral("floating");
        img.image = grabWidget(floating);
        img.logicalSize = floating->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("window_title")] = floating->windowTitle();
        ctx.images.append(img);
    }});

    // ---- fixed dialogs ----
    scenes.append({QStringLiteral("dialog-tx-eq"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* action = ctx.window->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"));
        if (action == nullptr) {
            ctx.failure = QStringLiteral("no toolsTxEqualizer action");
            return;
        }
        action->trigger();
        captureDialog(ctx, visibleTopLevel<TxEqDialog>(), QStringLiteral("dialog-tx-eq"));
    }});
    scenes.append({QStringLiteral("dialog-tx-cfc"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* tx = ctx.window->findChild<TxApplet*>();
        if (tx == nullptr) {
            ctx.failure = QStringLiteral("no TxApplet");
            return;
        }
        tx->requestOpenCfcDialog();
        captureDialog(ctx, visibleTopLevel<TxCfcDialog>(), QStringLiteral("dialog-tx-cfc"));
    }});
    scenes.append({QStringLiteral("dialog-container-settings"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        auto* manager = ctx.window->findChild<ContainerManager*>();
        ContainerWidget* panel = mainPanel(ctx);
        if (manager == nullptr || panel == nullptr) {
            ctx.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The Containers > Edit Container path (MainWindow.cpp:11138), shown
        // instead of exec()'d so the grab needs no nested loop.
        ContainerSettingsDialog dialog(panel, ctx.window, manager);
        dialog.show();
        captureDialog(ctx, &dialog, QStringLiteral("dialog-container-settings"));
        dialog.reject();
    }});
    scenes.append({QStringLiteral("dialog-pan-layout"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        captureModal<PanLayoutDialog>(ctx, QStringLiteral("dialog-pan-layout"), [&ctx] {
            QMetaObject::invokeMethod(ctx.window, "showPanLayoutDialog", Qt::DirectConnection);
        });
    }});
    scenes.append({QStringLiteral("dialog-pure-signal"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        QMetaObject::invokeMethod(ctx.window, "openPureSignalDialog", Qt::DirectConnection);
        captureDialog(ctx, visibleTopLevel<PsForm>(), QStringLiteral("dialog-pure-signal"));
    }});
    scenes.append({QStringLiteral("dialog-diversity"), QStringLiteral("2v"), true,
                   [](SceneContext& ctx) {
        prepareForDialog(ctx);
        QMetaObject::invokeMethod(ctx.window, "openDiversityDialog", Qt::DirectConnection);
        captureDialog(ctx, visibleTopLevel<DiversityDialog>(), QStringLiteral("dialog-diversity"));
    }});
    return scenes;
}

QVector<Scene> setupPageScenes(MainWindow* window)
{
    QVector<Scene> scenes;
    SetupDialog* dialog = openSetupDialog(window);
    if (dialog == nullptr) {
        return scenes;
    }
    const QVector<SetupLeaf> leaves = setupLeaves(dialog);
    QHash<QString, int> labelCounts;
    for (const SetupLeaf& leaf : leaves) {
        labelCounts[leaf.label] += 1;
    }
    for (const SetupLeaf& leaf : leaves) {
        const QString stem = labelCounts.value(leaf.label) > 1
            ? slug(leaf.category) + QLatin1Char('-') + slug(leaf.label)
            : slug(leaf.label);
        const QString name = QStringLiteral("dialog-setup-") + stem;
        const int index = leaf.index;
        const bool hidden = leaf.hidden;
        const QString label = leaf.label;
        const QString category = leaf.category;
        scenes.append({name, QStringLiteral("2v"), true,
                       [name, index, hidden, label, category](SceneContext& ctx) {
            prepareForDialog(ctx);
            SetupDialog* d = openSetupDialog(ctx.window);
            if (d == nullptr) {
                ctx.failure = QStringLiteral("Setup did not open");
                return;
            }
            auto* tree = d->findChild<QTreeWidget*>();
            QTreeWidgetItem* item = tree != nullptr ? treeItemForIndex(tree, index) : nullptr;
            if (item == nullptr) {
                ctx.failure = QStringLiteral("no tree item for page %1").arg(index);
                return;
            }
            tree->setCurrentItem(item);
            ctx.notes.append(QStringLiteral("Setup page %1 / %2 (registry index %3)%4")
                                 .arg(category, label).arg(index)
                                 .arg(hidden ? QStringLiteral(", hidden in the tree") : QString()));
            captureDialog(ctx, d, name);
        }});
    }
    dialog->close();
    return scenes;
}

} // namespace NereusSDR::Preview
