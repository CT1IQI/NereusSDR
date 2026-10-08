// no-port-check: NereusSDR-original visual preview tool; fixed illustrative data.
// =================================================================
// tests/visual/PreviewData.h  (NereusSDR)
// =================================================================
//
// Fixed illustrative data for the workspace visual preview tool: a seeded
// spectrum and waterfall generator, and the freeze step that holds every
// live readout still so two captures of one scene match pixel for pixel.
// Nothing here reads the wall clock, the network or an unseeded RNG.
//
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QJsonArray>
#include <QString>
#include <QVector>
#include <QtGlobal>

class QWidget;

namespace NereusSDR {

class MainWindow;
class SpectrumWidget;

namespace Preview {

// Shape of one illustrative signal on a pan.
enum class SignalShape {
    Usb,  // 2.4 kHz voice passband above the dial frequency
    Lsb,  // 2.4 kHz voice passband below the dial frequency
    Cw,   // 60 Hz carrier
    Ft8,  // one 50 Hz FT8 tone
};

struct SignalSpec {
    double hz{0.0};    // dial (SSB) or tone/carrier (CW, FT8) frequency
    double dbm{0.0};   // peak level the trace should read
    SignalShape shape{SignalShape::Usb};
};

struct PanSpectrum {
    QString panId;
    double noiseDbm{-126.0};
    QVector<SignalSpec> signalList;
    quint32 seed{1};
};

// Result of feeding one pan: what the trace reads at the strongest signal.
struct SpectrumCheck {
    QString panId;
    double targetHz{0.0};
    double targetDbm{0.0};
    double displayedDbm{0.0};
    double centreHz{0.0};
    double spanHz{0.0};
    double ddcCentreHz{0.0};
    double sampleRateHz{0.0};
    int waterfallRows{0};
};

// Deterministic xorshift32, so the data is identical on every platform.
class SeededRng {
public:
    explicit SeededRng(quint32 seed) : m_state(seed == 0 ? 0x9E3779B9u : seed) {}
    quint32 next();
    // Uniform in [lo, hi).
    double uniform(double lo, double hi);

private:
    quint32 m_state;
};

// Bins in the linear power form SpectrumWidget::updateSpectrumLinear
// expects (full FFT, negative frequencies first), for one frame. row < 0 is
// the steady trace frame (every signal at its stated level); row >= 0 is a
// waterfall row with seeded per-row modulation (voice syllables, CW keying,
// FT8 cycles) and its own noise jitter.
QVector<float> binsForFrame(const SpectrumWidget& sw, const PanSpectrum& spec, int row);

// Feeds one pan: pauses its waterfall ticker, pushes enough rows to fill the
// waterfall, then the steady trace frame enough times for averaging to
// settle. Returns the check value.
SpectrumCheck feedPan(SpectrumWidget* sw, const PanSpectrum& spec);

// Feeds a keyed pan the way LocalTxDisplaySource does on a local window:
// the transmit window set to `spanHz` around the carrier, then transmit
// trace pixels (updateSpectrumFromTxPixels) and transmit waterfall rows
// (pushTxWaterfallRow) of an LSB voice signal (upper sideband when `usb`),
// filter `lowHz`..`highHz` from the carrier, peak `peakDbm` over a floor of
// `floorDbm`. Display calls only; nothing reaches a connection.
void feedTxPan(SpectrumWidget* sw, double carrierHz, bool usb, int lowHz, int highHz,
               double peakDbm, double floorDbm, quint32 seed);

// The dBm offset and window ENB passed with every frame (the values the
// existing spectrum tests use).
inline constexpr double kDbmOffset = -60.0;
inline constexpr double kWindowEnb = 1.5;
inline constexpr int kBinCount = 4096;          // bins per kReferenceRateHz
inline constexpr double kReferenceRateHz = 192000.0;
inline constexpr int kTraceFrames = 240;

// Fixed text for the frozen readouts.
inline constexpr const char* kFrozenUtc = "14:32:07 UTC";
inline constexpr double kFrozenCpuPercent = 7.0;
inline constexpr double kFrozenRxMbps = 18.4;
inline constexpr double kFrozenTxMbps = 0.6;
inline constexpr int kFrozenRttMs = 2;

// Stops every active QTimer under `root` and returns a description of each
// one stopped (class of its parent, object name, interval).
QJsonArray stopTimersUnder(QObject* root);

// Freezes the main window's live readouts: UTC clock, connection segment
// (pulse, rates, RTT, audio pip), CPU tile. Returns what it froze.
QJsonArray freezeReadouts(MainWindow* window);

} // namespace Preview
} // namespace NereusSDR
