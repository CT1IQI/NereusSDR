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
//   2026-10-07 - Proposed workspace header scenes (header-always,
//                 header-reveal-idle, header-reveal-shown, header-hidden,
//                 arrange-on) with the Main Panel top crop. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Proposed four-edge workspace scenes (edge-right,
//                 edge-left, edge-top-bottom, edge-all-four, edge-all-four-clean,
//                 free-container, floating-container, drag-preview-left,
//                 drag-preview-between). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-07 - Longpath instrument scenes (instruments-sheet,
//                 mixed-container, mixed-container-reveal) with region
//                 provenance. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Workspace profile scenes (profile-ssb, profile-dx,
//                 profile-contest, profile-ft8, profile-rail-rest,
//                 profile-rail-open, profile-manage-dialog,
//                 profile-new-menu) with slice states and rail
//                 measurements. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Whole-image provenance regions on every sidecar; coverage
//                 scenes (clean-spectrum, cw-digital, tx-processing,
//                 edit-add-content, edit-source-select, edit-properties,
//                 four-rx-look, setup-look-*). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Named regions for each pan plot, the overlays on it, the
//                 pan separators and each VFO flag; fed readings name the
//                 poller's reading-consumer path. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Round 2: plain-English plates, setup-look-* removed
//                 (look-dialog-setup-* covers them), clean-spectrum-reveal,
//                 full-content strip details, EdgeDropOverlay proposed, TX
//                 profiles seeded for tx-processing. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - edit-source-select shows the source chip's menu open
//                 (its own --menu image) in place of its plate;
//                 edit-properties notes the declared property controls;
//                 preview factories take the render mode. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Review C: the declared-property group and property_*
//                 controls are tagged proposed; the source menu note says
//                 it was opened programmatically. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The edge values note the right strip's 0 px padding and
//                 3 px handle. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Setup page scenes centre the page's tree item and keep
//                 the pointer's hover out of the tree, and Radio Status
//                 holds its uptime, so they are the same every run.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "PreviewScenes.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QEnterEvent>
#include <QLabel>
#include <QMenu>
#include <QCheckBox>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QTimeZone>
#include <QToolTip>
#include <QMetaObject>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QAbstractItemView>
#include <QComboBox>
#include <QHash>
#include <QJsonDocument>
#include <QListWidget>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <memory>

#include "PreviewData.h"
#include "core/ControlRanges.h"
#include "core/WdspTypes.h"
#include "core/meters/SliceMeterPump.h"
#include "gui/DiversityDialog.h"
#include "gui/MainWindow.h"
#include "gui/MoxDisplayController.h"
#include "gui/PanLayoutDialog.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/PsForm.h"
#include "gui/SMeterWidget.h"
#include "gui/SetupDialog.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/applets/CwxApplet.h"
#include "gui/applets/DigitalApplet.h"
#include "gui/applets/DvkApplet.h"
#include "gui/applets/EqApplet.h"
#include "gui/applets/PhoneCwApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/applets/TxEqDialog.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerTypes.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/EdgeDockStrip.h"
#include "gui/containers/WorkspaceHeader.h"
#include "gui/containers/WorkspaceProfiles.h"
#include "gui/containers/WorkspaceProfilesDialog.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/MeterPoller.h"
#include "core/MicProfileManager.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/widgets/ProfileRail.h"
#include "gui/widgets/ProfileRailDock.h"
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

// ---- provenance regions ----------------------------------------------------

// The dynamic property a widget's provenance is tagged with; untagged
// widgets inherit from their nearest tagged ancestor, and an image root
// with no tag is retained.
constexpr auto kProvenanceProperty = "nereusPreviewProvenance";
// Optional region name and content type for a tagged widget.
constexpr auto kRegionNameProperty = "nereusPreviewRegionName";
constexpr auto kRegionTypeProperty = "nereusPreviewRegionType";
// Set on a widget that gets a region of its own even when its provenance
// matches its parent's (pan plots, the overlays on them, pan separators
// and VFO flags), so the retained-pixel proof can find them by name.
constexpr auto kForceRegionProperty = "nereusPreviewForceRegion";

QString provRetained() { return QStringLiteral("retained"); }
QString provLongpath() { return QStringLiteral("longpath-port"); }
QString provProposed() { return QStringLiteral("proposed"); }
QString provNotWired() { return QStringLiteral("exists-not-wired"); }
QString provNotImplemented() { return QStringLiteral("not-implemented"); }

void tagProvenance(QWidget* w, const QString& provenance, bool overwrite = true)
{
    if (w != nullptr && (overwrite || !w->property(kProvenanceProperty).isValid())) {
        w->setProperty(kProvenanceProperty, provenance);
    }
}

// Provenance by class. Container chrome stays retained (so containers in a
// proposed strip read as retained); the proposed headers, strips, splitter
// handles and profile chrome are proposed; the ported Longpath widgets are
// longpath-port.
QString classProvenance(QWidget* w)
{
    if (dynamic_cast<FrequencyInstrument*>(w) != nullptr || dynamic_cast<NeedleInstrument*>(w) != nullptr
        || dynamic_cast<BarInstrument*>(w) != nullptr || dynamic_cast<BandwidthFilterPane*>(w) != nullptr
        || dynamic_cast<ProfileRail*>(w) != nullptr) {
        return provLongpath();
    }
    if (dynamic_cast<EdgeDockStrip*>(w) != nullptr || dynamic_cast<WorkspaceGroupHeader*>(w) != nullptr
        || dynamic_cast<WorkspaceEntryHeader*>(w) != nullptr || dynamic_cast<WorkspaceCornerGrip*>(w) != nullptr
        || dynamic_cast<ProfileRailDock*>(w) != nullptr || dynamic_cast<WorkspaceProfilesDialog*>(w) != nullptr
        || w->objectName() == QLatin1String("profileRailPopover")
        || w->objectName() == QLatin1String("edgeDropOverlay")) {
        return provProposed();
    }
    // The property editor's declared-property group and controls
    // (ContentPropertyEditor, preview only); the rest of Container Settings
    // stays retained.
    if (w->objectName() == QLatin1String("declaredProperties")
        || w->objectName().startsWith(QLatin1String("property_"))) {
        return provProposed();
    }
    if (auto* handle = dynamic_cast<QSplitterHandle*>(w);
        handle != nullptr && dynamic_cast<EdgeDockSplitter*>(handle->splitter()) != nullptr) {
        return provProposed();
    }
    if (dynamic_cast<ContainerWidget*>(w) != nullptr) {
        return provRetained();
    }
    return {};
}

void forceRegion(QWidget* w, const QString& name)
{
    if (!w->property(kRegionNameProperty).isValid()) {
        w->setProperty(kRegionNameProperty, name);
    }
    w->setProperty(kForceRegionProperty, true);
}

// Names the panadapter regions under `root`: `pan-<n>-plot` for each pan's
// SpectrumWidget (the widget's own rect: spectrum, frequency scale and
// waterfall), `flag-<letter>` for each VFO flag on it, `pan-<n>-<object
// name or class>` for every other widget painted over the plot (the pan
// toolbar, corner buttons, status badges, hints; children of the
// SpectrumWidget or of its PanadapterApplet), and `pan-separator-<k>` for
// the splitter handles between pans. All keep the provenance they inherit.
void tagPanRegions(QWidget* root)
{
    QList<PanadapterApplet*> applets = root->findChildren<PanadapterApplet*>();
    if (auto* self = qobject_cast<PanadapterApplet*>(root)) {
        applets.prepend(self);
    }
    for (PanadapterApplet* applet : applets) {
        SpectrumWidget* plot = applet->spectrumWidget();
        if (plot == nullptr) {
            continue;
        }
        const QString pan = applet->panId();
        forceRegion(plot, pan + QStringLiteral("-plot"));
        QList<QWidget*> over;
        for (QObject* o : plot->children()) {
            if (auto* w = qobject_cast<QWidget*>(o)) {
                over.append(w);
            }
        }
        for (QObject* o : applet->children()) {
            auto* w = qobject_cast<QWidget*>(o);
            if (w != nullptr && w != plot && w->geometry().intersects(plot->geometry())) {
                over.append(w);
            }
        }
        QHash<QString, int> seen;
        for (QWidget* w : over) {
            if (w->isWindow()) {
                continue;
            }
            if (auto* flag = qobject_cast<VfoWidget*>(w)) {
                SliceModel* slice = flag->sliceForTest();
                const QChar letter = slice != nullptr ? slice->sliceLetter() : QChar(QLatin1Char('?'));
                forceRegion(flag, QStringLiteral("flag-%1").arg(letter));
                continue;
            }
            const QString what = w->objectName().isEmpty() ? QString::fromLatin1(w->metaObject()->className())
                                                             : w->objectName();
            const int n = seen.value(what, 0);
            seen.insert(what, n + 1);
            forceRegion(w, n == 0 ? QStringLiteral("%1-%2").arg(pan, what)
                                  : QStringLiteral("%1-%2-%3").arg(pan, what).arg(n + 1));
        }
    }
    QList<PanadapterStack*> stacks = root->findChildren<PanadapterStack*>();
    int separator = 0;
    for (PanadapterStack* stack : stacks) {
        for (QSplitterHandle* handle : stack->findChildren<QSplitterHandle*>()) {
            if (!handle->isVisibleTo(root)) {
                continue;
            }
            forceRegion(handle, QStringLiteral("pan-separator-%1").arg(separator++));
        }
    }
}

// Tags every widget under `root` by class (never over a scene's own tag),
// and each container entry widget with its entry id, content type and,
// for Longpath content, longpath-port.
void tagTree(QWidget* root)
{
    QList<QWidget*> all = root->findChildren<QWidget*>();
    all.prepend(root);
    for (QWidget* w : all) {
        const QString p = classProvenance(w);
        if (!p.isEmpty()) {
            tagProvenance(w, p, false);
        }
    }
    tagPanRegions(root);
    QList<ContainerContentHost*> hosts = root->findChildren<ContainerContentHost*>();
    if (auto* self = qobject_cast<ContainerContentHost*>(root)) {
        hosts.prepend(self);
    }
    for (ContainerContentHost* host : hosts) {
        const ContainerDocument doc = host->captureDocument();
        for (const auto& row : host->entryRows()) {
            if (row.widget == nullptr) {
                continue;
            }
            QString typeId;
            for (const ContentEntry& e : doc.contents) {
                if (e.id == row.entryId) {
                    typeId = e.typeId;
                }
            }
            if (!row.widget->property(kRegionNameProperty).isValid()) {
                row.widget->setProperty(kRegionNameProperty, row.entryId);
                row.widget->setProperty(kRegionTypeProperty, typeId);
            }
            if (typeId.startsWith(QLatin1String("longpath."))) {
                tagProvenance(row.widget, provLongpath(), false);
            }
        }
    }
}

QJsonArray regionRectJson(const QRect& r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

QRect regionRectFrom(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    return QRect(a.at(0).toInt(), a.at(1).toInt(), a.at(2).toInt(), a.at(3).toInt());
}

// Appends the regions of `w` (and its subtree) to `out`. Rects are in
// `root` coordinates moved by `offset`; `clip` is in root coordinates.
void walkRegions(QWidget* root, QWidget* w, const QString& inherited, int parent, const QRect& clip,
                 const QPoint& offset, QJsonArray& out)
{
    const QRect rect(w == root ? QPoint(0, 0) : w->mapTo(root, QPoint(0, 0)), w->size());
    const QRect visible = rect.intersected(clip);
    if (visible.isEmpty()) {
        return;
    }
    const QString own = w->property(kProvenanceProperty).toString();
    const QString provenance = own.isEmpty() ? inherited : own;
    int index = parent;
    if (w == root || provenance != inherited || w->property(kForceRegionProperty).toBool()) {
        index = int(out.size());
        QString name = w->property(kRegionNameProperty).toString();
        if (name.isEmpty()) {
            name = w->objectName();
        }
        if (name.isEmpty()) {
            name = QString::fromLatin1(w->metaObject()->className());
        }
        QString type = w->property(kRegionTypeProperty).toString();
        if (type.isEmpty()) {
            type = w->property("containerFactoryType").toString();
        }
        const int depth = parent < 0 ? 0 : out.at(parent).toObject().value(QStringLiteral("depth")).toInt() + 1;
        out.append(QJsonObject{{QStringLiteral("name"), name},
                               {QStringLiteral("provenance"), provenance},
                               {QStringLiteral("rect"), regionRectJson(rect.translated(offset))},
                               {QStringLiteral("visible_rect"), regionRectJson(visible.translated(offset))},
                               {QStringLiteral("type"), type},
                               {QStringLiteral("widget_class"), QString::fromLatin1(w->metaObject()->className())},
                               {QStringLiteral("parent"), parent},
                               {QStringLiteral("depth"), depth}});
    }
    for (QObject* o : w->children()) {
        auto* child = qobject_cast<QWidget*>(o);
        if (child == nullptr || child->isWindow() || !child->isVisibleTo(root)) {
            continue;
        }
        walkRegions(root, child, provenance, index, visible, offset, out);
    }
}

// The regions of the top-level `top` painted at `offset` into an image of
// `imageSize`, appended to `out`.
void appendRegions(QJsonArray& out, QWidget* top, const QPoint& offset, const QSize& imageSize)
{
    tagTree(top);
    // A root that is not a top-level (a detail image of a strip's body)
    // inherits from its nearest tagged ancestor, as it would in the window.
    QString own = top->property(kProvenanceProperty).toString();
    for (QWidget* up = top->parentWidget(); own.isEmpty() && up != nullptr && !top->isWindow();
         up = up->parentWidget()) {
        own = up->property(kProvenanceProperty).toString();
        if (own.isEmpty()) {
            own = classProvenance(up);
        }
    }
    const QRect clip = QRect(-offset, imageSize).intersected(top->rect());
    walkRegions(top, top, own.isEmpty() ? provRetained() : own, -1, clip, QPoint(0, 0) + offset, out);
}

QString regionUnits()
{
    return QStringLiteral(
        "rect and visible_rect are [x, y, width, height] in logical px from this image's top-left; "
        "device px = logical px x dpr");
}

QString regionsContract()
{
    return QStringLiteral(
        "regions: the top-most provenance regions covering the whole image, in paint order (a parent "
        "before its children, siblings bottom to top; composited top-levels after the window). Each "
        "widget carries nereusPreviewProvenance (retained, longpath-port, proposed, exists-not-wired, "
        "not-implemented) or inherits it from its nearest tagged ancestor; an image root with no tag "
        "is retained. A region is written for the root and for each visible widget whose provenance "
        "differs from its parent's, and for each named panadapter part whatever its provenance: pan-<n>-plot "
        "(widget_class NereusSDR::SpectrumWidget, the pan's SpectrumWidget rect taken from the widget, not "
        "computed: spectrum, frequency scale and waterfall), flag-<letter> (VfoWidget, the VFO flag of that "
        "slice), pan-<n>-<object name or class> (each other widget over the plot: pan toolbar, corner "
        "buttons, status badges, hints; a repeated name gets -2, -3) and pan-separator-<k> (k from 0 over the visible handles) (splitter handles between pans); the plot "
        "minus the regions inside it is the plot area alone. A point's provenance is that of the last region whose visible_rect "
        "contains it. Fields: name (entry id, object name or class), provenance, rect (the widget), "
        "visible_rect (clipped to its ancestors and the image; regions with no visible area are "
        "omitted), type (content type id for container content, else empty), widget_class, parent "
        "(index of the enclosing region, -1 for an image root), depth (0 for a root)");
}

// Sets the whole-image regions of `img`, grabbed from `root`.
void attachRegions(CapturedImage& img, QWidget* root)
{
    QJsonArray regions;
    appendRegions(regions, root, QPoint(0, 0), img.logicalSize);
    img.extra[QStringLiteral("regions")] = regions;
    img.extra[QStringLiteral("region_units")] = regionUnits();
    img.extra[QStringLiteral("regions_contract")] = regionsContract();
}

// The regions of `regions` inside `crop` (logical px of the source image),
// moved to the crop's top-left and scaled by `scale`.
QJsonArray cropRegions(const QJsonArray& regions, const QRect& crop, int scale)
{
    QJsonArray out;
    QHash<int, int> remap;
    const auto place = [&crop, scale](const QRect& r) {
        const QRect m = r.translated(-crop.topLeft());
        return QRect(m.x() * scale, m.y() * scale, m.width() * scale, m.height() * scale);
    };
    for (int i = 0; i < regions.size(); ++i) {
        QJsonObject r = regions.at(i).toObject();
        const QRect visible = regionRectFrom(r.value(QStringLiteral("visible_rect"))).intersected(crop);
        if (visible.isEmpty()) {
            continue;
        }
        remap.insert(i, int(out.size()));
        r[QStringLiteral("rect")] = regionRectJson(place(regionRectFrom(r.value(QStringLiteral("rect")))));
        r[QStringLiteral("visible_rect")] = regionRectJson(place(visible));
        r[QStringLiteral("parent")] = remap.value(r.value(QStringLiteral("parent")).toInt(), -1);
        out.append(r);
    }
    return out;
}

void appendCropImages(SceneContext& ctx, const CapturedImage& full, const QString& name);

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
        if (ctx.extraCrops) {
            ctx.extraCrops(ctx, full.crops, full.dpr);
        }
    }
    attachRegions(full, ctx.window);
    ctx.images.append(full);
    appendCropImages(ctx, full, name);
}

// Each crop of `full` as its own image, plus a 3x nearest-neighbour copy.
void appendCropImages(SceneContext& ctx, const CapturedImage& full, const QString& name)
{
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
        if (full.extra.contains(QStringLiteral("regions"))) {
            c.extra[QStringLiteral("regions")] =
                cropRegions(full.extra.value(QStringLiteral("regions")).toArray(), crop.logical, 1);
            c.extra[QStringLiteral("region_units")] = regionUnits();
            c.extra[QStringLiteral("regions_contract")] = regionsContract();
        }
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
        if (full.extra.contains(QStringLiteral("regions"))) {
            big.extra[QStringLiteral("regions")] =
                cropRegions(full.extra.value(QStringLiteral("regions")).toArray(), crop.logical, 3);
            big.extra[QStringLiteral("region_units")] = regionUnits();
            big.extra[QStringLiteral("regions_contract")] = regionsContract();
        }
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

// ---- proposed workspace headers -------------------------------------------

// The Main Panel's top band, where the group and first entry headers sit.
constexpr int kPanelTopCropPx = 120;

void addPanelTopCrop(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    ContainerWidget* panel = mainPanel(ctx);
    if (panel == nullptr || !panel->isVisible() || !ctx.window->isAncestorOf(panel)) {
        return;
    }
    const QRect logical(panel->mapTo(ctx.window, QPoint(0, 0)),
                        QSize(panel->width(), qMin(panel->height(), kPanelTopCropPx)));
    const QRect clipped = logical.intersected(QRect(QPoint(0, 0), ctx.window->size()));
    if (!clipped.isEmpty()) {
        crops.append({QStringLiteral("panel-top"), clipped, deviceRect(clipped, dpr)});
    }
}

QJsonObject proposedHeaderValues()
{
    return QJsonObject{
        {QStringLiteral("group_header_height_px"), WorkspaceGroupHeader::kHeight},
        {QStringLiteral("group_header_background"), QStringLiteral(
            "titleBarStyle(): #26262b / #1c1c20 / #141417, bottom #0d0d0f, top rgba(255,255,255,18), top radius 9")},
        {QStringLiteral("group_header_margins"), QStringLiteral("2,0,4,0, spacing 4")},
        {QStringLiteral("stripe"), QStringLiteral("3 px #d8a55f")},
        {QStringLiteral("grip"), QStringLiteral("U+22EE U+22EE, 11 px #7e7e85 (hover #dcdce1), hit 14x18")},
        {QStringLiteral("title"), QStringLiteral("capsFont 9 px DemiBold #828288")},
        {QStringLiteral("source_chip"), QStringLiteral(
            "mono 9 px slice colour on #16161c, 1 px slice colour at 60%, radius 4, height 13")},
        {QStringLiteral("buttons"), QStringLiteral("16x14, glyph #7e7e85, hover #26262b radius 7: pop out, options")},
        {QStringLiteral("plate"), QStringLiteral("glassPanelFill #17171b..#0e0e10, 1 px #1f1f23, radius 10")},
        {QStringLiteral("entry_header_height_px"), WorkspaceEntryHeader::kHeight},
        {QStringLiteral("entry_header"), QStringLiteral(
            "transparent, bottom hairline #1f1f23, grip 10 px #58585e (hover #a6a6ac), "
            "title capsFont 9 #7e7e85, options 14x12")},
        {QStringLiteral("content_left_gutter_px"), 0},
        {QStringLiteral("reveal"), QStringLiteral(
            "zero extent; after 120 ms in the top 18 px band or keyboard focus; fade 0 to 0.96 in 90 ms; "
            "6 px shadow rgba(0,0,0,110) to transparent; hides 600 ms after leaving the band")},
        {QStringLiteral("arrange"), QStringLiteral(
            "header bottom border #4a7ba8; 1 px dashed #4a7ba8 at 60% outline; 12x12 dot corner #7e7e85; "
            "4 px inside edge zones; Ctrl+Shift+A; not saved")},
    };
}

// One proposed header scene: the two-band data with the workspace look,
// the Main Panel in `mode`, then `pose` (after the data is fed).
void sceneProposedHeader(SceneContext& ctx, HeaderMode mode, const QString& name,
                         const std::function<void(SceneContext&, ContainerWidget*)>& pose = {})
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.proposedValues = proposedHeaderValues();
    ctx.extraCrops = addPanelTopCrop;
    runWindowScene(ctx, twoBandPoses(), 0, [mode](SceneContext& c) {
        ContainerWidget* panel = mainPanel(c);
        if (panel == nullptr) {
            c.failure = QStringLiteral("no Main Panel container");
            return;
        }
        // The options menu's Header > Always / Reveal / Hidden path.
        emit panel->headerModeRequested(mode);
    }, [pose](SceneContext& c) {
        if (pose) {
            pose(c, mainPanel(c));
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    if (ContainerWidget* panel = mainPanel(ctx)) {
        ctx.notes.append(QStringLiteral("Main Panel header extent %1 px, header visible %2, arrange %3")
                             .arg(panel->headerExtent())
                             .arg(panel->chromeVisible() ? QStringLiteral("yes") : QStringLiteral("no"))
                             .arg(panel->arrangeActive() ? QStringLiteral("on") : QStringLiteral("off")));
    }
    captureWindow(ctx, name);
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

// ---- proposed four-edge workspace -----------------------------------------

QString slug(const QString& text);

struct EdgeEntry {
    QString typeId;
    // FreeCanvas placement in container pixels; empty for a stack.
    QRectF rect;
    // A legacy record for the item's own settings (button columns).
    QString legacyRecord;
    // Source context keys merged into the entry's context (rxSource).
    QJsonObject context;
    // Keys merged into the entry's config (a Longpath binding).
    QJsonObject config;
};

// One row of buttons: every button in its own column, at `heightRatio`,
// as Container Settings sets them for a short strip.
template <typename Item>
QString singleRowRecord(float heightRatio)
{
    Item item;
    item.setColumns(qMax(1, item.buttonCount()));
    item.setHeightRatio(heightRatio);
    return item.serialize();
}

// Adds a container to the committed workspace, as the Containers menu and
// the settings dialog do: a draft with registry entries, committed.
ContainerDocument makeSceneContainer(ContainerManager* manager, const QString& name, DockEdge edge,
                                     ContentLayout layout, const QVector<EdgeEntry>& entries)
{
    ContainerDocument d;
    d.id = QStringLiteral("preview-") + slug(name);
    d.name = name;
    d.layout = layout;
    d.dockMode = DockMode::PanelDocked;
    d.edge = edge;
    d.header = HeaderMode::Always;
    for (const EdgeEntry& e : entries) {
        ContentEntry entry = manager->contentRegistry()->makeEntry(e.typeId);
        entry.id = d.id + QStringLiteral("-") + slug(e.typeId);
        // A second entry of the same type gets a numbered id.
        for (int n = 2; std::any_of(d.contents.cbegin(), d.contents.cend(),
                                    [&entry](const ContentEntry& c) { return c.id == entry.id; }); ++n) {
            entry.id = d.id + QStringLiteral("-") + slug(e.typeId) + QStringLiteral("-%1").arg(n);
        }
        for (auto it = e.config.constBegin(); it != e.config.constEnd(); ++it) {
            entry.config.insert(it.key(), it.value());
        }
        if (!e.legacyRecord.isEmpty()) {
            entry.config.insert(QStringLiteral("legacyRecord"), e.legacyRecord);
        }
        for (auto it = e.context.constBegin(); it != e.context.constEnd(); ++it) {
            entry.context.insert(it.key(), it.value());
        }
        if (!e.rect.isNull()) {
            entry.extensions[QStringLiteral("freeCanvasRect")] =
                QJsonArray{e.rect.x(), e.rect.y(), e.rect.width(), e.rect.height()};
        }
        d.contents.append(entry);
    }
    return d;
}

QString addSceneContainer(SceneContext& ctx, const QString& name, DockEdge edge,
                          ContentLayout layout, const QVector<EdgeEntry>& entries)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr || manager->contentRegistry() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return {};
    }
    WorkspaceDocument draft = manager->workspaceStore()->snapshot();
    const ContainerDocument d = makeSceneContainer(manager, name, edge, layout, entries);
    draft.containers.append(d);
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        ctx.failure = QStringLiteral("could not add the %1 container").arg(name);
        return {};
    }
    return d.id;
}

// Free canvas rows for a 112 px strip: a meter view is at least 49 px
// tall, and the canvas keeps 20 px above and below, so the 82 px under an
// Always header scrolls by 7 px (recorded in the Task 4 report).
constexpr double kEndRowHeightPx = 49.0;

// "Meters": the Thetis S-Meter preset and the Power/SWR face side by side.
QVector<EdgeEntry> metersEntries()
{
    return {{QStringLiteral("meter.sMeter"), QRectF(0, 0, 420, kEndRowHeightPx)},
            {QStringLiteral("meter.powerSwr"), QRectF(440, 0, 420, kEndRowHeightPx)}};
}

QString addMetersContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Meters"), edge, ContentLayout::FreeCanvas, metersEntries());
}

// "Band": the band and mode button boxes side by side, one row each.
QVector<EdgeEntry> bandEntries()
{
    return {{QStringLiteral("BANDBTNS"), QRectF(0, 0, 840, kEndRowHeightPx),
             singleRowRecord<BandButtonItem>(0.4f)},
            {QStringLiteral("MODEBTNS"), QRectF(860, 0, 520, kEndRowHeightPx),
             singleRowRecord<ModeButtonItem>(0.4f)}};
}

QString addBandContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Band"), edge, ContentLayout::FreeCanvas, bandEntries());
}

// "Levels": two bar meters stacked, a container that reads well on a side.
QVector<EdgeEntry> levelsEntries()
{
    return {{QStringLiteral("meter.signalMaxBin"), {}}, {QStringLiteral("meter.comp"), {}}};
}

QString addLevelsContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Levels"), edge, ContentLayout::VerticalStack, levelsEntries());
}

QJsonObject proposedEdgeValues()
{
    return QJsonObject{
        {QStringLiteral("layout"), QStringLiteral(
            "vertical splitter [top | middle | bottom], middle [left | spectrum | right]; "
            "top and bottom span the width; an empty strip takes no space")},
        {QStringLiteral("strip_padding_px"), EdgeDockStrip::kPaddingPx},
        {QStringLiteral("strip_padding_right_px"), EdgeDockStrip::kRightPaddingPx},
        {QStringLiteral("strip_gap_px"), EdgeDockStrip::kGapPx},
        {QStringLiteral("strip_background"), QStringLiteral("app-bg")},
        {QStringLiteral("handle"), QStringLiteral(
            "6 px (3 px on the spectrum side of the right strip, today's splitter handle), "
            "app-bg, 1 px centre line border role, accent role under the pointer")},
        {QStringLiteral("thickness_left_right_px"), EdgeDockStrip::kSideThicknessPx},
        {QStringLiteral("thickness_top_bottom_px"), EdgeDockStrip::kEndThicknessPx},
        {QStringLiteral("thickness_top_bottom_default"), QStringLiteral(
            "max(112, tallest container's preferred height incl. header + 2 x padding); "
            "scrolls only when made smaller")},
        {QStringLiteral("thickness_side_minimum"), QStringLiteral(
            "widest container minimum + 2 x padding")},
        {QStringLiteral("thickness_right_first_run"), QStringLiteral(
            "today's Main Panel width (the 3 px-handle splitter's 1024/256 split, "
            "the panel raised to its minimum), no padding, until dragged or saved")},
        {QStringLiteral("free_and_floating_background"), QStringLiteral(
            "opaque glass plate (panel-top to panel-bottom); floating shell panel role")},
        {QStringLiteral("target_band"), QStringLiteral("6 px at each edge, accent at 35%")},
        {QStringLiteral("drop_preview"), QStringLiteral(
            "accent at 22% fill, 1 px #3d78b0, radius 6; 28 px between containers")},
        {QStringLiteral("grid"), QStringLiteral("8 px, off by default, Alt places freely")},
    };
}

void addStripCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        return;
    }
    addCrop(crops, QStringLiteral("strip-left"), ctx.window, area->strip(DockEdge::Left), dpr);
    addCrop(crops, QStringLiteral("strip-top"), ctx.window, area->strip(DockEdge::Top), dpr);
    addCrop(crops, QStringLiteral("strip-bottom"), ctx.window, area->strip(DockEdge::Bottom), dpr);
}

void noteStrips(SceneContext& ctx)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        ctx.notes.append(QStringLiteral("no EdgeDockArea: the workspace preview switch is off"));
        return;
    }
    const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("left")},
                                              {DockEdge::Right, QStringLiteral("right")},
                                              {DockEdge::Top, QStringLiteral("top")},
                                              {DockEdge::Bottom, QStringLiteral("bottom")}};
    for (const auto& [edge, name] : edges) {
        EdgeDockStrip* strip = area->strip(edge);
        QStringList titles;
        for (QWidget* w : strip->containers()) {
            if (auto* c = qobject_cast<ContainerWidget*>(w); c != nullptr && !c->isHidden()) {
                QString scroll;
                if (auto* area = c->contentHolder()->findChild<QScrollArea*>()) {
                    scroll = QStringLiteral(", viewport %1x%2, body min %3x%4")
                                 .arg(area->viewport()->width())
                                 .arg(area->viewport()->height())
                                 .arg(area->widget() ? area->widget()->minimumSizeHint().width() : -1)
                                 .arg(area->widget() ? area->widget()->minimumSizeHint().height() : -1);
                }
                titles.append(QStringLiteral("%1 (container %5x%6, minimum width %7, content %2x%3%4)")
                                  .arg(c->notes().isEmpty() ? c->id() : c->notes())
                                  .arg(c->contentHolder()->width())
                                  .arg(c->contentHolder()->height())
                                  .arg(scroll)
                                  .arg(c->width()).arg(c->height())
                                  .arg(qMax(c->minimumWidth(), c->minimumSizeHint().width())));
            }
        }
        ctx.notes.append(QStringLiteral("%1 strip: %2, %3x%4 [%5]")
                             .arg(name, strip->isVisible() ? QStringLiteral("shown") : QStringLiteral("empty"))
                             .arg(strip->width()).arg(strip->height())
                             .arg(titles.join(QStringLiteral(", "))));
    }
}

// One proposed edge scene: the two-band data with the workspace look, every
// container's header in `header` (Always unless a scene says otherwise),
// `arrange` before the data and `pose` after.
void sceneProposedEdges(SceneContext& ctx, const QString& name,
                        const std::function<void(SceneContext&)>& arrange,
                        const std::function<void(SceneContext&)>& pose = {},
                        HeaderMode header = HeaderMode::Always)
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addStripCrops;
    runWindowScene(ctx, twoBandPoses(), 0, [arrange, header](SceneContext& c) {
        if (mainPanel(c) == nullptr || c.window->findChild<EdgeDockArea*>() == nullptr) {
            c.failure = QStringLiteral("no Main Panel container or edge area");
            return;
        }
        if (arrange) {
            arrange(c);
        }
        if (!c.failure.isEmpty()) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        for (ContainerWidget* container : manager->allContainers()) {
            emit container->headerModeRequested(header);
        }
    }, [pose](SceneContext& c) {
        if (pose) {
            pose(c);
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
}

// ---- Longpath instruments (workspace preview) ------------------------------

// The fixed instrument values the sheet shows.
constexpr double kSheetFrequencyHz = 14225000.0;
constexpr double kSheetOtherHz = 7185000.0;
constexpr double kSheetSignalDbm = -73.0;
constexpr double kSheetSignalPeakDbm = -68.0;
constexpr double kSheetTxWatts = 85.0;
constexpr double kSheetSwr = 1.25;
// A realistic ALC level just under Thetis's red zone (which starts at
// 0 dB). Fixed: the sheet has no transmitter.
constexpr double kSheetAlcDb = -3.0;
constexpr int kSheetFilterLowHz = 150;
constexpr int kSheetFilterHighHz = 2850;
constexpr int kSheetTracePoints = 160;
constexpr int kSheetMarginPx = 16;
constexpr int kSheetGapPx = 16;
constexpr int kSheetLabelPx = 18;
constexpr int kSheetRowGapPx = 14;
// The needle hub crop: this far either side of the pivot column, and
// this far above and below the footer's top edge.
constexpr int kHubHalfWidthPx = 60;
constexpr int kHubAbovePx = 60;
constexpr int kHubBelowPx = 12;

QJsonArray rectJson(const QRect& r)
{
    return QJsonArray{r.x(), r.y(), r.width(), r.height()};
}

// A fixed passband trace over the pane's 10 kHz span, centred on the VFO:
// a deterministic noise floor near -118 dBm, a USB voice hump from +200 to
// +2700 Hz, and a carrier at +1200 Hz.
QVector<float> sheetTrace(int spanHz)
{
    QVector<float> trace(kSheetTracePoints);
    for (int i = 0; i < kSheetTracePoints; ++i) {
        const double offsetHz = -spanHz / 2.0 + spanHz * i / (kSheetTracePoints - 1.0);
        double dbm = -118.0 + 2.5 * std::sin(i * 1.7) + 1.5 * std::sin(i * 0.37);
        if (offsetHz >= 200.0 && offsetHz <= 2700.0) {
            dbm = std::max(dbm, -86.0 + 6.0 * std::sin(offsetHz / 350.0) + 2.0 * std::sin(i * 2.3));
        }
        if (std::abs(offsetHz - 1200.0) < spanHz / double(kSheetTracePoints)) {
            dbm = -62.0;
        }
        trace[i] = static_cast<float>(dbm);
    }
    return trace;
}

QSize minimumOf(const QWidget* w)
{
    return w->minimumSizeHint().expandedTo(w->minimumSize());
}

struct SheetRow {
    QString key;        // crop and region name stem
    QString title;
    std::function<QWidget*(QWidget* parent)> make;
    int column{0};
};

// instruments-sheet: every ported widget at its preferred and minimum
// size, on its own top-level window (not the MainWindow).
void sceneInstrumentsSheet(SceneContext& ctx)
{
    ctx.provenance = QStringLiteral("longpath-port");
    auto frequency = [](FrequencyInstrument::Form form) {
        return [form](QWidget* parent) -> QWidget* {
            auto* f = new FrequencyInstrument(parent);
            f->setForm(form);
            f->setFrequency(kSheetFrequencyHz);
            f->setOtherFrequency(kSheetOtherHz);
            f->setActiveIsThis(true);
            return f;
        };
    };
    const QVector<SheetRow> rows = {
        {QStringLiteral("frequency"), QStringLiteral("FrequencyInstrument, NumberOnly, 14.225.000 (other 7.185.000)"),
         frequency(FrequencyInstrument::Form::NumberOnly), 0},
        {QStringLiteral("frequency-bandstrip"), QStringLiteral("FrequencyInstrument, BandStrip"),
         frequency(FrequencyInstrument::Form::BandStrip), 0},
        {QStringLiteral("frequency-flatarc"), QStringLiteral("FrequencyInstrument, FlatArc"),
         frequency(FrequencyInstrument::Form::FlatArc), 0},
        {QStringLiteral("filter"), QStringLiteral("BandwidthFilterPane, RX A USB 150..2850, fixed trace"),
         [](QWidget* parent) -> QWidget* {
             auto* p = new BandwidthFilterPane(parent);
             p->setLabel(QStringLiteral("RX A"));
             p->setAccent(QColor::fromRgb(ControlRanges::sliceColour(0)));
             p->setHasFrequency(true);
             p->setVfoFrequency(kSheetFrequencyHz);
             p->setFilter(kSheetFilterLowHz, kSheetFilterHighHz);
             p->setTrace(sheetTrace(p->spanHz()));
             return p;
         }, 0},
        {QStringLiteral("bar-swr"), QStringLiteral("BarInstrument, TX: SWR 1.25"),
         [](QWidget* parent) -> QWidget* {
             auto* b = new BarInstrument(parent);
             b->setPrimary(MeterBinding::TxSwr);
             b->onReading(MeterBinding::TxSwr, kSheetSwr);
             return b;
         }, 0},
        {QStringLiteral("bar-alc"), QStringLiteral("BarInstrument, TX: ALC -3 dB (fixed sheet value)"),
         [](QWidget* parent) -> QWidget* {
             auto* b = new BarInstrument(parent);
             // TX: ALC carries Thetis's ALC scale (ReadingSource.cpp). A
             // refusal would leave the unbound face, which must not stand
             // in for ALC, so the sidecar records it.
             b->setProperty("sheetBindRefused", !b->setPrimary(MeterBinding::TxAlc));
             b->onReading(MeterBinding::TxAlc, kSheetAlcDb);
             return b;
         }, 0},
        {QStringLiteral("needle-signal"), QStringLiteral("NeedleInstrument, RX: Signal Peak -73 dBm, peak -68"),
         [](QWidget* parent) -> QWidget* {
             auto* n = new NeedleInstrument(parent);
             n->setPrimary(MeterBinding::SignalPeak);
             n->onReading(MeterBinding::SignalPeak, kSheetSignalPeakDbm);
             n->onReading(MeterBinding::SignalPeak, kSheetSignalDbm);
             return n;
         }, 1},
        {QStringLiteral("needle-txpower"), QStringLiteral("NeedleInstrument, TX: Forward Power 85 W"),
         [](QWidget* parent) -> QWidget* {
             auto* n = new NeedleInstrument(parent);
             n->setPrimary(MeterBinding::TxPower);
             n->onReading(MeterBinding::TxPower, kSheetTxWatts);
             return n;
         }, 1},
    };

    auto sheet = std::make_unique<QWidget>();
    sheet->setObjectName(QStringLiteral("instrumentsSheet"));
    sheet->setWindowTitle(QStringLiteral("Longpath instruments"));
    sheet->setAttribute(Qt::WA_DontShowOnScreen, true);
    sheet->setAutoFillBackground(true);
    struct Placed {
        const SheetRow* row;
        QLabel* label;
        QWidget* preferred;
        QWidget* minimum;
    };
    QVector<Placed> placed;
    for (const SheetRow& row : rows) {
        auto* label = new QLabel(sheet.get());
        label->setStyleSheet(QStringLiteral("color: #828288; font-size: 10px;"));
        placed.append({&row, label, row.make(sheet.get()), row.make(sheet.get())});
    }
    sheet->resize(400, 400);
    sheet->show();
    pump(kFinalSettleMs);

    // Sizes come from the shown widgets (FrequencyInstrument's minimum
    // counts its A/B row only when it is shown).
    int columnX[2] = {kSheetMarginPx, kSheetMarginPx};
    int columnY[2] = {kSheetMarginPx, kSheetMarginPx};
    int column0Width = 0;
    for (const Placed& p : placed) {
        const QSize pref = p.preferred->sizeHint();
        const QSize min = minimumOf(p.minimum);
        column0Width = p.row->column == 0 ? qMax(column0Width, pref.width() + kSheetGapPx + min.width())
                                          : column0Width;
    }
    columnX[1] = kSheetMarginPx + column0Width + 2 * kSheetGapPx;
    int right = 0;
    int bottom = 0;
    for (Placed& p : placed) {
        const int c = p.row->column;
        const QSize pref = p.preferred->sizeHint();
        const QSize min = minimumOf(p.minimum);
        p.label->setText(QStringLiteral("%1   preferred %2x%3 | minimum %4x%5")
                             .arg(p.row->title).arg(pref.width()).arg(pref.height())
                             .arg(min.width()).arg(min.height()));
        p.label->adjustSize();
        p.label->move(columnX[c], columnY[c]);
        const int y = columnY[c] + kSheetLabelPx;
        p.preferred->setGeometry(QRect(QPoint(columnX[c], y), pref));
        p.minimum->setGeometry(QRect(QPoint(columnX[c] + pref.width() + kSheetGapPx, y), min));
        columnY[c] = y + qMax(pref.height(), min.height()) + kSheetRowGapPx;
        right = qMax(right, qMax(p.minimum->geometry().right(), p.label->geometry().right()));
        bottom = qMax(bottom, columnY[c]);
    }
    sheet->resize(right + kSheetMarginPx, bottom + kSheetMarginPx - kSheetRowGapPx);
    pump(kFinalSettleMs);
    for (const QJsonValue& v : stopTimersUnder(sheet.get())) {
        ctx.stoppedTimers.append(v);
    }

    CapturedImage img;
    img.name = QStringLiteral("instruments-sheet");
    img.kind = QStringLiteral("sheet");
    img.image = grabWidget(sheet.get());
    img.logicalSize = sheet->size();
    img.dpr = img.image.devicePixelRatio();
    QJsonArray regions;
    for (const Placed& p : placed) {
        for (const auto& [w, kind] : {std::pair<QWidget*, QString>{p.preferred, QStringLiteral("preferred")},
                                      std::pair<QWidget*, QString>{p.minimum, QStringLiteral("minimum")}}) {
            regions.append(QJsonObject{
                {QStringLiteral("name"), p.row->key + QLatin1Char('-') + kind},
                {QStringLiteral("provenance"), QStringLiteral("longpath-port")},
                {QStringLiteral("class"), QString::fromLatin1(w->metaObject()->className())},
                {QStringLiteral("size"), kind},
                {QStringLiteral("rect"), rectJson(w->geometry())}});
        }
        regions.append(QJsonObject{
            {QStringLiteral("name"), p.row->key + QStringLiteral("-label")},
            {QStringLiteral("provenance"), QStringLiteral("proposed")},
            {QStringLiteral("rect"), rectJson(p.label->geometry())}});
        if (p.minimum->property("sheetBindRefused").toBool()) {
            ctx.notes.append(QStringLiteral("%1: BarInstrument::setPrimary(TxAlc) refused it; "
                                            "the bar shows its unbound face, not ALC").arg(p.row->key));
        }
    }
    // The per-instrument list (with its size column) predates the
    // whole-image regions.
    img.extra[QStringLiteral("sheet_regions")] = regions;
    tagProvenance(sheet.get(), provProposed());
    attachRegions(img, sheet.get());
    img.extra[QStringLiteral("window")] = QStringLiteral("its own top-level QWidget, not the MainWindow");
    if (ctx.crops) {
        const auto cropOf = [&](const QString& name, const QRect& logical) {
            const QRect clipped = logical.intersected(QRect(QPoint(0, 0), sheet->size()));
            if (!clipped.isEmpty()) {
                img.crops.append({name, clipped, deviceRect(clipped, img.dpr)});
            }
        };
        for (const Placed& p : placed) {
            if (p.row->key == QLatin1String("frequency")) {
                cropOf(QStringLiteral("frequency-digits"), p.preferred->geometry());
                cropOf(QStringLiteral("frequency-digits-min"), p.minimum->geometry());
            } else if (p.row->key == QLatin1String("needle-signal")) {
                for (const auto& [w, suffix] : {std::pair<QWidget*, QString>{p.preferred, QString()},
                                                std::pair<QWidget*, QString>{p.minimum, QStringLiteral("-min")}}) {
                    auto* footer = w->findChild<InstrumentFooter*>();
                    const int footerTop = footer != nullptr ? footer->y() : w->height();
                    const QRect hub(w->x() + w->width() / 2 - kHubHalfWidthPx,
                                    w->y() + footerTop - kHubAbovePx,
                                    2 * kHubHalfWidthPx, kHubAbovePx + kHubBelowPx);
                    cropOf(QStringLiteral("needle-hub") + suffix, hub);
                }
            }
        }
    }
    ctx.images.append(img);
    appendCropImages(ctx, img, img.name);
    ctx.notes.append(QStringLiteral("sheet %1x%2; instruments built directly with fixed values, "
                                    "no MeterPoller or slice binding")
                         .arg(sheet->width()).arg(sheet->height()));
    ctx.notes.append(QStringLiteral("bar-alc: TX: ALC shows a fixed sheet value of %1 dB on Thetis's "
                                    "ALC scale (-30..+12 dB, red from 0 dB); not a live reading")
                         .arg(kSheetAlcDb));
}

// "Station": Longpath instruments mixed with Thetis meter faces, slice A.
QVector<EdgeEntry> stationEntries()
{
    const QJsonObject sliceA{{QStringLiteral("rxSource"), 1}};
    return {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kFilterType), {}, {}, sliceA},
            {QStringLiteral("meter.sMeter"), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kNeedleType), {}, {}, sliceA},
            {QStringLiteral("meter.crossNeedle"), {}, {}, sliceA},
            {QString::fromLatin1(LongpathContent::kBarType), {}, {}, sliceA}};
}

QString addStationContainer(SceneContext& ctx, DockEdge edge)
{
    return addSceneContainer(ctx, QStringLiteral("Station"), edge, ContentLayout::VerticalStack, stationEntries());
}

// After the data is fed (the poller and timers are stopped): one reading
// through MeterPoller::readingUpdated for each Longpath needle, as the
// poller would send for slice A, and one trace refresh for each filter
// pane from the fed pan.
void poseStation(SceneContext& ctx, const QString& id)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerContentHost* host = manager != nullptr ? manager->contentHost(id) : nullptr;
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (host == nullptr || poller == nullptr) {
        ctx.failure = QStringLiteral("no Station host or MeterPoller");
        return;
    }
    for (NeedleInstrument* needle : host->findChildren<NeedleInstrument*>()) {
        const QJsonObject context = needle->property("containerSourceContext").toJsonObject();
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, kSheetSignalPeakDbm);
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, kSheetSignalDbm);
        ctx.notes.append(QStringLiteral("Longpath needle: readingUpdated SignalPeak -68 then -73 dBm"));
    }
    for (BandwidthFilterPane* pane : host->findChildren<BandwidthFilterPane*>()) {
        for (QTimer* timer : pane->findChildren<QTimer*>()) {
            QMetaObject::invokeMethod(timer, "timeout");
        }
        ctx.notes.append(QStringLiteral("Longpath filter pane: one trace refresh from the fed pan"));
    }
    ctx.notes.append(QStringLiteral("Longpath bar (TX: SWR): no reading in receive, shows its dash"));
}

QString regionProvenance(const QString& typeId)
{
    return typeId.startsWith(QLatin1String("longpath.")) ? QStringLiteral("longpath-port")
                                                        : QStringLiteral("retained");
}

// The sidecar regions of the Station container: each entry's widget rect,
// the headers and the strips, in window coordinates.
QJsonArray stationRegions(SceneContext& ctx, const QString& id)
{
    QJsonArray regions;
    auto* manager = ctx.window->findChild<ContainerManager*>();
    ContainerWidget* container = manager->container(id);
    ContainerContentHost* host = manager->contentHost(id);
    auto windowRect = [&ctx](QWidget* w, const QRect& local) {
        return QRect(w->mapTo(ctx.window, local.topLeft()), local.size());
    };
    QRect viewport;
    if (auto* area = container->contentHolder()->findChild<QScrollArea*>()) {
        viewport = windowRect(area->viewport(), area->viewport()->rect());
    } else {
        viewport = windowRect(host, host->rect());
    }
    const ContainerDocument doc = host->captureDocument();
    for (const ContentEntry& entry : doc.contents) {
        const QRect rect = windowRect(host, host->entryBoundary(entry.id));
        QJsonObject r{{QStringLiteral("name"), entry.id},
                      {QStringLiteral("type"), entry.typeId},
                      {QStringLiteral("provenance"), regionProvenance(entry.typeId)},
                      {QStringLiteral("rect"), rectJson(rect)},
                      {QStringLiteral("visible_rect"), rectJson(rect.intersected(viewport))}};
        for (const auto& row : host->entryRows()) {
            if (row.entryId == entry.id && row.widget != nullptr) {
                r.insert(QStringLiteral("widget_class"),
                         QString::fromLatin1(row.widget->metaObject()->className()));
            }
        }
        regions.append(r);
        if (WorkspaceEntryHeader* header = host->entryHeader(entry.id); header != nullptr && header->isVisible()) {
            regions.append(QJsonObject{{QStringLiteral("name"), entry.id + QStringLiteral("-header")},
                                       {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                       {QStringLiteral("rect"), rectJson(windowRect(header, header->rect()))}});
        }
    }
    if (WorkspaceGroupHeader* header = container->workspaceHeader(); header != nullptr && header->isVisible()) {
        regions.append(QJsonObject{{QStringLiteral("name"), id + QStringLiteral("-group-header")},
                                   {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                   {QStringLiteral("rect"), rectJson(windowRect(header, header->rect()))}});
    }
    regions.append(QJsonObject{{QStringLiteral("name"), id + QStringLiteral("-viewport")},
                               {QStringLiteral("provenance"), QStringLiteral("proposed")},
                               {QStringLiteral("rect"), rectJson(viewport)}});
    if (auto* area = ctx.window->findChild<EdgeDockArea*>()) {
        const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("strip-left")},
                                                  {DockEdge::Right, QStringLiteral("strip-right")}};
        for (const auto& [edge, name] : edges) {
            if (EdgeDockStrip* strip = area->strip(edge); strip != nullptr && strip->isVisible()) {
                regions.append(QJsonObject{{QStringLiteral("name"), name},
                                           {QStringLiteral("provenance"), QStringLiteral("proposed")},
                                           {QStringLiteral("rect"), rectJson(windowRect(strip, strip->rect()))}});
            }
        }
    }
    return regions;
}

void sceneMixedContainer(SceneContext& ctx, const QString& name, HeaderMode header)
{
    auto stationId = std::make_shared<QString>();
    sceneProposedEdges(ctx, name, [stationId](SceneContext& c) {
        // Controller resolution: the Station container in the left strip;
        // the Main Panel stays right.
        *stationId = addStationContainer(c, DockEdge::Left);
    }, [stationId](SceneContext& c) {
        poseStation(c, *stationId);
        pump(kFinalSettleMs);
    }, header);
    if (!ctx.failure.isEmpty() || stationId->isEmpty()) {
        return;
    }
    const QJsonArray regions = stationRegions(ctx, *stationId);
    for (CapturedImage& img : ctx.images) {
        if (img.name == name) {
            img.extra[QStringLiteral("entry_regions")] = regions;
            img.extra[QStringLiteral("region_provenance")] = QStringLiteral(
                "longpath-port: Longpath instruments; retained: Thetis meter faces; "
                "proposed: headers, strips and the container viewport");
        }
    }
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
    attachRegions(img, dialog);
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

// ---- workspace profiles (workspace preview) --------------------------------

// The fixed "last saved" time the preview's profiles show.
QDateTime profileClock()
{
    return QDateTime(QDate(2026, 10, 7), QTime(14, 32), QTimeZone::utc());
}

QJsonObject rxSource(int rx)
{
    return QJsonObject{{QStringLiteral("rxSource"), rx}};
}

// A container bound to one slice: the header's source chip names it.
ContainerDocument onSlice(ContainerDocument d, int rx)
{
    d.config.insert(QStringLiteral("rxSource"), rx);
    return d;
}

// The DX top strip: room for the needles without taking half the window.
constexpr int kDxTopStripPx = 190;

// The four seeded profiles, in rail order (S, D, C, F).
enum class SceneProfile { Ssb, Dx, Contest, Ft8 };

QString profileName(SceneProfile p)
{
    switch (p) {
    case SceneProfile::Ssb: return QStringLiteral("SSB");
    case SceneProfile::Dx: return QStringLiteral("DX");
    case SceneProfile::Contest: return QStringLiteral("Contest");
    case SceneProfile::Ft8: return QStringLiteral("FT8");
    }
    return {};
}

struct SeededProfiles {
    QMap<SceneProfile, QString> ids;
    // Containers to pose after the data is fed: needles fed for slice A
    // (as MeterPoller would), filter panes given one trace refresh.
    QMap<SceneProfile, QStringList> needleHosts;
    QMap<SceneProfile, QStringList> filterHosts;
    QMap<SceneProfile, QString> description;
};

void placeMain(WorkspaceDocument& d, DockEdge edge)
{
    for (ContainerDocument& c : d.containers) {
        if (c.id == d.mainContainerId) {
            c.dockMode = DockMode::PanelDocked;
            c.edge = edge;
            c.visible = true;
        }
    }
}

// Commits `base` edited by `edit`, every container's header `header`.
bool commitArrangement(SceneContext& ctx, const WorkspaceDocument& base, HeaderMode header,
                       const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    WorkspaceDocument draft = base;
    draft.revision = manager->workspaceStore()->snapshot().revision;
    edit(draft, manager);
    for (ContainerDocument& c : draft.containers) {
        c.header = header;
    }
    if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
        return false;
    }
    pump(kSettleMs);
    return true;
}

// Seeds SSB, DX, Contest and FT8 through the same calls the rail and the
// manage dialog make (rename, save, New > From Current Arrangement), then
// switches to `show`.
bool seedProfiles(SceneContext& ctx, SceneProfile show, SeededProfiles& seeded)
{
    WorkspaceProfiles* profiles = ctx.window->workspaceProfiles();
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (profiles == nullptr || manager == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no workspace profiles: the workspace preview switch is off");
        return false;
    }
    if (profiles->ids().size() != 1) {
        ctx.failure = QStringLiteral("expected the first-run Default profile only");
        return false;
    }
    profiles->setClock(&profileClock);
    const WorkspaceDocument base = manager->workspaceStore()->snapshot();

    // SSB: Main Panel right, the Station mixed container left, Reveal.
    if (!commitArrangement(ctx, base, HeaderMode::Reveal, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(makeSceneContainer(m, QStringLiteral("Station"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, stationEntries()));
        })) {
        ctx.failure = QStringLiteral("could not commit the SSB arrangement");
        return false;
    }
    const QString first = profiles->activeId();
    if (!profiles->rename(first, QStringLiteral("SSB")).ok() || !profiles->save().ok()) {
        ctx.failure = QStringLiteral("could not rename and save SSB");
        return false;
    }
    seeded.ids[SceneProfile::Ssb] = first;
    seeded.needleHosts[SceneProfile::Ssb] = {QStringLiteral("preview-station")};
    seeded.filterHosts[SceneProfile::Ssb] = {QStringLiteral("preview-station")};
    seeded.description[SceneProfile::Ssb] = QStringLiteral(
        "Main Panel right; Station (Longpath frequency, filter, needle and bar with Thetis "
        "S-Meter and cross needle, slice A) left; every header Reveal");

    const auto create = [&](SceneProfile p, HeaderMode header,
                            const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit) {
        if (!commitArrangement(ctx, base, header, edit)) {
            ctx.failure = QStringLiteral("could not commit the %1 arrangement").arg(profileName(p));
            return false;
        }
        const auto r = profiles->createFromCurrent(profileName(p));
        if (!r.ok()) {
            ctx.failure = QStringLiteral("could not create %1: %2").arg(profileName(p), r.error);
            return false;
        }
        seeded.ids[p] = r.id;
        return true;
    };

    // DX: Main Panel right; a Frequency (Longpath) per slice left, with
    // source chips; signal needles for A and B on top; Always.
    if (!create(SceneProfile::Dx, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            const QString freq = QString::fromLatin1(LongpathContent::kFrequencyType);
            const QString needle = QString::fromLatin1(LongpathContent::kNeedleType);
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Frequency A"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, {{freq, {}, {}, rxSource(1)}}), 1));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Frequency B"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, {{freq, {}, {}, rxSource(2)}}), 2));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal A"), DockEdge::Top,
                                                   ContentLayout::VerticalStack, {{needle, {}, {}, rxSource(1)}}), 1));
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Signal B"), DockEdge::Top,
                                                   ContentLayout::VerticalStack, {{needle, {}, {}, rxSource(2)}}), 2));
            d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
        })) {
        return false;
    }
    seeded.needleHosts[SceneProfile::Dx] = {QStringLiteral("preview-signal-a"), QStringLiteral("preview-signal-b")};
    seeded.description[SceneProfile::Dx] = QStringLiteral(
        "Main Panel right; Frequency A and Frequency B (Longpath, source chips RX A / RX B) "
        "left; Signal A and Signal B (Longpath needles) top; every header Always");

    // Contest: all four strips, dense, Always.
    if (!create(SceneProfile::Contest, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(makeSceneContainer(m, QStringLiteral("Meters"), DockEdge::Top,
                                                   ContentLayout::FreeCanvas, metersEntries()));
            d.containers.append(makeSceneContainer(m, QStringLiteral("Levels"), DockEdge::Left,
                                                   ContentLayout::VerticalStack, levelsEntries()));
            d.containers.append(onSlice(makeSceneContainer(
                m, QStringLiteral("Filter A"), DockEdge::Left, ContentLayout::VerticalStack,
                {{QString::fromLatin1(LongpathContent::kFilterType), {}, {}, rxSource(1)}}), 1));
            d.containers.append(makeSceneContainer(m, QStringLiteral("Band"), DockEdge::Bottom,
                                                   ContentLayout::FreeCanvas, bandEntries()));
        })) {
        return false;
    }
    seeded.filterHosts[SceneProfile::Contest] = {QStringLiteral("preview-filter-a")};
    seeded.description[SceneProfile::Contest] = QStringLiteral(
        "Meters top, Levels and Filter A (Longpath filter passband, slice A) left, Main Panel "
        "right, Band bottom; every header Always");

    // FT8: a narrow right strip with only slice B's frequency and filter;
    // the Main Panel moves to the left strip; Hidden.
    if (!create(SceneProfile::Ft8, HeaderMode::Hidden, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Left);
            d.containers.append(onSlice(makeSceneContainer(
                m, QStringLiteral("FT8"), DockEdge::Right, ContentLayout::VerticalStack,
                {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, rxSource(2)},
                 {QString::fromLatin1(LongpathContent::kFilterType), {}, {}, rxSource(2)}}), 2));
            d.arrangement.strips[DockEdge::Right].thickness = EdgeDockStrip::kSideThicknessPx;
        })) {
        return false;
    }
    seeded.filterHosts[SceneProfile::Ft8] = {QStringLiteral("preview-ft8")};
    seeded.description[SceneProfile::Ft8] = QStringLiteral(
        "FT8 (Longpath frequency and filter passband, slice B) alone in a narrow right strip; "
        "Main Panel in the left strip; every header Hidden");

    const auto r = profiles->switchTo(seeded.ids.value(show));
    if (!r.ok()) {
        ctx.failure = QStringLiteral("could not switch to %1: %2").arg(profileName(show), r.error);
        return false;
    }
    pump(kSettleMs);
    return true;
}

// The peak a fed RX needle holds above the slice's level (slice A's -68
// over -73 dBm, as the Station scenes pose it).
constexpr double kFedPeakAboveDb = 5.0;

// Feeds every Longpath needle and bar under `root` bound to RX: Signal Peak
// the reading MeterPoller would send for its slice: the slice's posed
// signal level, after a peak kFedPeakAboveDb above it. Display-only: the
// readingUpdated signal the instruments listen to, no radio or Core.
// The seam a fed reading went through. Each Longpath needle and bar
// registers its context with MeterPoller::addReadingConsumer on creation;
// the poller is stopped (freeze), so the scene stands in for its timer
// frame and emits readingUpdated for the registered context.
QString consumerSeam(MeterPoller* poller)
{
    return QStringLiteral("MeterPoller::readingUpdated to a registered reading consumer "
                          "(addReadingConsumer; poller stopped, %1 consumers; display-only)")
        .arg(poller->readingConsumerCountForTest());
}

void feedSliceInstruments(SceneContext& ctx, QWidget* root)
{
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (poller == nullptr || root == nullptr) {
        return;
    }
    QList<QWidget*> instruments;
    for (NeedleInstrument* n : root->findChildren<NeedleInstrument*>()) {
        instruments.append(n);
    }
    for (BarInstrument* b : root->findChildren<BarInstrument*>()) {
        instruments.append(b);
    }
    for (QWidget* w : instruments) {
        if (!w->property("longpathPrimaryBinding").isValid()
            || w->property("longpathPrimaryBinding").toInt() != MeterBinding::SignalPeak) {
            continue;
        }
        const QJsonObject context = w->property("containerSourceContext").toJsonObject();
        // The host resolves rxSource (1-based) to sliceId (0-based).
        const int sliceId = context.contains(QStringLiteral("sliceId"))
                                ? context.value(QStringLiteral("sliceId")).toInt()
                                : context.value(QStringLiteral("rxSource")).toInt() - 1;
        SliceModel* slice = sliceId >= 0 ? ctx.model->sliceById(sliceId) : nullptr;
        if (slice == nullptr) {
            continue;
        }
        const int rx = sliceId + 1;
        const double level = slice->signalStrengthDbm();
        const QString unavailable = poller->bindingUnavailableReason(MeterBinding::SignalPeak);
        if (!unavailable.isEmpty()) {
            ctx.notes.append(QStringLiteral("RX: Signal Peak unavailable (%1); the instrument shows no value")
                                 .arg(unavailable));
        }
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level + kFedPeakAboveDb);
        emit poller->readingUpdated(context, MeterBinding::SignalPeak, level);
        const QString entry = w->property(kRegionNameProperty).toString().isEmpty()
                                  ? w->property("containerEntryId").toString()
                                  : w->property(kRegionNameProperty).toString();
        ctx.fedReadings.append(QJsonObject{
            {QStringLiteral("instrument"), QString::fromLatin1(w->metaObject()->className())},
            {QStringLiteral("entry"), entry},
            {QStringLiteral("slice"), QString(QChar('A' + rx - 1))},
            {QStringLiteral("binding"), QStringLiteral("RX: Signal Peak")},
            {QStringLiteral("values_dbm"), QJsonArray{level + kFedPeakAboveDb, level}},
            {QStringLiteral("seam"), consumerSeam(poller)}});
    }
}

void poseProfile(SceneContext& ctx, const SeededProfiles& seeded, SceneProfile p)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    auto* poller = ctx.window->findChild<MeterPoller*>();
    for (const QString& id : seeded.needleHosts.value(p)) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr || poller == nullptr) {
            continue;
        }
        feedSliceInstruments(ctx, host);
        ctx.notes.append(QStringLiteral("%1 Longpath needles: readingUpdated RX: Signal Peak for each "
                                        "needle's slice, as MeterPoller sends (see fed_readings)").arg(id));
    }
    for (const QString& id : seeded.filterHosts.value(p)) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr) {
            continue;
        }
        for (BandwidthFilterPane* pane : host->findChildren<BandwidthFilterPane*>()) {
            for (QTimer* timer : pane->findChildren<QTimer*>()) {
                QMetaObject::invokeMethod(timer, "timeout");
            }
            ctx.notes.append(QStringLiteral("%1 Longpath filter pane: one trace refresh from the fed pan").arg(id));
        }
    }
    if (p == SceneProfile::Ssb) {
        ctx.notes.append(QStringLiteral("Station Longpath bar (TX: SWR): no reading in receive, shows its dash"));
    }
}

QJsonArray sliceStates(SceneContext& ctx)
{
    QJsonArray out;
    for (SliceModel* s : ctx.model->slices()) {
        out.append(QJsonObject{{QStringLiteral("slice"), QString(QChar('A' + s->sliceIndex()))},
                               {QStringLiteral("frequency_hz"), s->frequency()},
                               {QStringLiteral("mode"), SliceModel::modeName(s->dspMode())},
                               {QStringLiteral("filter_low_hz"), s->filterLow()},
                               {QStringLiteral("filter_high_hz"), s->filterHigh()}});
    }
    return out;
}

QLabel* titleLabel(TitleBar* bar)
{
    for (QLabel* label : bar->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (label->text() == QStringLiteral("NereusSDR")) {
            return label;
        }
    }
    return nullptr;
}

// Logical-px measurements of the rail against the network readout and
// the title, in window coordinates.
QJsonObject railMeasure(SceneContext& ctx)
{
    ProfileRailDock* dock = ctx.window->profileRailDock();
    auto* bar = ctx.window->findChild<TitleBar*>();
    QLabel* title = bar != nullptr ? titleLabel(bar) : nullptr;
    if (dock == nullptr || title == nullptr) {
        return QJsonObject{{QStringLiteral("error"), QStringLiteral("no rail dock or title")}};
    }
    QWidget* readout = bar->connectionSegment();
    const int readoutRight = readout->mapTo(ctx.window, QPoint(readout->width(), 0)).x();
    const int textWidth = title->fontMetrics().horizontalAdvance(title->text());
    const int titleTextLeft = title->mapTo(ctx.window, QPoint((title->width() - textWidth) / 2, 0)).x();
    QPushButton* badge = dock->collapsedBadge();
    const QRect badgeRect(badge->mapTo(ctx.window, QPoint(0, 0)), badge->size());
    const int openWidth = dock->isOpen() ? dock->popover()->width() : dock->popover()->sizeHint().width();
    const int openLeft = badgeRect.x() + badgeRect.width() - openWidth;
    QJsonArray barChildren;
    for (QWidget* w : bar->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (w->isVisible()) {
            barChildren.append(QStringLiteral("%1 %2x%3 at x %4")
                                   .arg(QString::fromLatin1(w->metaObject()->className()))
                                   .arg(w->width()).arg(w->height())
                                   .arg(w->mapTo(ctx.window, QPoint(0, 0)).x()));
        }
    }
    return QJsonObject{
        {QStringLiteral("window_width"), ctx.window->width()},
        {QStringLiteral("collapsed_badge"), rectJson(badgeRect)},
        {QStringLiteral("collapsed_width"), badgeRect.width()},
        {QStringLiteral("collapsed_height"), badgeRect.height()},
        {QStringLiteral("open_width"), openWidth},
        {QStringLiteral("open_height"), dock->isOpen() ? dock->popover()->height()
                                                       : dock->popover()->sizeHint().height()},
        {QStringLiteral("open_left"), openLeft},
        {QStringLiteral("network_readout_right"), readoutRight},
        {QStringLiteral("title_text_left"), titleTextLeft},
        {QStringLiteral("free_readout_to_title"), titleTextLeft - readoutRight},
        {QStringLiteral("free_readout_to_badge"), badgeRect.x() - readoutRight},
        {QStringLiteral("open_covers_readout_px"), qMax(0, readoutRight - openLeft)},
        {QStringLiteral("title_bar_children"), barChildren},
    };
}

QJsonArray railRegions(SceneContext& ctx)
{
    QJsonArray regions;
    ProfileRailDock* dock = ctx.window->profileRailDock();
    if (dock == nullptr) {
        return regions;
    }
    QPushButton* badge = dock->collapsedBadge();
    regions.append(QJsonObject{
        {QStringLiteral("name"), QStringLiteral("profile-rail-collapsed")},
        {QStringLiteral("provenance"), QStringLiteral("proposed")},
        {QStringLiteral("note"), QStringLiteral("NereusSDR wrapper; Longpath pill style")},
        {QStringLiteral("rect"), rectJson(QRect(badge->mapTo(ctx.window, QPoint(0, 0)), badge->size()))}});
    if (dock->isOpen()) {
        regions.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("profile-rail-popover")},
            {QStringLiteral("provenance"), QStringLiteral("proposed")},
            {QStringLiteral("rect"), rectJson(dock->popover()->geometry())}});
        ProfileRail* rail = dock->rail();
        regions.append(QJsonObject{
            {QStringLiteral("name"), QStringLiteral("profile-rail")},
            {QStringLiteral("provenance"), QStringLiteral("longpath-port")},
            {QStringLiteral("rect"), rectJson(QRect(rail->mapTo(ctx.window, QPoint(0, 0)), rail->size()))}});
    }
    return regions;
}

// Paints a separate top-level (menu, tooltip, dialog) into a window image
// where it sits on screen.
void compositeOver(CapturedImage& img, QWidget* window, QWidget* top)
{
    if (top == nullptr || !top->isVisible()) {
        return;
    }
    const QImage over = grabWidget(top);
    const QPoint at = top->mapToGlobal(QPoint(0, 0)) - window->mapToGlobal(QPoint(0, 0));
    QPainter painter(&img.image);
    painter.drawImage(QRect(at, top->size()), over);
}

QWidget* visibleToolTip()
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w->isVisible() && w->inherits("QTipLabel")) {
            return w;
        }
    }
    return nullptr;
}

void addTopStripCropImage(SceneContext& ctx, const QString& name)
{
    // At the crop profile captureWindow already cut it.
    if (ctx.crops) {
        return;
    }
    for (const CapturedImage& img : ctx.images) {
        if (img.name != name) {
            continue;
        }
        CapturedImage full = img;
        full.crops.clear();
        addCrop(full.crops, QStringLiteral("top-strip"), ctx.window, ctx.window->findChild<TitleBar*>(), full.dpr);
        appendCropImages(ctx, full, name);
        return;
    }
}

void annotateProfileImages(SceneContext& ctx, const QString& name, const SeededProfiles& seeded,
                           SceneProfile shown)
{
    QJsonObject profiles;
    for (auto it = seeded.ids.constBegin(); it != seeded.ids.constEnd(); ++it) {
        profiles.insert(profileName(it.key()), it.value());
    }
    const QJsonArray slices = sliceStates(ctx);
    const QJsonObject measure = railMeasure(ctx);
    const QJsonArray regions = railRegions(ctx);
    for (CapturedImage& img : ctx.images) {
        if (img.name != name && img.extra.value(QStringLiteral("source")).toString() != name) {
            continue;
        }
        img.extra[QStringLiteral("slice_states")] = slices;
        img.extra[QStringLiteral("active_profile")] = profileName(shown);
        img.extra[QStringLiteral("arrangement")] = seeded.description.value(shown);
        img.extra[QStringLiteral("profiles")] = profiles;
        img.extra[QStringLiteral("profile_saved_at")] = profileClock().toString(Qt::ISODate);
        img.extra[QStringLiteral("rail_measure")] = measure;
        img.extra[QStringLiteral("rail_regions")] = regions;
        img.extra[QStringLiteral("region_provenance")] = QStringLiteral(
            "longpath-port: the profile rail (fanned out) and the Longpath instruments; proposed: "
            "profiles, the collapsed badge wrapper, the popover, arrangements, the dialog");
    }
}

// One profile scene: the two-band station, profiles seeded, `shown`
// active, the rail at rest unless `pose` changes it.
// Paints `tops` into the window image `name` and cuts its crops again
// from the composited image.
void compositeAndRecrop(SceneContext& ctx, const QString& name, const QList<QWidget*>& tops)
{
    for (int i = ctx.images.size() - 1; i >= 0; --i) {
        if (ctx.images[i].extra.value(QStringLiteral("source")).toString() == name) {
            ctx.images.removeAt(i);
        }
    }
    for (CapturedImage& img : ctx.images) {
        if (img.name != name) {
            continue;
        }
        QJsonArray regions = img.extra.value(QStringLiteral("regions")).toArray();
        for (QWidget* top : tops) {
            compositeOver(img, ctx.window, top);
            if (top != nullptr && top->isVisible()) {
                appendRegions(regions, top, top->mapToGlobal(QPoint(0, 0)) - ctx.window->mapToGlobal(QPoint(0, 0)),
                              img.logicalSize);
            }
        }
        img.extra[QStringLiteral("regions")] = regions;
        const CapturedImage full = img;
        appendCropImages(ctx, full, name);
        return;
    }
}

// The scene runs inside the event loop, so processEvents never runs the
// deleteLater() of the badges the rail dropped on its last rebuild; live,
// the loop does on return. Hide them so they are not painted under the
// current badges. (Flushing every deferred delete instead also drops what
// the poses feed, so it is limited to the rail.)
void hideStaleRailBadges(ProfileRail* rail)
{
    QSet<QPushButton*> current;
    for (const QString& id : rail->badges()) {
        current.insert(rail->badgeButton(id));
    }
    for (QPushButton* b : rail->findChildren<QPushButton*>(QStringLiteral("profileRailBadge"),
                                                          Qt::FindDirectChildrenOnly)) {
        if (!current.contains(b)) {
            b->hide();
        }
    }
}

void sceneProfile(SceneContext& ctx, const QString& name, SceneProfile shown,
                  const std::function<void(SceneContext&)>& pose = {},
                  const std::function<void(SceneContext&)>& afterCapture = {},
                  bool topStripCrop = false)
{
    ctx.provenance = QStringLiteral("proposed");
    ctx.extraCrops = addStripCrops;
    auto seeded = std::make_shared<SeededProfiles>();
    runWindowScene(ctx, twoBandPoses(), 0, [seeded, shown](SceneContext& c) {
        seedProfiles(c, shown, *seeded);
    }, [seeded, shown, pose](SceneContext& c) {
        poseProfile(c, *seeded, shown);
        if (pose) {
            pose(c);
        }
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
    if (afterCapture) {
        afterCapture(ctx);
    }
    if (topStripCrop) {
        addTopStripCropImage(ctx, name);
    }
    annotateProfileImages(ctx, name, *seeded, shown);
}

// The display-only transmit state of slice A (baseline-tx), before the
// freeze.
void poseTxBeforeFreeze(SceneContext& c)
{
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
}

// The transmit display and S-meter levels, after the data is fed.
void poseTxAfterFeed(SceneContext& c)
{
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
}

// ---- coverage scenes (workspace preview) -----------------------------------

// The ALC level the transmit scenes pose (baseline-tx records it as not
// shown on the default panel).
constexpr double kTxAlcDb = -2.0;
// A not-implemented plate: its width and its gap below what it explains.
constexpr int kPlateWidthPx = 270;
constexpr int kPlateGapPx = 6;
// Container Settings is sized to the window less this much.
constexpr int kDialogInsetPx = 80;
// The free container's clearance from the dB scale and the time scale.
constexpr int kFreeScaleClearancePx = 72;
constexpr int kFreeBottomClearancePx = 24;
// The placeholder per-MAC scope tx-processing seeds TX profiles under,
// in the throwaway test profile (a locally administered address).
constexpr auto kPreviewTxProfileScope = "02:00:00:00:00:00";
// At most this many window growths for a strip's full-content image.
constexpr int kFullStripGrowSteps = 4;

// Applets the source has but MainWindow does not construct (their lines
// are commented out in MainWindow.cpp). Each is plain UI over NyiOverlay
// markers with no model, network or device access, so the preview builds
// them standalone as container content, tagged exists-not-wired.
constexpr auto kPreviewCwxType = "preview.cwx";
constexpr auto kPreviewDvkType = "preview.dvk";
constexpr auto kPreviewDigitalType = "preview.digital";
constexpr auto kPreviewEqType = "preview.eq";

// Every slice's state on every image of the scene.
void annotateSlices(SceneContext& ctx)
{
    const QJsonArray slices = sliceStates(ctx);
    for (CapturedImage& img : ctx.images) {
        img.extra[QStringLiteral("slice_states")] = slices;
    }
}

void addAllStripCrops(SceneContext& ctx, QVector<CropRect>& crops, double dpr)
{
    addStripCrops(ctx, crops, dpr);
    if (auto* area = ctx.window->findChild<EdgeDockArea*>()) {
        addCrop(crops, QStringLiteral("strip-right"), ctx.window, area->strip(DockEdge::Right), dpr);
    }
}

// Takes `typeIds` out of the Main Panel, so a singleton can be placed in
// another container (its "Move").
void takeFromMain(WorkspaceDocument& d, const QStringList& typeIds)
{
    for (ContainerDocument& c : d.containers) {
        if (c.id == d.mainContainerId) {
            c.contents.removeIf([&typeIds](const ContentEntry& e) { return typeIds.contains(e.typeId); });
        }
    }
}

bool registerPreviewApplets(SceneContext& ctx)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->contentRegistry() == nullptr || manager->workspaceStore() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace");
        return false;
    }
    ContainerContentRegistry* registry = manager->contentRegistry();
    RadioModel* model = ctx.model;
    using Make = std::function<QWidget*(RadioModel*, QWidget*)>;
    const auto add = [registry, model](const char* typeId, const QString& title, const Make& make) {
        const QString id = QString::fromLatin1(typeId);
        if (registry->hasWidgetFactory(id)) {
            return;
        }
        registry->registerWidgetFactory(id, {id, title, false, true, {}},
            [model, make](const ContentEntry&, QWidget* parent, ContentRenderMode) -> QWidget* {
                QWidget* w = make(model, parent);
                tagProvenance(w, provNotWired());
                return w;
            });
    };
    add(kPreviewCwxType, QStringLiteral("CWX (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new CwxApplet(m, p); });
    add(kPreviewDvkType, QStringLiteral("DVK (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new DvkApplet(m, p); });
    add(kPreviewDigitalType, QStringLiteral("Digital (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new DigitalApplet(m, p); });
    add(kPreviewEqType, QStringLiteral("EQ (not wired)"),
        [](RadioModel* m, QWidget* p) -> QWidget* { return new EqApplet(m, p); });
    ctx.notes.append(QStringLiteral(
        "CwxApplet, DvkApplet, DigitalApplet and EqApplet: constructed standalone through preview "
        "widget factories (MainWindow leaves them commented out); exists-not-wired"));
    return true;
}

bool commitScene(SceneContext& ctx, HeaderMode header,
                 const std::function<void(WorkspaceDocument&, ContainerManager*)>& edit)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    if (manager == nullptr || manager->workspaceStore() == nullptr || manager->contentRegistry() == nullptr) {
        ctx.failure = QStringLiteral("no container workspace: the workspace preview switch is off");
        return false;
    }
    if (!commitArrangement(ctx, manager->workspaceStore()->snapshot(), header, edit)) {
        ctx.failure = QStringLiteral("could not commit the scene arrangement");
        return false;
    }
    return true;
}

QJsonObject bindingConfig(int binding)
{
    return QJsonObject{{QString::fromLatin1(LongpathContent::kBindingKey), binding}};
}

// A plate where a control does not exist in this build. `message` is the
// plain-English text on the plate; `reason` (the technical cause) goes to
// the sidecar notes, never onto the plate.
QLabel* addNotImplementedPlate(SceneContext& ctx, QWidget* near, const QString& what, const QString& message,
                               const QString& reason, bool below = true)
{
    QWidget* window = ctx.window;
    ctx.notes.append(QStringLiteral("not-implemented plate \"%1\": %2").arg(message, reason));
    auto* plate = new QLabel(message, window);
    plate->setObjectName(QStringLiteral("previewNotImplementedPlate"));
    plate->setWordWrap(true);
    plate->setStyleSheet(QStringLiteral(
        "QLabel { background: #1c1c20; color: #c8c8cd; border: 1px dashed #7e7e85; "
        "border-radius: 6px; padding: 6px 8px; font-size: 11px; }"));
    plate->setFixedWidth(below ? kPlateWidthPx : qMin(kPlateWidthPx, near->width() - 2 * kPlateGapPx));
    plate->adjustSize();
    QPoint at = below ? near->mapTo(window, QPoint(0, near->height() + kPlateGapPx))
                      : near->mapTo(window, QPoint(kPlateGapPx, kPlateGapPx));
    at.setX(qBound(0, at.x(), window->width() - plate->width()));
    at.setY(qBound(0, at.y(), window->height() - plate->height()));
    plate->move(at);
    plate->raise();
    plate->show();
    tagProvenance(plate, provNotImplemented());
    plate->setProperty(kRegionNameProperty, QStringLiteral("plate: %1").arg(what));
    return plate;
}

// clean-spectrum: one pan, no docked containers, one free container with
// a Longpath frequency and needle for slice A, header Reveal idle.
// clean-spectrum-reveal (`revealed`): the same with that header shown, as
// when the pointer has rested in the container's top band.
void sceneCleanSpectrum(SceneContext& ctx, bool revealed)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QString id = QStringLiteral("preview-instruments");
    runWindowScene(ctx, defaultPoses(), 0, [id](SceneContext& c) {
        if (!commitScene(c, HeaderMode::Reveal, [](WorkspaceDocument& d, ContainerManager* m) {
                for (ContainerDocument& cd : d.containers) {
                    if (cd.id == d.mainContainerId) {
                        cd.visible = false;
                    }
                }
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("Instruments"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QString::fromLatin1(LongpathContent::kFrequencyType), {}, {}, rxSource(1)},
                     {QString::fromLatin1(LongpathContent::kNeedleType), {}, {}, rxSource(1)}}), 1));
            })) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* box = manager->container(id);
        if (box == nullptr) {
            c.failure = QStringLiteral("the Instruments container was not created");
            return;
        }
        // Options > Place Free, then moved by its header to the lower right.
        emit box->placeFreeRequested();
        pump(kSettleMs);
        QWidget* pane = box->parentWidget();
        if (pane == nullptr) {
            c.failure = QStringLiteral("the Instruments container was not placed free");
            return;
        }
        WorkspaceDocument draft = manager->workspaceStore()->snapshot();
        for (auto& d : draft.containers) {
            if (d.id == id) {
                const QSize size = d.geometry.isValid() ? d.geometry.size() : box->size();
                d.geometry = QRect(QPoint(pane->width() - size.width() - kFreeScaleClearancePx,
                                          pane->height() - size.height() - kFreeBottomClearancePx),
                                   size);
                c.notes.append(QStringLiteral("Instruments placed free at %1,%2 %3x%4 in a %5x%6 pane")
                                   .arg(d.geometry.x()).arg(d.geometry.y())
                                   .arg(size.width()).arg(size.height())
                                   .arg(pane->width()).arg(pane->height()));
            }
        }
        if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
            c.failure = QStringLiteral("could not move the Instruments container");
        }
    }, [id, revealed](SceneContext& c) {
        // MeterPoller polls Longpath instruments on their own; with no
        // radio the needle gets slice A's reading through the poller's
        // signal, as it would live.
        auto* manager = c.window->findChild<ContainerManager*>();
        feedSliceInstruments(c, manager->contentHost(id));
        if (revealed) {
            ContainerWidget* box = manager->container(id);
            if (box == nullptr) {
                c.failure = QStringLiteral("the Instruments container is gone");
                return;
            }
            // The pointer has been in the top band for 120 ms.
            box->setHeaderRevealedForPreview(true);
            c.notes.append(QStringLiteral("Instruments: ContainerWidget::setHeaderRevealedForPreview(true)"));
        }
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    ctx.notes.append(QStringLiteral("Main Panel hidden (visible false); Instruments: Place Free, Header Reveal, "
                                    "%1").arg(revealed ? QStringLiteral("header shown") : QStringLiteral("pointer away")));
    noteStrips(ctx);
    captureWindow(ctx, revealed ? QStringLiteral("clean-spectrum-reveal") : QStringLiteral("clean-spectrum"));
    annotateSlices(ctx);
}

// How far the scroll areas in `strip` (its own and each container's) are
// scrolled past their viewports along the strip's axis, in logical px.
int stripOverflow(EdgeDockStrip* strip)
{
    const bool vertical = strip->orientation() == Qt::Vertical;
    int overflow = 0;
    for (QScrollArea* sa : strip->findChildren<QScrollArea*>()) {
        if (sa->widget() == nullptr || !sa->isVisibleTo(strip)) {
            continue;
        }
        overflow += vertical ? sa->verticalScrollBar()->maximum() : sa->horizontalScrollBar()->maximum();
    }
    return overflow;
}

// `<scene>--strip-<edge>-full`: each strip that scrolls, rendered with its
// whole content showing. The window is grown along the strip's axis until
// nothing in the strip scrolls, the strip is grabbed, and the window is
// put back; the window image (taken before) stays as the window looks.
void captureStripsFull(SceneContext& ctx, const QString& scene)
{
    auto* area = ctx.window->findChild<EdgeDockArea*>();
    if (area == nullptr) {
        return;
    }
    const QPair<DockEdge, QString> edges[] = {{DockEdge::Left, QStringLiteral("strip-left")},
                                              {DockEdge::Right, QStringLiteral("strip-right")},
                                              {DockEdge::Top, QStringLiteral("strip-top")},
                                              {DockEdge::Bottom, QStringLiteral("strip-bottom")}};
    const QSize original = ctx.window->size();
    for (const auto& [edge, edgeName] : edges) {
        EdgeDockStrip* strip = area->strip(edge);
        if (strip == nullptr || !strip->isVisible()) {
            continue;
        }
        const int before = stripOverflow(strip);
        if (before <= 0) {
            continue;
        }
        const bool vertical = strip->orientation() == Qt::Vertical;
        QSize grown = original;
        int overflow = before;
        for (int step = 0; step < kFullStripGrowSteps && overflow > 0; ++step) {
            grown += vertical ? QSize(0, overflow) : QSize(overflow, 0);
            ctx.window->resize(grown);
            pump(kSettleMs);
            overflow = stripOverflow(strip);
        }
        const QString name = scene + QStringLiteral("--") + edgeName + QStringLiteral("-full");
        CapturedImage img;
        img.name = name;
        img.kind = QStringLiteral("detail");
        img.image = grabWidget(strip);
        img.logicalSize = strip->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("widget_class")] = QString::fromLatin1(strip->metaObject()->className());
        img.extra[QStringLiteral("full_content")] = QJsonObject{
            {QStringLiteral("scrolled_px_in_window"), before},
            {QStringLiteral("window_grown_to"), QJsonArray{grown.width(), grown.height()}},
            {QStringLiteral("still_scrolled_px"), overflow},
            {QStringLiteral("method"), QStringLiteral(
                 "the window grown along the strip's axis until no scroll area in the strip scrolls, "
                 "the strip grabbed, the window restored")}};
        attachRegions(img, strip);
        ctx.images.append(img);
        ctx.notes.append(QStringLiteral("%1: %2 px scrolled in the window; grabbed with the window at %3x%4 "
                                        "(%5 px still scrolled)")
                             .arg(name).arg(before).arg(grown.width()).arg(grown.height()).arg(overflow));
        ctx.window->resize(original);
        pump(kSettleMs);
    }
}

// cw-digital: slice A 14.030 CW and slice B 7.074 DIGU.
QVector<PanPose> cwDigitalPoses()
{
    const QVector<PanPose> four = fourRxPoses();
    PanPose a = four.at(2);
    a.panId = panIdAt(0);
    a.spectrum.panId = a.panId;
    PanPose b = four.at(1);
    b.panId = panIdAt(1);
    b.spectrum.panId = b.panId;
    return {a, b};
}

void sceneCwDigital(SceneContext& ctx)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    runWindowScene(ctx, cwDigitalPoses(), 0, [](SceneContext& c) {
        if (!registerPreviewApplets(c)) {
            return;
        }
        if (!commitScene(c, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
                placeMain(d, DockEdge::Left);
                takeFromMain(d, {QStringLiteral("applet:PHCW")});
                d.containers.append(makeSceneContainer(
                    m, QStringLiteral("CW and Digital"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QStringLiteral("applet:PHCW"), {}},
                     {QString::fromLatin1(kPreviewCwxType), {}},
                     {QString::fromLatin1(kPreviewDvkType), {}},
                     {QString::fromLatin1(kPreviewDigitalType), {}}}));
            })) {
            return;
        }
        auto* phcw = c.window->findChild<PhoneCwApplet*>();
        if (phcw == nullptr) {
            c.failure = QStringLiteral("no PhoneCwApplet");
            return;
        }
        phcw->showPage(1);
        c.notes.append(QStringLiteral("Main Panel in the left strip; Phone / CW moved to the right strip, "
                                      "PhoneCwApplet::showPage(1) (its CW page)"));
    }, [](SceneContext& c) {
        // The CW page is not built in this build: showPage(1) falls back to
        // the Phone page, so a plate says so over the applet's top.
        if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Cwx)) {
            return;
        }
        auto* phcw = c.window->findChild<PhoneCwApplet*>();
        addNotImplementedPlate(c, phcw, QStringLiteral("Phone / CW, CW page"),
                               QStringLiteral("CW page: not built yet in NereusSDR"),
                               QStringLiteral("the Phone / CW applet's CW page is gated by "
                                              "UnbuiltFeature::Cwx; showPage(1) shows the Phone page"),
                               false);
        c.notes.append(QStringLiteral("Phone / CW CW page: not built (UnbuiltFeature::Cwx); the applet shows "
                                      "its Phone page and a not-implemented plate covers its top"));
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("cw-digital"));
    captureStripsFull(ctx, QStringLiteral("cw-digital"));
    annotateSlices(ctx);
}

// After the data is fed: each Longpath TX instrument gets the reading
// MeterPoller::publishGlobalReading would send every context in transmit.
void poseLongpathTx(SceneContext& ctx, const QStringList& hostIds)
{
    auto* manager = ctx.window->findChild<ContainerManager*>();
    auto* poller = ctx.window->findChild<MeterPoller*>();
    if (manager == nullptr || poller == nullptr) {
        ctx.failure = QStringLiteral("no ContainerManager or MeterPoller");
        return;
    }
    const QHash<int, double> values{{MeterBinding::TxPower, kTxForwardWatts},
                                    {MeterBinding::TxSwr, kTxSwr},
                                    {MeterBinding::TxAlc, kTxAlcDb}};
    for (const QString& id : hostIds) {
        ContainerContentHost* host = manager->contentHost(id);
        if (host == nullptr) {
            ctx.failure = QStringLiteral("no %1 container").arg(id);
            return;
        }
        QList<QWidget*> instruments;
        for (NeedleInstrument* n : host->findChildren<NeedleInstrument*>()) {
            instruments.append(n);
        }
        for (BarInstrument* b : host->findChildren<BarInstrument*>()) {
            instruments.append(b);
        }
        for (QWidget* w : instruments) {
            const int binding = w->property("longpathPrimaryBinding").toInt();
            const QJsonObject context = w->property("containerSourceContext").toJsonObject();
            emit poller->readingUpdated(context, binding, values.value(binding));
            ctx.fedReadings.append(QJsonObject{
                {QStringLiteral("instrument"), QString::fromLatin1(w->metaObject()->className())},
                {QStringLiteral("entry"), id},
                {QStringLiteral("context"), QStringLiteral("TX (every context)")},
                {QStringLiteral("binding"), binding},
                {QStringLiteral("values"), QJsonArray{values.value(binding)}},
                {QStringLiteral("seam"), consumerSeam(poller)}});
            ctx.notes.append(QStringLiteral("%1 %2: readingUpdated binding %3 = %4, as MeterPoller "
                                            "publishes a transmit reading to every context")
                                 .arg(id, QString::fromLatin1(w->metaObject()->className()))
                                 .arg(binding).arg(values.value(binding)));
        }
    }
}

// The TX profile selector reads MicProfileManager, which is scoped by the
// radio's MAC and stays empty until a radio connects. The scene gives it a
// placeholder scope in this process's throwaway test profile and runs its
// own first-launch load(), which writes "Default" (active) and the factory
// profiles there, as the first connect would. No radio, Core or real
// settings are reached.
QJsonObject seedTxProfiles(SceneContext& ctx)
{
    MicProfileManager* profiles = ctx.model->micProfileManager();
    if (profiles == nullptr || !profiles->profileNames().isEmpty()) {
        return {};
    }
    profiles->setMacAddress(QString::fromLatin1(kPreviewTxProfileScope));
    profiles->load();
    ctx.notes.append(QStringLiteral("TX profiles seeded in the test profile (see tx_profile_seed)"));
    return QJsonObject{
        {QStringLiteral("scope"), QString::fromLatin1(kPreviewTxProfileScope)},
        {QStringLiteral("method"), QStringLiteral(
             "MicProfileManager::setMacAddress(placeholder) then load() (its first-launch seed) in the "
             "isolated test profile; no radio connected")},
        {QStringLiteral("profiles"), QJsonArray::fromStringList(profiles->profileNames())},
        {QStringLiteral("active"), profiles->activeProfileName()}};
}

void sceneTxProcessing(SceneContext& ctx)
{
    // Display-only transmit state, as baseline-tx. Refuse to run if a
    // connection exists: these seams must never reach a radio.
    if (ctx.model->connection() != nullptr) {
        ctx.failure = QStringLiteral("a RadioConnection exists; refusing the transmit scene");
        return;
    }
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QStringList meterHosts = {QStringLiteral("preview-tx-power"), QStringLiteral("preview-swr"),
                                    QStringLiteral("preview-alc")};
    QJsonObject txProfileSeed;
    runWindowScene(ctx, twoBandPoses(), 0, [&txProfileSeed](SceneContext& c) {
        if (!registerPreviewApplets(c)) {
            return;
        }
        if (!commitScene(c, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
                placeMain(d, DockEdge::Left);
                takeFromMain(d, {QStringLiteral("applet:TX"), QStringLiteral("applet:pure_signal"),
                                 QStringLiteral("applet:mod_monitor")});
                d.containers.append(makeSceneContainer(
                    m, QStringLiteral("TX Processing"), DockEdge::Right, ContentLayout::VerticalStack,
                    {{QStringLiteral("applet:TX"), {}},
                     {QString::fromLatin1(kPreviewEqType), {}},
                     {QStringLiteral("applet:pure_signal"), {}},
                     {QStringLiteral("applet:mod_monitor"), {}}}));
                const QString needle = QString::fromLatin1(LongpathContent::kNeedleType);
                const QString bar = QString::fromLatin1(LongpathContent::kBarType);
                d.containers.append(makeSceneContainer(m, QStringLiteral("TX Power"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{needle, {}, {}, {}, bindingConfig(MeterBinding::TxPower)}}));
                d.containers.append(makeSceneContainer(m, QStringLiteral("SWR"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{needle, {}, {}, {}, bindingConfig(MeterBinding::TxSwr)}}));
                d.containers.append(makeSceneContainer(m, QStringLiteral("ALC"), DockEdge::Top,
                    ContentLayout::VerticalStack, {{bar, {}, {}, {}, bindingConfig(MeterBinding::TxAlc)}}));
                d.arrangement.strips[DockEdge::Top].thickness = kDxTopStripPx;
            })) {
            return;
        }
        c.notes.append(QStringLiteral("Main Panel in the left strip; TX, PureSignal and Mod Monitor moved "
                                      "to the right strip with EQ; TX Power, SWR and ALC on top"));
        txProfileSeed = seedTxProfiles(c);
        poseTxBeforeFreeze(c);
    }, [meterHosts](SceneContext& c) {
        poseTxAfterFeed(c);
        poseLongpathTx(c, meterHosts);
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    auto* tx = ctx.window->findChild<TxApplet*>();
    if (tx == nullptr) {
        ctx.failure = QStringLiteral("no TxApplet");
        return;
    }
    ctx.notes.append(QStringLiteral("TxApplet TX profile selector: %1")
                         .arg(tx->profileCombo() != nullptr && tx->profileCombo()->isVisible()
                                  ? QStringLiteral("visible, \"%1\"").arg(tx->profileCombo()->currentText())
                                  : QStringLiteral("not visible")));
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("tx-processing"));
    for (CapturedImage& img : ctx.images) {
        if (img.name == QLatin1String("tx-processing") && !txProfileSeed.isEmpty()) {
            img.extra[QStringLiteral("tx_profile_seed")] = txProfileSeed;
        }
    }
    captureStripsFull(ctx, QStringLiteral("tx-processing"));
    // The CFC and TX EQ dialogs over the same state, alone.
    auto* action = ctx.window->findChild<QAction*>(QStringLiteral("toolsTxEqualizer"));
    if (action == nullptr) {
        ctx.failure = QStringLiteral("no toolsTxEqualizer action");
        return;
    }
    action->trigger();
    TxEqDialog* eq = visibleTopLevel<TxEqDialog>();
    captureDialog(ctx, eq, QStringLiteral("tx-processing--tx-eq"));
    if (eq != nullptr) {
        eq->close();
    }
    tx->requestOpenCfcDialog();
    TxCfcDialog* cfc = visibleTopLevel<TxCfcDialog>();
    captureDialog(ctx, cfc, QStringLiteral("tx-processing--cfc"));
    if (cfc != nullptr) {
        cfc->close();
    }
    annotateSlices(ctx);
}

QListWidgetItem* rowContaining(QListWidget* list, const QString& text)
{
    for (int i = 0; list != nullptr && i < list->count(); ++i) {
        if (list->item(i)->text().contains(text, Qt::CaseInsensitive)) {
            return list->item(i);
        }
    }
    return nullptr;
}

QJsonArray listTexts(QListWidget* list)
{
    QJsonArray out;
    for (int i = 0; list != nullptr && i < list->count(); ++i) {
        out.append(list->item(i)->text());
    }
    return out;
}

// What the entry property editor shows: its labels, the Source combo and
// whether the advanced group is enabled.
void noteEditor(SceneContext& ctx, ContainerSettingsDialog* dialog)
{
    QStringList labels;
    for (QLabel* l : dialog->findChildren<QLabel*>()) {
        if (l->isVisible() && !l->text().isEmpty()) {
            labels.append(l->text());
        }
    }
    ctx.notes.append(QStringLiteral("dialog labels: %1").arg(labels.join(QStringLiteral(" | "))));
    if (auto* source = dialog->findChild<QComboBox*>(QStringLiteral("contentSlice"))) {
        QStringList items;
        for (int i = 0; i < source->count(); ++i) {
            items.append(source->itemText(i));
        }
        ctx.notes.append(QStringLiteral("Source combo (contentSlice): %1, effectively %2, items [%3], current \"%4\"")
                             .arg(source->isVisible() ? QStringLiteral("shown") : QStringLiteral("not shown"))
                             .arg(source->isEnabled() ? QStringLiteral("enabled") : QStringLiteral("disabled"))
                             .arg(items.join(QStringLiteral(", ")), source->currentText()));
    } else {
        ctx.notes.append(QStringLiteral("no Source combo (contentSlice) in the dialog"));
    }
}

// The Station container (slice A on its chip) in the left strip, the Main
// Panel right, and Containers > Edit Container open on Station over the
// window; `select` poses the dialog. With `composite` off the window
// image leaves the dialog out (it would cover what the scene shows) and
// the dialog has its own image only.
// `menu`, when the pose sets it, is a popup open over the window: it is
// composited into the window image and has its own image (`name--menu`).
void sceneEditDialog(SceneContext& ctx, const QString& name,
                     const std::function<void(SceneContext&, ContainerSettingsDialog*)>& select,
                     bool composite = true, const std::shared_ptr<QPointer<QMenu>>& menu = {})
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    const QString id = QStringLiteral("preview-station");
    auto dialog = std::make_shared<QPointer<ContainerSettingsDialog>>();
    runWindowScene(ctx, twoBandPoses(), 0, [](SceneContext& c) {
        commitScene(c, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            d.containers.append(onSlice(makeSceneContainer(m, QStringLiteral("Station"), DockEdge::Left,
                                                           ContentLayout::VerticalStack, stationEntries()), 1));
        });
    }, [id, dialog, select](SceneContext& c) {
        poseStation(c, id);
        if (!c.failure.isEmpty()) {
            return;
        }
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* station = manager->container(id);
        // The Containers > Edit Container path, shown instead of exec()'d
        // so the grab needs no nested loop.
        *dialog = new ContainerSettingsDialog(station, c.window, manager);
        (*dialog)->selectDraftContainer(id);
        (*dialog)->show();
        pump(kDialogSettleMs);
        // Its default size is larger than the window; sized down to the
        // window as a user would (never below its minimum).
        const QSize fit = (*dialog)->size().boundedTo(c.window->size() - QSize(kDialogInsetPx, kDialogInsetPx))
                              .expandedTo((*dialog)->minimumSizeHint());
        c.notes.append(QStringLiteral("Container Settings default %1x%2, shown at %3x%4 to fit the %5x%6 window")
                           .arg((*dialog)->width()).arg((*dialog)->height())
                           .arg(fit.width()).arg(fit.height())
                           .arg(c.window->width()).arg(c.window->height()));
        (*dialog)->resize(fit);
        pump(kFinalSettleMs);
        (*dialog)->move(c.window->mapToGlobal(c.window->rect().center()) - (*dialog)->rect().center());
        select(c, dialog->data());
        pump(kFinalSettleMs);
        for (const QJsonValue& v : stopTimersUnder(dialog->data())) {
            c.stoppedTimers.append(v);
        }
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    noteStrips(ctx);
    captureWindow(ctx, name);
    const bool menuOpen = menu && *menu;
    if (composite) {
        compositeAndRecrop(ctx, name, {dialog->data()});
    } else {
        ctx.notes.append(QStringLiteral("the dialog is not composited into the window image; see %1--dialog")
                             .arg(name));
        if (menuOpen) {
            compositeAndRecrop(ctx, name, {menu->data()});
        }
    }
    if (menuOpen) {
        captureDialog(ctx, menu->data(), name + QStringLiteral("--menu"));
        (*menu)->close();
    }
    captureDialog(ctx, dialog->data(), name + QStringLiteral("--dialog"));
    annotateSlices(ctx);
    if (*dialog) {
        (*dialog)->reject();
        (*dialog)->deleteLater();
    }
}

void sceneEditAddContent(SceneContext& ctx)
{
    sceneEditDialog(ctx, QStringLiteral("edit-add-content"), [](SceneContext& c, ContainerSettingsDialog* d) {
        auto* available = d->findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        QListWidgetItem* longpath = rowContaining(available, QStringLiteral("(Longpath)"));
        if (longpath == nullptr) {
            c.failure = QStringLiteral("no Longpath entry in the Available list");
            return;
        }
        available->setCurrentItem(longpath);
        available->scrollToItem(longpath, QAbstractItemView::PositionAtCenter);
        c.notes.append(QStringLiteral("Available list (containerAvailableContents), first Longpath row "
                                      "selected and centred: %1")
                           .arg(QString::fromUtf8(QJsonDocument(listTexts(available)).toJson(QJsonDocument::Compact))));
    });
}

void sceneEditSourceSelect(SceneContext& ctx)
{
    // The Station header's source menu opened programmatically, as a chip
    // click opens it, over the window as ContainerWidget shows it. The dialog has its own image.
    auto menu = std::make_shared<QPointer<QMenu>>();
    sceneEditDialog(ctx, QStringLiteral("edit-source-select"), [menu](SceneContext& c, ContainerSettingsDialog* d) {
        auto* contents = d->findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
        QListWidgetItem* frequency = rowContaining(contents, QStringLiteral("Frequency"));
        if (frequency == nullptr) {
            c.failure = QStringLiteral("no Frequency row in the Station contents");
            return;
        }
        contents->setCurrentItem(frequency);
        pump(kFinalSettleMs);
        noteEditor(c, d);
        auto* manager = c.window->findChild<ContainerManager*>();
        ContainerWidget* station = manager->container(QStringLiteral("preview-station"));
        WorkspaceGroupHeader* header = station != nullptr ? station->workspaceHeader() : nullptr;
        WorkspaceSourceChip* chip = header != nullptr ? header->sourceChip() : nullptr;
        if (chip == nullptr || !chip->isVisible()) {
            c.failure = QStringLiteral("no visible source chip on the Station header");
            return;
        }
        // The menu the chip's click opens (ContainerWidget::showSourceMenu),
        // shown with popup() so the grab needs no nested loop.
        *menu = new QMenu(station);
        station->populateSourceMenu(**menu);
        tagProvenance(menu->data(), provProposed());
        (*menu)->popup(chip->mapToGlobal(QPoint(0, chip->height())));
        pump(kFinalSettleMs);
        if (!(*menu)->isVisible()) {
            c.failure = QStringLiteral("the source menu did not show");
            return;
        }
        QStringList items;
        for (QAction* a : (*menu)->actions()) {
            if (a->isSeparator()) {
                items.append(QStringLiteral("-"));
                continue;
            }
            items.append(QStringLiteral("%1%2%3").arg(a->text(),
                a->isChecked() ? QStringLiteral(" (checked)") : QString(),
                a->isEnabled() ? QString() : QStringLiteral(" (disabled)")));
        }
        c.notes.append(QStringLiteral("source chip \"%1\": menu opened programmatically, as a chip click "
                                      "opens it, below the chip, items [%2]. "
                                      "A choice commits the container source as Container Settings' "
                                      "Slice combo does").arg(chip->text(), items.join(QStringLiteral(", "))));
    }, false, menu);
}

void sceneEditProperties(SceneContext& ctx)
{
    sceneEditDialog(ctx, QStringLiteral("edit-properties"), [](SceneContext& c, ContainerSettingsDialog* d) {
        auto* contents = d->findChild<QListWidget*>(QStringLiteral("containerDraftContents"));
        QListWidgetItem* needle = rowContaining(contents, QStringLiteral("Needle"));
        if (needle == nullptr) {
            c.failure = QStringLiteral("no Needle row in the Station contents");
            return;
        }
        contents->setCurrentItem(needle);
        pump(kFinalSettleMs);
        c.notes.append(QStringLiteral("Station contents: %1, \"%2\" selected")
                           .arg(QString::fromUtf8(QJsonDocument(listTexts(contents)).toJson(QJsonDocument::Compact)),
                                needle->text()));
        noteEditor(c, d);
        QStringList properties;
        for (QWidget* w : d->findChildren<QWidget*>()) {
            if (w->objectName().startsWith(QStringLiteral("property_")) && w->isVisible()) {
                QString value;
                if (auto* combo = qobject_cast<QComboBox*>(w)) {
                    value = combo->currentText();
                } else if (auto* check = qobject_cast<QCheckBox*>(w)) {
                    value = check->isChecked() ? QStringLiteral("on") : QStringLiteral("off");
                } else if (auto* edit = qobject_cast<QLineEdit*>(w)) {
                    value = edit->text();
                }
                properties.append(QStringLiteral("%1 \"%2\"").arg(w->objectName(), value));
            }
        }
        c.notes.append(QStringLiteral("declared property controls: %1")
                           .arg(properties.isEmpty() ? QStringLiteral("none shown") : properties.join(QStringLiteral(", "))));
    });
}

// four-rx-look: four compact frequencies left, four Longpath S-meter bars
// bottom, the Main Panel right.
void sceneFourRxLook(SceneContext& ctx)
{
    ctx.provenance = provProposed();
    ctx.proposedValues = proposedEdgeValues();
    ctx.extraCrops = addAllStripCrops;
    runWindowScene(ctx, fourRxPoses(), 2, [](SceneContext& c) {
        commitScene(c, HeaderMode::Always, [](WorkspaceDocument& d, ContainerManager* m) {
            placeMain(d, DockEdge::Right);
            const QString freq = QString::fromLatin1(LongpathContent::kFrequencyType);
            const QString bar = QString::fromLatin1(LongpathContent::kBarType);
            for (int rx = 1; rx <= 4; ++rx) {
                const QString letter(QChar('A' + rx - 1));
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("Frequency %1").arg(letter), DockEdge::Left, ContentLayout::VerticalStack,
                    {{freq, {}, {}, rxSource(rx)}}), rx));
            }
            for (int rx = 1; rx <= 4; ++rx) {
                const QString letter(QChar('A' + rx - 1));
                d.containers.append(onSlice(makeSceneContainer(
                    m, QStringLiteral("S-Meter %1").arg(letter), DockEdge::Bottom, ContentLayout::VerticalStack,
                    {{bar, {}, {}, rxSource(rx), bindingConfig(MeterBinding::SignalPeak)}}), rx));
            }
        });
    }, [](SceneContext& c) {
        auto* area = c.window->findChild<EdgeDockArea*>();
        feedSliceInstruments(c, area != nullptr ? area->strip(DockEdge::Bottom) : nullptr);
        pump(kFinalSettleMs);
    });
    if (!ctx.failure.isEmpty()) {
        return;
    }
    ctx.notes.append(QStringLiteral(
        "S-Meter A to D (Longpath bars, RX: Signal Peak): each fed its slice's posed level through "
        "MeterPoller::readingUpdated, as the poller sends with a radio (see fed_readings)"));
    noteStrips(ctx);
    captureWindow(ctx, QStringLiteral("four-rx-look"));
    annotateSlices(ctx);
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
        runWindowScene(ctx, twoBandPoses(), 0, poseTxBeforeFreeze, poseTxAfterFeed);
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
        attachRegions(img, floating);
        ctx.images.append(img);
    }});

    // ---- proposed workspace headers (workspace preview switch on) ----
    scenes.append({QStringLiteral("header-always"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Always, QStringLiteral("header-always"));
    }, true});
    scenes.append({QStringLiteral("header-reveal-idle"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Reveal, QStringLiteral("header-reveal-idle"));
    }, true});
    scenes.append({QStringLiteral("header-reveal-shown"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Reveal, QStringLiteral("header-reveal-shown"),
                            [](SceneContext& c, ContainerWidget* panel) {
            if (panel != nullptr) {
                // The pointer has been in the top band for 120 ms.
                panel->setHeaderRevealedForPreview(true);
                c.notes.append(QStringLiteral("ContainerWidget::setHeaderRevealedForPreview(true)"));
            }
        });
    }, true});
    scenes.append({QStringLiteral("header-hidden"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Hidden, QStringLiteral("header-hidden"));
    }, true});
    scenes.append({QStringLiteral("arrange-on"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedHeader(ctx, HeaderMode::Always, QStringLiteral("arrange-on"),
                            [](SceneContext& c, ContainerWidget*) {
            if (auto* manager = c.window->findChild<ContainerManager*>()) {
                manager->setArrangeMode(true);
                c.notes.append(QStringLiteral("ContainerManager::setArrangeMode(true)"));
            }
        });
    }, true});

    // ---- proposed four-edge workspace (workspace preview switch on) ----
    scenes.append({QStringLiteral("edge-right"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-right"), {});
    }, true});
    scenes.append({QStringLiteral("edge-left"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-left"), [](SceneContext& c) {
            // Options > Move to Edge > Left.
            emit mainPanel(c)->moveToEdgeRequested(DockEdge::Left);
        });
    }, true});
    scenes.append({QStringLiteral("edge-top-bottom"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-top-bottom"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
        });
    }, true});
    scenes.append({QStringLiteral("edge-all-four"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("edge-all-four"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
            addLevelsContainer(c, DockEdge::Left);
        });
    }, true});
    scenes.append({QStringLiteral("edge-all-four-clean"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // edge-all-four with every container's header Reveal, idle: no
        // group or entry header shows and none takes room.
        sceneProposedEdges(ctx, QStringLiteral("edge-all-four-clean"), [](SceneContext& c) {
            addMetersContainer(c, DockEdge::Top);
            addBandContainer(c, DockEdge::Bottom);
            addLevelsContainer(c, DockEdge::Left);
            c.notes.append(QStringLiteral("every container: Header Reveal, pointer away"));
        }, {}, HeaderMode::Reveal);
    }, true});
    // ---- Longpath instruments as container content (switch on) ----
    scenes.append({QStringLiteral("instruments-sheet"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneInstrumentsSheet(ctx); }, true});
    scenes.append({QStringLiteral("mixed-container"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneMixedContainer(ctx, QStringLiteral("mixed-container"), HeaderMode::Always);
    }, true});
    scenes.append({QStringLiteral("mixed-container-reveal"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // mixed-container with every header Reveal, pointer away.
        sceneMixedContainer(ctx, QStringLiteral("mixed-container-reveal"), HeaderMode::Reveal);
    }, true});
    // ---- workspace profiles (switch on) ----
    scenes.append({QStringLiteral("profile-ssb"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-ssb"), SceneProfile::Ssb); },
                   true});
    scenes.append({QStringLiteral("profile-dx"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-dx"), SceneProfile::Dx); },
                   true});
    scenes.append({QStringLiteral("profile-contest"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProfile(ctx, QStringLiteral("profile-contest"), SceneProfile::Contest);
    }, true});
    scenes.append({QStringLiteral("profile-ft8"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneProfile(ctx, QStringLiteral("profile-ft8"), SceneProfile::Ft8); },
                   true});
    scenes.append({QStringLiteral("profile-rail-rest"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // SSB with the rail at rest: the active badge only.
        sceneProfile(ctx, QStringLiteral("profile-rail-rest"), SceneProfile::Ssb, {}, {}, true);
    }, true});
    scenes.append({QStringLiteral("profile-rail-open"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // The fan-out through the dock's open() seam, the pointer over the
        // D badge (hover state, its remove cross) and its tooltip.
        auto tip = std::make_shared<QPointer<QWidget>>();
        sceneProfile(ctx, QStringLiteral("profile-rail-open"), SceneProfile::Ssb, [tip](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            WorkspaceProfiles* profiles = c.window->workspaceProfiles();
            dock->open();
            hideStaleRailBadges(dock->rail());
            pump(kFinalSettleMs);
            QPushButton* d = profiles->ids().size() > 1 ? dock->rail()->badgeButton(profiles->ids().at(1)) : nullptr;
            if (d == nullptr) {
                c.failure = QStringLiteral("no D badge in the rail");
                return;
            }
            const QPointF local(d->width() / 2.0, d->height() / 2.0);
            d->setAttribute(Qt::WA_UnderMouse, true);
            QEnterEvent enter(local, d->mapTo(d->window(), local), d->mapToGlobal(local));
            QCoreApplication::sendEvent(d, &enter);
            d->update();
            // The pointer's spot; Qt offsets the tooltip from it as it does live.
            QToolTip::showText(d->mapToGlobal(local.toPoint()), d->toolTip(), d);
            pump(kFinalSettleMs);
            *tip = visibleToolTip();
            c.notes.append(*tip ? QStringLiteral("pointer over the D badge: hover state set, Enter sent, "
                                                 "tooltip \"%1\" composited where it shows").arg(d->toolTip())
                                : QStringLiteral("pointer over the D badge: hover state set; the tooltip did "
                                                 "not show (background-only process), not composited"));
        }, [tip](SceneContext& c) {
            if (*tip) {
                compositeAndRecrop(c, QStringLiteral("profile-rail-open"), {tip->data()});
            }
        }, true);
    }, true});
    scenes.append({QStringLiteral("profile-new-menu"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // The + in the fanned-out rail: the New menu, as MainWindow shows it.
        auto menu = std::make_shared<QPointer<QMenu>>();
        sceneProfile(ctx, QStringLiteral("profile-new-menu"), SceneProfile::Ssb, [menu](SceneContext& c) {
            ProfileRailDock* dock = c.window->profileRailDock();
            dock->open();
            hideStaleRailBadges(dock->rail());
            pump(kFinalSettleMs);
            QWidget* plus = dock->rail()->plusButton();
            *menu = WorkspaceProfilesDialog::createNewMenu(c.window->workspaceProfiles(), c.window);
            tagProvenance(menu->data(), provProposed());
            (*menu)->popup(plus->mapToGlobal(QPoint(0, plus->height())));
            pump(kFinalSettleMs);
            if (!(*menu)->isVisible()) {
                c.failure = QStringLiteral("the New menu did not show");
            }
        }, [menu](SceneContext& c) {
            if (!*menu) {
                return;
            }
            compositeAndRecrop(c, QStringLiteral("profile-new-menu"), {menu->data()});
            captureDialog(c, menu->data(), QStringLiteral("profile-new-menu--menu"));
            (*menu)->close();
        });
    }, true});
    scenes.append({QStringLiteral("profile-manage-dialog"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        // Containers > Manage Workspace Profiles over the SSB window, and the
        // dialog alone.
        sceneProfile(ctx, QStringLiteral("profile-manage-dialog"), SceneProfile::Ssb, [](SceneContext& c) {
            auto* action = c.window->findChild<QAction*>(QStringLiteral("containersManageProfiles"));
            if (action == nullptr) {
                c.failure = QStringLiteral("no containersManageProfiles action");
                return;
            }
            action->trigger();
            pump(kDialogSettleMs);
            auto* dialog = c.window->findChild<WorkspaceProfilesDialog*>();
            if (dialog == nullptr) {
                c.failure = QStringLiteral("the manage dialog did not open");
                return;
            }
            dialog->move(c.window->mapToGlobal(c.window->rect().center()) - dialog->rect().center());
            pump(kFinalSettleMs);
        }, [](SceneContext& c) {
            auto* dialog = c.window->findChild<WorkspaceProfilesDialog*>();
            compositeAndRecrop(c, QStringLiteral("profile-manage-dialog"), {dialog});
            captureDialog(c, dialog, QStringLiteral("profile-manage-dialog--dialog"));
            if (dialog != nullptr) {
                dialog->close();
            }
        });
    }, true});
    scenes.append({QStringLiteral("free-container"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("free-container"), [](SceneContext& c) {
            const QString id = addLevelsContainer(c, DockEdge::Left);
            if (id.isEmpty()) {
                return;
            }
            // Options > Place Free.
            auto* manager = c.window->findChild<ContainerManager*>();
            emit manager->container(id)->placeFreeRequested();
            // Moved by its header to the lower right of the 20 m waterfall,
            // clear of every VFO flag and of the dB and time scales.
            constexpr int kScaleClearancePx = 72;
            constexpr int kBottomClearancePx = 24;
            ContainerWidget* levels = manager->container(id);
            QWidget* pane = levels != nullptr ? levels->parentWidget() : nullptr;
            if (pane == nullptr) {
                c.failure = QStringLiteral("the Levels container was not placed free");
                return;
            }
            WorkspaceDocument draft = manager->workspaceStore()->snapshot();
            for (auto& d : draft.containers) {
                if (d.id == id) {
                    // The committed free size; the widget may not have taken it yet.
                    const QSize size = d.geometry.isValid() ? d.geometry.size() : levels->size();
                    d.geometry = QRect(QPoint(pane->width() - size.width() - kScaleClearancePx,
                                              pane->height() - size.height() - kBottomClearancePx),
                                       size);
                    c.notes.append(QStringLiteral("Levels moved to %1,%2 %3x%4 in a %5x%6 pane")
                                       .arg(d.geometry.x()).arg(d.geometry.y())
                                       .arg(size.width()).arg(size.height())
                                       .arg(pane->width()).arg(pane->height()));
                }
            }
            if (manager->commitWorkspace(draft, draft.revision).status != CommitStatus::Saved) {
                c.failure = QStringLiteral("could not move the Levels container");
            }
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            ContainerWidget* levels = manager->container(QStringLiteral("preview-levels"));
            if (levels == nullptr) {
                c.failure = QStringLiteral("the Levels container was not created");
                return;
            }
            emit levels->headerModeRequested(HeaderMode::Reveal);
            pump(kSettleMs);
            // The pointer has been in the top band for 120 ms.
            levels->setHeaderRevealedForPreview(true);
            c.notes.append(QStringLiteral("Levels: Place Free, Header Reveal, "
                                          "ContainerWidget::setHeaderRevealedForPreview(true)"));
        });
    }, true});
    scenes.append({QStringLiteral("floating-container"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("floating-container"), [](SceneContext& c) {
            const QString id = addLevelsContainer(c, DockEdge::Left);
            if (id.isEmpty()) {
                return;
            }
            c.window->findChild<ContainerManager*>()->floatContainer(id);
        });
        if (!ctx.failure.isEmpty()) {
            return;
        }
        auto* floating = visibleTopLevel<FloatingContainer>();
        if (floating == nullptr) {
            ctx.failure = QStringLiteral("the Levels container did not float");
            return;
        }
        if (ctx.beforeGrab) {
            ctx.beforeGrab(floating);
            pump(kFinalSettleMs);
        }
        CapturedImage img;
        img.name = QStringLiteral("floating-container--floating");
        img.kind = QStringLiteral("floating");
        img.image = grabWidget(floating);
        img.logicalSize = floating->size();
        img.dpr = img.image.devicePixelRatio();
        img.extra[QStringLiteral("window_title")] = floating->windowTitle();
        img.extra[QStringLiteral("frameless")] =
            floating->windowFlags().testFlag(Qt::FramelessWindowHint);
        attachRegions(img, floating);
        ctx.images.append(img);
    }, true});
    scenes.append({QStringLiteral("drag-preview-left"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("drag-preview-left"), {}, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            manager->setArrangeMode(true);
            manager->showDropPreviewForTest(DockEdge::Left, 0);
            c.notes.append(QStringLiteral(
                "Arrange on; ContainerManager::showDropPreviewForTest(Left, 0)"));
        });
    }, true});
    scenes.append({QStringLiteral("drag-preview-between"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) {
        sceneProposedEdges(ctx, QStringLiteral("drag-preview-between"), [](SceneContext& c) {
            addLevelsContainer(c, DockEdge::Right);
        }, [](SceneContext& c) {
            auto* manager = c.window->findChild<ContainerManager*>();
            manager->setArrangeMode(true);
            manager->showDropPreviewForTest(DockEdge::Right, 1);
            c.notes.append(QStringLiteral(
                "Arrange on; ContainerManager::showDropPreviewForTest(Right, 1)"));
        });
    }, true});

    // ---- coverage scenes (switch on) ----
    scenes.append({QStringLiteral("clean-spectrum"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) { sceneCleanSpectrum(ctx, false); }, true});
    scenes.append({QStringLiteral("clean-spectrum-reveal"), QStringLiteral("1"), false,
                   [](SceneContext& ctx) { sceneCleanSpectrum(ctx, true); }, true});
    scenes.append({QStringLiteral("cw-digital"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneCwDigital(ctx); }, true});
    scenes.append({QStringLiteral("tx-processing"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneTxProcessing(ctx); }, true});
    scenes.append({QStringLiteral("edit-add-content"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditAddContent(ctx); }, true});
    scenes.append({QStringLiteral("edit-source-select"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditSourceSelect(ctx); }, true});
    scenes.append({QStringLiteral("edit-properties"), QStringLiteral("2v"), false,
                   [](SceneContext& ctx) { sceneEditProperties(ctx); }, true});
    scenes.append({QStringLiteral("four-rx-look"), QStringLiteral("2x2"), false,
                   [](SceneContext& ctx) { sceneFourRxLook(ctx); }, true});

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

// Setup > Diagnostics > Radio Status shows the time since the bench
// connection came up, which moved by a second between runs. Its clock is
// a QTimer member of the page (not a child, so stopTimersUnder() cannot
// reach it): cut every timer that ticks into the page as it fires, cut the
// model's connection-state tick, then show a fixed time.
constexpr auto kUptimeReadoutId = "diagnostics.radioStatus.uptime";
constexpr auto kFrozenUptime = "14m 32s";
// Longer than the page's 1000 ms uptime tick.
constexpr int kUptimeCutWaitMs = 1300;

void freezeRadioStatusUptime(SceneContext& ctx, QWidget* dialog)
{
    QLabel* uptime = nullptr;
    for (QLabel* label : dialog->findChildren<QLabel*>()) {
        if (label->property("nereusSetupId").toString() == QLatin1String(kUptimeReadoutId)) {
            uptime = label;
        }
    }
    if (uptime == nullptr) {
        return;
    }
    QWidget* page = uptime->parentWidget();
    while (page != nullptr && !page->inherits("NereusSDR::RadioStatusPage")) {
        page = page->parentWidget();
    }
    if (page == nullptr) {
        return;
    }
    class TimerCut : public QObject
    {
    public:
        explicit TimerCut(QObject* target) : m_target(target) {}
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Timer) {
                if (auto* timer = qobject_cast<QTimer*>(watched);
                    timer != nullptr && QObject::disconnect(timer, &QTimer::timeout, m_target, nullptr)) {
                    timer->stop();
                }
            }
            return false;
        }

    private:
        QObject* m_target;
    } cut(page);
    qApp->installEventFilter(&cut);
    pump(kUptimeCutWaitMs);
    qApp->removeEventFilter(&cut);
    QObject::disconnect(ctx.model, &RadioModel::connectionStateChanged, page, nullptr);
    uptime->setText(QString::fromLatin1(kFrozenUptime));
    ctx.notes.append(QStringLiteral("radio-status uptime held at %1").arg(QString::fromLatin1(kFrozenUptime)));
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
            // setCurrentItem() scrolls the tree only as far as it must, from
            // wherever a pending layout left it, so the offset varied by run.
            // Settle, then centre the item: the same tree scroll every run.
            pump(kFinalSettleMs);
            tree->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            // Qt sends the tree a synthetic enter from the real pointer,
            // wherever it is on screen, when the dialog shows, and one row
            // stayed hovered in some runs: no hover in the page tree.
            tree->viewport()->setAttribute(Qt::WA_Hover, false);
            QEvent leave(QEvent::Leave);
            QCoreApplication::sendEvent(tree->viewport(), &leave);
            freezeRadioStatusUptime(ctx, d);
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
