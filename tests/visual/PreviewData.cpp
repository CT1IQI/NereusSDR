// no-port-check: NereusSDR-original visual preview tool; fixed illustrative data.
// =================================================================
// tests/visual/PreviewData.cpp  (NereusSDR)
// =================================================================
//
// See PreviewData.h.
//
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "PreviewData.h"

#include <QJsonObject>
#include <QLabel>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include "core/AudioEngine.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/widgets/SystemTile.h"

namespace NereusSDR::Preview {

namespace {

// Voice-like SSB contour in dB relative to the peak, over audio frequency:
// a rise from 300 Hz, flat 500 to 1000 Hz, then a slope to -12 dB at 2.7 kHz.
constexpr double kSsbLowHz = 300.0;
constexpr double kSsbHighHz = 2700.0;
constexpr double kCwHalfWidthHz = 30.0;
constexpr double kFt8HalfWidthHz = 25.0;
constexpr double kNoiseJitterDb = 1.5;
constexpr double kSsbRippleDb = 2.0;
// Waterfall time base: one row per 33 ms tick, so 15 s is 450 rows.
constexpr int kRowsPerFt8Cycle = 450;
constexpr int kFt8TxRows = 380;      // 12.6 s of a 15 s cycle
constexpr int kRowsPerSyllable = 9;  // ~300 ms
constexpr int kRowsPerCwElement = 4; // ~130 ms

double voiceContourDb(double audioHz)
{
    if (audioHz < 500.0) {
        return -6.0 + 6.0 * (audioHz - kSsbLowHz) / (500.0 - kSsbLowHz);
    }
    if (audioHz <= 1000.0) {
        return 0.0;
    }
    return -12.0 * (audioHz - 1000.0) / (kSsbHighHz - 1000.0);
}

// Mixes three values into one well-spread seed (a 32-bit finaliser), so
// neighbouring signals and rows do not draw correlated values.
quint32 mixSeed(quint32 a, quint32 b, quint32 c)
{
    quint32 h = a * 0x9E3779B1u;
    h ^= b + 0x7F4A7C15u + (h << 6) + (h >> 2);
    h ^= c + 0x165667B1u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

double dbmToMw(double dbm)
{
    return std::pow(10.0, dbm / 10.0);
}

// Per-row level offset for one signal, or NaN when it is silent that row.
double rowModulationDb(const SignalSpec& sig, int sigIndex, int row, quint32 seed)
{
    if (row < 0) {
        return 0.0;
    }
    switch (sig.shape) {
    case SignalShape::Usb:
    case SignalShape::Lsb: {
        SeededRng rng(mixSeed(seed, static_cast<quint32>(sigIndex + 1),
                              static_cast<quint32>(row / kRowsPerSyllable + 1)));
        const double r = rng.uniform(0.0, 1.0);
        if (r < 0.18) {
            return std::nan("");  // pause between words
        }
        return rng.uniform(-9.0, 0.0);
    }
    case SignalShape::Cw: {
        SeededRng rng(mixSeed(seed ^ 0xC3u, static_cast<quint32>(sigIndex + 1),
                              static_cast<quint32>(row / kRowsPerCwElement + 1)));
        return rng.uniform(0.0, 1.0) < 0.42 ? std::nan("") : 0.0;
    }
    case SignalShape::Ft8: {
        const int inCycle = row % kRowsPerFt8Cycle;
        if (inCycle >= kFt8TxRows) {
            return std::nan("");
        }
        // Each tone is one station: on for the whole transmit period of
        // most cycles, at a level that differs a little cycle to cycle.
        SeededRng rng(mixSeed(seed ^ 0xF8u, static_cast<quint32>(sigIndex + 1),
                              static_cast<quint32>(row / kRowsPerFt8Cycle + 1)));
        if (rng.uniform(0.0, 1.0) < 0.2) {
            return std::nan("");
        }
        return rng.uniform(-4.0, 0.0);
    }
    }
    return 0.0;
}

// The signal's level (dBm) at frequency f, or NaN when it has no energy there.
double signalDbmAt(const SignalSpec& sig, double f, double binWidthHz, double rippleDb)
{
    switch (sig.shape) {
    case SignalShape::Usb:
    case SignalShape::Lsb: {
        const double audio = sig.shape == SignalShape::Usb ? f - sig.hz : sig.hz - f;
        if (audio < kSsbLowHz || audio > kSsbHighHz) {
            return std::nan("");
        }
        return sig.dbm + voiceContourDb(audio) + rippleDb;
    }
    case SignalShape::Cw:
    case SignalShape::Ft8: {
        const double half = sig.shape == SignalShape::Cw ? kCwHalfWidthHz : kFt8HalfWidthHz;
        if (std::abs(f - sig.hz) > std::max(half, binWidthHz / 2.0)) {
            return std::nan("");
        }
        return sig.dbm;
    }
    }
    return std::nan("");
}

} // namespace

quint32 SeededRng::next()
{
    quint32 x = m_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m_state = x;
    return x;
}

double SeededRng::uniform(double lo, double hi)
{
    return lo + (hi - lo) * (static_cast<double>(next()) / 4294967296.0);
}

QVector<float> binsForFrame(const SpectrumWidget& sw, const PanSpectrum& spec, int row)
{
    const double sampleRate = sw.sampleRate();
    // A fixed bin width (kBinCount bins per 192 kHz), so a wider stream
    // gets proportionally more bins, as a fixed-resolution FFT would.
    const int binCount = std::max(kBinCount, static_cast<int>(std::lround(
        kBinCount * sampleRate / kReferenceRateHz)));
    const double binWidth = sampleRate / binCount;
    const double fftLow = sw.ddcCenterFrequency() - sampleRate / 2.0;
    // The trace frame uses the pan's own seed; each waterfall row its own.
    SeededRng noise(mixSeed(spec.seed, 0x4E01u, static_cast<quint32>(row + 2)));
    SeededRng ripple(spec.seed * 40503u + 17u);

    QVector<double> rowOffsets(spec.signalList.size());
    for (int s = 0; s < spec.signalList.size(); ++s) {
        rowOffsets[s] = rowModulationDb(spec.signalList[s], s, row, spec.seed);
    }

    QVector<float> bins(binCount);
    for (int i = 0; i < binCount; ++i) {
        const double f = fftLow + i * binWidth;
        double mw = dbmToMw(spec.noiseDbm + noise.uniform(-kNoiseJitterDb, kNoiseJitterDb));
        // The ripple is negative only, so the peak of each passband is the
        // stated level.
        const double rippleDb = ripple.uniform(-kSsbRippleDb, 0.0);
        for (int s = 0; s < spec.signalList.size(); ++s) {
            if (std::isnan(rowOffsets[s])) {
                continue;
            }
            const double dbm = signalDbmAt(spec.signalList[s], f, binWidth, rippleDb);
            if (!std::isnan(dbm)) {
                mw += dbmToMw(dbm + rowOffsets[s]);
            }
        }
        // updateSpectrumLinear folds dbmOffset into the avenger's scale:
        // displayed dBm = 10*log10(linear) + dbmOffset.
        const double dbmTotal = 10.0 * std::log10(mw);
        bins[i] = static_cast<float>(std::pow(10.0, (dbmTotal - kDbmOffset) / 10.0));
    }
    return bins;
}

SpectrumCheck feedPan(SpectrumWidget* sw, const PanSpectrum& spec)
{
    SpectrumCheck check;
    check.panId = spec.panId;
    if (sw == nullptr) {
        return check;
    }
    sw->setWaterfallTickerPausedForTest(true);

    // Enough rows to fill the waterfall at its current height.
    // The GPU path uploads only the rows written since its last frame, and
    // no more than one texture's height of them, so more rows than that
    // between two frames would leave most of the texture stale. Render a
    // frame after every eight rows (the live app pushes about one row per
    // frame, so it never meets this).
    const int height = sw->waterfallHeightForTest();
    const int rows = height + 8;
    const int chunk = 8;
    for (int r = rows - 1; r >= 0; --r) {
        sw->updateSpectrumLinear(0, binsForFrame(*sw, spec, r), kWindowEnb, kDbmOffset);
        sw->tickWaterfallForTest();
#ifdef NEREUS_GPU_SPECTRUM
        if (r % chunk == 0) {
            (void)sw->grabFramebuffer();
        }
#endif
    }
    const QVector<float> trace = binsForFrame(*sw, spec, -1);
    for (int i = 0; i < kTraceFrames; ++i) {
        sw->updateSpectrumLinear(0, trace, kWindowEnb, kDbmOffset);
    }

    check.centreHz = sw->centerFrequency();
    check.spanHz = sw->bandwidth();
    check.ddcCentreHz = sw->ddcCenterFrequency();
    check.sampleRateHz = sw->sampleRate();
    check.waterfallRows = rows;

    // The check value: the trace's maximum over the strongest signal.
    const SignalSpec* strongest = nullptr;
    for (const SignalSpec& sig : spec.signalList) {
        if (strongest == nullptr || sig.dbm > strongest->dbm) {
            strongest = &sig;
        }
    }
    const QVector<float>& px = sw->renderedPixels();
    if (strongest == nullptr || px.isEmpty() || check.spanHz <= 0.0) {
        return check;
    }
    double lo = strongest->hz - 100.0;
    double hi = strongest->hz + 100.0;
    if (strongest->shape == SignalShape::Usb) {
        lo = strongest->hz + kSsbLowHz;
        hi = strongest->hz + kSsbHighHz;
    } else if (strongest->shape == SignalShape::Lsb) {
        lo = strongest->hz - kSsbHighHz;
        hi = strongest->hz - kSsbLowHz;
    }
    const double left = check.centreHz - check.spanHz / 2.0;
    const int n = px.size();
    const int x0 = std::clamp(static_cast<int>(std::floor((lo - left) / check.spanHz * n)), 0, n - 1);
    const int x1 = std::clamp(static_cast<int>(std::ceil((hi - left) / check.spanHz * n)), 0, n - 1);
    float best = -1000.0f;
    for (int x = x0; x <= x1; ++x) {
        best = std::max(best, px[x]);
    }
    check.targetHz = strongest->hz;
    check.targetDbm = strongest->dbm;
    check.displayedDbm = best;
    return check;
}

void feedTxPan(SpectrumWidget* sw, double carrierHz, bool usb, int lowHz, int highHz,
               double peakDbm, double floorDbm, quint32 seed)
{
    if (sw == nullptr) {
        return;
    }
    sw->setWaterfallTickerPausedForTest(true);
    const double span = sw->bandwidth();
    sw->setTxCenterFrequency(carrierHz);
    sw->setTxSampleRate(span);
    // One point per pixel, as the transmit analyzer is set up.
    const int pixels = std::max(800, sw->width());
    const double left = carrierHz - span / 2.0;

    auto frame = [&](int row) {
        SeededRng noise(mixSeed(seed, 0x7A01u, static_cast<quint32>(row + 2)));
        double rowDb = 0.0;
        if (row >= 0) {
            SeededRng syl(mixSeed(seed, 0x7A02u, static_cast<quint32>(row / kRowsPerSyllable + 1)));
            rowDb = syl.uniform(0.0, 1.0) < 0.18 ? -60.0 : syl.uniform(-9.0, 0.0);
        }
        QVector<float> px(pixels);
        for (int x = 0; x < pixels; ++x) {
            const double f = left + (x + 0.5) * span / pixels;
            const double audio = usb ? f - carrierHz : carrierHz - f;
            double mw = dbmToMw(floorDbm + noise.uniform(-kNoiseJitterDb, kNoiseJitterDb));
            if (audio >= lowHz && audio <= highHz) {
                mw += dbmToMw(peakDbm + voiceContourDb(std::clamp(audio, kSsbLowHz, kSsbHighHz))
                              + rowDb);
            }
            px[x] = static_cast<float>(10.0 * std::log10(mw));
        }
        return px;
    };

    const int height = sw->waterfallHeightForTest();
    const int rows = height + 8;
    for (int r = rows - 1; r >= 0; --r) {
        sw->pushTxWaterfallRow(-1, frame(r));
        sw->tickWaterfallForTest();
#ifdef NEREUS_GPU_SPECTRUM
        if (r % 8 == 0) {
            (void)sw->grabFramebuffer();
        }
#endif
    }
    sw->updateSpectrumFromTxPixels(-1, frame(-1));
}

QJsonArray stopTimersUnder(QObject* root)
{
    QJsonArray stopped;
    if (root == nullptr) {
        return stopped;
    }
    const QList<QTimer*> timers = root->findChildren<QTimer*>();
    for (QTimer* timer : timers) {
        if (!timer->isActive()) {
            continue;
        }
        QJsonObject entry;
        entry[QStringLiteral("parent")] = timer->parent() != nullptr
            ? QString::fromLatin1(timer->parent()->metaObject()->className())
            : QString();
        entry[QStringLiteral("name")] = timer->objectName();
        entry[QStringLiteral("interval_ms")] = timer->interval();
        timer->stop();
        stopped.append(entry);
    }
    return stopped;
}

QJsonArray freezeReadouts(MainWindow* window)
{
    QJsonArray frozen;
    if (window == nullptr) {
        return frozen;
    }
    if (auto* title = window->findChild<TitleBar*>()) {
        const QString live = title->utcText();
        for (QLabel* label : title->findChildren<QLabel*>()) {
            if (!live.isEmpty() && label->text() == live) {
                label->setText(QString::fromLatin1(kFrozenUtc));
                frozen.append(QStringLiteral("utc-clock: %1").arg(QString::fromLatin1(kFrozenUtc)));
                break;
            }
        }
        if (ConnectionSegment* seg = title->connectionSegment()) {
            seg->setPulsePausedForTest(true);
            seg->setRates(kFrozenRxMbps, kFrozenTxMbps);
            for (int i = 0; i < 3; ++i) {
                seg->setRttMs(kFrozenRttMs);
            }
            seg->setAudioFlowState(AudioEngine::FlowState::Healthy);
            frozen.append(QStringLiteral("connection-segment: pulse held, rates %1/%2 Mbps, RTT %3 ms, audio healthy")
                              .arg(kFrozenRxMbps).arg(kFrozenTxMbps).arg(kFrozenRttMs));
        }
    }
    if (auto* tile = window->findChild<SystemTile*>()) {
        tile->setCpuPercent(kFrozenCpuPercent);
        frozen.append(QStringLiteral("cpu: %1%").arg(kFrozenCpuPercent));
    }
    return frozen;
}

} // namespace NereusSDR::Preview
