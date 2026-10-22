// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - the MaxBin hold falls again after a very strong signal.
//
// Ported from Longpath tests/tst_maxbin_hold_decay.cpp [@383f058]
// (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath, PR 215,
// commit 695a3511; no top-of-file GPL header upstream, the project-level
// LICENSE applies). The five step cases and their comments are
// Longpath's, kept verbatim with German comments; failure messages are in
// English and the qInfo progress lines are left out so the run is quiet.
// NereusSDR adds aDetectorBinAboveZeroDbmReleases, which drives a real
// WdspEngine detector (setupMaxBinDetector, onSpectrumBinsForMaxBin,
// getMaxBinDbm) and fails on the code before the fix.
//
// Modification history (NereusSDR):
//   2026-10-22: Ported from Longpath by J.J. Boyd (KG4VCF), with
//               AI-assisted transformation via Claude Code.
//
// --- From tst_maxbin_hold_decay.cpp (Longpath) ---
//
// Der MaxBin-Haltewert blieb nach einem sehr starken Signal oben kleben.
//
// Thetis rechnet (wdsp/analyzer.c:815-818 [@501e3f5], bei uns bis zum
// 2026-10-09 Zeile fuer Zeile uebernommen):
//
//     held -= fabs((1 - decay) * held);
//
// Der Abzug haengt am BETRAG des Haltewerts. Oberhalb 0 dBm schrumpft
// er mit dem Wert selbst, der Haltewert naehert sich 0 dBm asymptotisch
// und unterschreitet sie nie -- die Anzeige klebt (Thetis #2681). Die
// Vorlage hat denselben Fehler im Oktober 2026 behoben; hier steht eine
// eigene Fassung: Abfall gegen einen Boden bei -160 dBm statt gegen 0.
//
// Dieser Pruefstand faellt gegen die alte Fassung -- das ist sein Zweck.
//
// [NereusSDR note: the "-160 dBm floor" line above is stale upstream. The
//  code Longpath shipped, and this port, use a 20 dB minimum reference;
//  the -160 dBm floor was the rejected first attempt that
//  itStillRidesAStrongerLiveReadingAtOnce guards against.]

#include <QtTest>

#include <cmath>

#include "core/WdspEngine.h"

using namespace NereusSDR;

namespace {

// From Longpath tests/tst_maxbin_hold_decay.cpp:26-37 [@383f058]
// Die Rechnung, wie sie bis zum 2026-10-09 im Baum stand. Nur hier, als
// Gegenprobe: so sieht man, was der Fehler wirklich tat.
double altThetisHold(double heldDbm, double liveDbm, double decay)
{
    double next = heldDbm - std::abs((1.0 - decay) * heldDbm);
    return liveDbm > next ? liveDbm : next;
}

// tau = 0.5 s bei 20 Bildern/s -- die Werte, mit denen MainWindow den
// Detektor fuer das S-Meter einrichtet.
constexpr double kDecay = 0.90483741803595952;  // exp(-1/(0.5*20))

// Thetis's own example detector (wdsp/analyzer.c:1442 [@501e3f5]):
// SetupDetectMaxBin(1, 0, 0, 0, 192000.0, -3000.0, -300.0, 0.5, 60),
// at the 20 frames per second the comment below names.
constexpr double kRate     = 192000.0;
constexpr double kLowHz    = -3000.0;
constexpr double kHighHz   = -300.0;
constexpr double kTau      = 0.5;
constexpr int    kFps      = 20;
constexpr int    kBins     = 1024;

QVector<float> flatFrame(float dbm)
{
    return QVector<float>(kBins, dbm);
}

} // namespace

class TstMaxBinHoldDecay : public QObject
{
    Q_OBJECT

private slots:
    void theOldFormulaStickedAboveZeroDbm();
    void theHoldNowFallsFromAStrongSignal();
    void itStillRidesAStrongerLiveReadingAtOnce();
    void theWorkingRangeDecaysAsBefore();
    void itNeverFallsBelowTheFloorAndKeepsTheSentinel();
    void aDetectorBinAboveZeroDbmReleases();
};

// From Longpath tests/tst_maxbin_hold_decay.cpp:52-119 [@383f058]

// Erst der Beweis, dass es den Fehler wirklich gab: mit der alten
// Rechnung steht der Haltewert nach einer vollen Minute (1200 Bilder)
// immer noch ueber 0 dBm.
void TstMaxBinHoldDecay::theOldFormulaStickedAboveZeroDbm()
{
    double held = 10.0;
    for (int frame = 0; frame < 1200; ++frame) {
        held = altThetisHold(held, -400.0, kDecay);
    }
    QVERIFY2(held > 0.0, "The old formula no longer shows the fault");
}

void TstMaxBinHoldDecay::theHoldNowFallsFromAStrongSignal()
{
    double held = 10.0;
    int frames = 0;
    // Zwanzig Sekunden bei 20 Bildern/s muessen weit mehr als genug sein.
    for (; frames < 400; ++frames) {
        held = WdspEngine::maxBinHoldNextDbm(held, -400.0, kDecay);
        if (held < -100.0) { break; }
    }
    QVERIFY2(held < -100.0, "The hold still sticks near the top");
    // Und zwar zuegig: in unter zwei Sekunden (40 Bilder bei 20/s).
    // Gemessen sind es 33 -- der Abzug betraegt im oberen Bereich den
    // Mindestbezug von 20 dB mal (1 - decay) und waechst erst, wenn der
    // Betrag des Haltewerts darueber hinausgeht.
    QVERIFY2(frames < 40, qPrintable(QStringLiteral("took %1 frames").arg(frames)));
}

void TstMaxBinHoldDecay::itStillRidesAStrongerLiveReadingAtOnce()
{
    // Peak-Attack unveraendert: ein staerkerer Messwert gilt sofort.
    QCOMPARE(WdspEngine::maxBinHoldNextDbm(-120.0, -42.0, kDecay), -42.0);
    // Ein schwaecherer zieht den Haltewert nicht schneller herunter als
    // bisher -- hier faellt die Fassung mit dem Boden bei -160 dBm
    // durch, die bei -42 dBm dreimal so schnell abfiel.
    const double neu = WdspEngine::maxBinHoldNextDbm(-42.0, -120.0, kDecay);
    const double alt = altThetisHold(-42.0, -120.0, kDecay);
    QVERIFY2(std::abs(alt - neu) < 0.01, "The fall in the strong range has changed");
}

void TstMaxBinHoldDecay::theWorkingRangeDecaysAsBefore()
{
    // Bei -80 dBm -- dem Bereich, in dem dieses Instrument wirklich
    // arbeitet -- muss die neue Rechnung dieselbe sein wie die alte,
    // sonst waere es keine Fehlerbehebung, sondern eine Umstellung.
    const double alt  = altThetisHold(-80.0, -400.0, kDecay);
    const double neu  = WdspEngine::maxBinHoldNextDbm(-80.0, -400.0, kDecay);
    QVERIFY2(std::abs(alt - neu) < 0.01,
             qPrintable(QStringLiteral("working range differs: %1 instead of %2").arg(neu).arg(alt)));
}

void TstMaxBinHoldDecay::itNeverFallsBelowTheFloorAndKeepsTheSentinel()
{
    // Ohne Signal laeuft der Haltewert nach unten, aber nicht ueber den
    // Sentinel aus Init_DetectMaxBin hinaus.
    double held = -150.0;
    for (int frame = 0; frame < 2000; ++frame) {
        held = WdspEngine::maxBinHoldNextDbm(held, -400.0, kDecay);
    }
    QCOMPARE(held, -400.0);
    QCOMPARE(WdspEngine::maxBinHoldNextDbm(-400.0, -400.0, kDecay), -400.0);
}

// NereusSDR: the same release through the detector the S-meter's
// "Max Bin" source reads. One frame with every bin at +10 dBm, then a
// quiet band at -130 dBm: the held value must fall below -100 dBm within
// two seconds (40 frames at 20 per second). Before the fix it crept
// toward 0 dBm and stayed above it for good.
void TstMaxBinHoldDecay::aDetectorBinAboveZeroDbmReleases()
{
    WdspEngine engine;
    engine.setupMaxBinDetector(0, 0, 0, kRate, kLowHz, kHighHz, kTau, kFps);

    engine.onSpectrumBinsForMaxBin(0, flatFrame(10.0f));
    QCOMPARE(engine.getMaxBinDbm(0), 10.0);

    int frames = 0;
    for (; frames < 1200; ++frames) {
        engine.onSpectrumBinsForMaxBin(0, flatFrame(-130.0f));
        if (engine.getMaxBinDbm(0) < -100.0) { break; }
    }
    const double held = engine.getMaxBinDbm(0);
    QVERIFY2(held < -100.0,
             qPrintable(QStringLiteral("the hold is still at %1 dBm after %2 frames")
                            .arg(held).arg(frames)));
    QVERIFY2(frames < 40, qPrintable(QStringLiteral("took %1 frames").arg(frames)));

    // With the band quiet, the hold rests on the live reading.
    for (int i = 0; i < 200; ++i) {
        engine.onSpectrumBinsForMaxBin(0, flatFrame(-130.0f));
    }
    QCOMPARE(engine.getMaxBinDbm(0), -130.0);
}

QTEST_MAIN(TstMaxBinHoldDecay)
#include "tst_maxbin_hold_decay.moc"
