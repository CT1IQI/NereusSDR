// =================================================================
// tests/tst_fft_window_after_gap.cpp  (NereusSDR)
// =================================================================
//
// A lost I/Q packet must not paint a line across the spectrum.
//
// Ported from Longpath tests/tst_fft_window_after_gap.cpp [@383f058]
// (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath, PR 62,
// commit caddf95b; no top-of-file copyright or GPL header upstream, the
// project-level LICENSE applies). Longpath's three engine cases and their
// comments are kept verbatim. Longpath took the idea from AetherSDR PR 5744
// (a449dee4, https://github.com/ten9876/AetherSDR); no AetherSDR code or
// test is taken.
//
// NereusSDR adds: the acceptance case (after a gap the next frame has no
// full-width step, with a control that shows the same feed without the
// reset does have one), the overlap tail, and the wiring from the radio to
// the engine (ReceiverManager's map, Protocol 1 and Protocol 2 from fake
// radios, the gap before the samples after it).
//
// Modification history (NereusSDR):
//   2026-10-22: Ported from Longpath and extended by J.J. Boyd (KG4VCF),
//               with AI-assisted transformation via Claude Code.
//   2026-10-23: The trigger is Longpath's (forward loss only, at most once
//               per 20 ms): a repeated or reordered frame or packet sends no
//               gap, and two losses inside the throttle send one. The
//               NereusSDR-original stall bound: a lossy link keeps making
//               frames, a single gap on a healthy link still drops the
//               window. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Claude Code.
//
// --- From tst_fft_window_after_gap.cpp (Longpath) ---
//
// [upstream line 1, a marker exempting the file from the port checks as
//  Longpath-original, is left out: in NereusSDR this file is a registered
//  Longpath port]
// =================================================================
// tests/tst_fft_window_after_gap.cpp  (Longpath)
// =================================================================
//
// Der Panadapter schiebt seine Abtastwerte in ein gleitendes Fenster und
// rechnet erst, wenn es voll ist. Fehlen mittendrin Pakete — WLAN,
// ausgelastete Gegenstelle, ein HL2 an einer langsamen Leitung —, steht
// im Fenster ein Sprung. Ein Sprung ist breitbandig: er malt einen
// Schmierer ueber die ganze Bildbreite, der aussieht wie ein Signal.
//
// Longpath zaehlte solche Loecher bisher nur (iqPacketLoss, Anzeige in
// der Titelleiste). Seit dem 2026-09-22 melden beide Protokolle sie
// ausserdem als iqSequenceGap, und jeder Panadapter wirft daraufhin sein
// angefangenes Fenster weg.
//
// Hier steht, was das Fenster selbst tut: die Fahne wird auf dem Faden
// abgeholt, dem das Fenster gehoert, der Zaehler laeuft mit, und ein
// halbvolles Fenster liefert nach dem Verwerfen kein Bild mehr, bis
// wieder genug FRISCHE Werte da sind.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/FFTEngine.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/ReceiverManager.h"
#include "fakes/P1FakeRadio.h"
#include "fakes/P2FakeRadio.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;
using NereusSDR::Test::P2FakeRadio;

namespace {

// From Longpath tests/tst_fft_window_after_gap.cpp:32-39 [@383f058].
// Ein Block Rauschen, verschachtelt I/Q, wie ihn die Verbindung liefert.
QVector<float> block(int pairs, float value)
{
    QVector<float> v;
    v.reserve(pairs * 2);
    for (int i = 0; i < pairs; ++i) { v << value << value; }
    return v;
}

// NereusSDR from here to the end of the namespace.
//
// A complex tone exactly on bin kToneBin of a kFftSize transform, from
// sample index `first` for `pairs` pairs: a gap is a jump in `first`.
constexpr int kFftSize = 1024;
constexpr int kToneBin = 100;
constexpr int kGapPairs = 37;   // lost samples: a phase jump of ~221 degrees
constexpr float kToneAmplitude = 0.5f;

QVector<float> tone(qint64 first, int pairs)
{
    QVector<float> v;
    v.reserve(pairs * 2);
    constexpr double kTwoPi = 6.283185307179586;
    for (int i = 0; i < pairs; ++i) {
        const double phase = kTwoPi * kToneBin * static_cast<double>(first + i)
                             / static_cast<double>(kFftSize);
        v << static_cast<float>(kToneAmplitude * std::cos(phase))
          << static_cast<float>(kToneAmplitude * std::sin(phase));
    }
    return v;
}

// The floor of a frame away from its tone, in dB below the tone's bin: the
// median of every bin more than kSkirtBins from the peak. A clean tone
// leaves it near the float floor; a jump in the window is broadband and
// lifts it across the whole width.
constexpr int kSkirtBins = 32;
// The two sides of the line, with a wide margin each way (measured on
// 2026-10-22: about -170 dB clean, -47 dB and -66 dB across the two seams
// below).
constexpr double kCleanFloorDb = -110.0;
constexpr double kStepFloorDb = -90.0;

double floorBelowToneDb(const QVector<float>& binsLinear)
{
    const int n = binsLinear.size();
    const auto peakIt = std::max_element(binsLinear.cbegin(), binsLinear.cend());
    const int peak = static_cast<int>(peakIt - binsLinear.cbegin());
    std::vector<double> far;
    for (int i = 0; i < n; ++i) {
        const int d = std::abs(i - peak);
        if (std::min(d, n - d) > kSkirtBins) {
            far.push_back(std::max(static_cast<double>(binsLinear[i]), 1e-30));
        }
    }
    std::nth_element(far.begin(), far.begin() + static_cast<std::ptrdiff_t>(far.size() / 2), far.end());
    return 10.0 * std::log10(far[far.size() / 2] / static_cast<double>(*peakIt));
}

QVector<float> frameBins(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(1).value<QVector<float>>();
}

// The stall-bound cases: a 1024 FFT at 48 kHz and 60 frames a second
// advances 800 samples a frame, so one frame interval is 800 pairs and the
// bound lets a gap through once 2400 pairs have passed with no frame.
constexpr double kBoundRateHz = 48000.0;
constexpr int kBoundFps = 60;
constexpr int kBoundIntervalPairs = 800;
// processFrame holds a frame that would come within 5 ms of the last one;
// each round of these cases waits this long, a lower bound, so the rate
// they see is the samples' and not the wall clock's.
constexpr int kPastMinimumFrameGapMs = 6;

// A sequence number one less than the next, as a radio would repeat it
// (skipEp6Sequence and skipDdcSequence add, modulo 2^32).
constexpr quint32 kBackOne = 0xFFFFFFFFu;
constexpr quint32 kBackThree = 0xFFFFFFFDu;

RadioInfo p1Info(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
    return info;
}

} // namespace

// NereusSDR: Longpath writes `{ Q_OBJECT` on the class line; Q_OBJECT
// stands on its own line here so AutoMoc sees it.
class TstFftWindowAfterGap : public QObject {
    Q_OBJECT

private slots:

    // From Longpath tests/tst_fft_window_after_gap.cpp:46-101 [@383f058].
    void aFullWindowGivesAFrame()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);
        // Gerechnet wird, wenn nach dem vollen Fenster der naechste Wert
        // ankommt — darum ein paar Werte mehr als die Fenstergroesse.
        engine.feedIQ(block(1024 + 16, 0.1f));
        QVERIFY2(frames.count() >= 1, "Ein volles Fenster hat kein Bild ergeben");
    }

    void theDiscardedWindowDoesNotProduceAFrame()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        // Halbes Fenster — noch kein Bild.
        engine.feedIQ(block(512, 0.1f));
        QCOMPARE(frames.count(), 0);
        QCOMPARE(engine.windowResets(), quint64(0));

        // Loch im Strom.
        engine.requestWindowReset();

        // Die zweite Haelfte allein darf jetzt KEIN Bild mehr ergeben:
        // das angefangene Fenster ist verworfen, es fehlen wieder 512.
        engine.feedIQ(block(512, 0.1f));
        QCOMPARE(frames.count(), 0);
        QCOMPARE(engine.windowResets(), quint64(1));

        // Erst mit dem Rest frischer Werte kommt wieder ein Bild.
        engine.feedIQ(block(512 + 16, 0.1f));
        QVERIFY2(frames.count() >= 1,
                 "Nach dem Verwerfen kam kein Bild mehr, obwohl genug frische Werte da waren");
    }

    void theFlagIsPickedUpOnTheFeedingThreadOnly()
    {
        // requestWindowReset() darf von jedem Faden kommen; wirksam wird
        // es erst im naechsten feedIQ(). Der Zaehler bleibt so lange 0.
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.requestWindowReset();
        QCOMPARE(engine.windowResets(), quint64(0));
        engine.feedIQ(block(8, 0.0f));
        QCOMPARE(engine.windowResets(), quint64(1));
        // Zweimal hintereinander gesetzt zaehlt einmal — die Fahne ist
        // ein Zustand, kein Ereigniszaehler.
        engine.requestWindowReset();
        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.0f));
        QCOMPARE(engine.windowResets(), quint64(2));
    }

    // NereusSDR: the control. Half a window of tone, a gap of kGapPairs
    // lost samples, the rest of the window: with no reset the frame spans
    // the seam and its floor stands far above a clean tone's. This is what
    // the next case would see without the fix, so the line can be seen.
    void withoutAResetTheSeamLiftsTheFloorAcrossTheWidth()
    {
        FFTEngine engine(0);
        engine.setFftSize(kFftSize);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        engine.feedIQ(tone(0, kFftSize / 2));
        engine.feedIQ(tone(kFftSize / 2 + kGapPairs, kFftSize / 2 + 16));
        QCOMPARE(frames.count(), 1);
        const double floorDb = floorBelowToneDb(frameBins(frames, 0));
        QVERIFY2(floorDb > kStepFloorDb,
                 qPrintable(QStringLiteral("a seam left the floor at %1 dB").arg(floorDb)));
    }

    // NereusSDR: the acceptance case. The same feed with the gap reported:
    // the next frame is the tone alone, with no full-width step.
    void afterAGapTheNextFrameHasNoFullWidthStep()
    {
        FFTEngine engine(0);
        engine.setFftSize(kFftSize);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        engine.feedIQ(tone(0, kFftSize / 2));
        engine.requestWindowReset();
        engine.feedIQ(tone(kFftSize / 2 + kGapPairs, kFftSize + 16));
        QCOMPARE(frames.count(), 1);
        const double floorDb = floorBelowToneDb(frameBins(frames, 0));
        QVERIFY2(floorDb < kCleanFloorDb,
                 qPrintable(QStringLiteral("the frame after a gap has a floor at %1 dB")
                                .arg(floorDb)));
        QCOMPARE(engine.windowResets(), quint64(1));
    }

    // NereusSDR: with overlapping transforms the samples kept from the last
    // frame (the overlap tail) come from before the gap as well. The reset
    // drops them too. 48 kHz at 60 frames a second advances 800 of 1024
    // samples, so 224 are carried over; a twin engine fed the same without
    // the reset shows the seam they would leave.
    void theResetAlsoDropsTheOverlapTail()
    {
        FFTEngine engine(0);
        FFTEngine twin(1);
        for (FFTEngine* e : {&engine, &twin}) {
            e->setFftSize(kFftSize);
            e->setSampleRate(48000.0);
            e->setOutputFps(60);
        }
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);
        QSignalSpy twinFrames(&twin, &FFTEngine::fftReadyLinear);

        engine.feedIQ(tone(0, kFftSize + 1));
        twin.feedIQ(tone(0, kFftSize + 1));
        QCOMPARE(frames.count(), 1);
        QCOMPARE(twinFrames.count(), 1);
        // Past processFrame's 5 ms minimum gap, so the next frame is not
        // held back (a lower bound: a busy machine only waits longer).
        QTest::qWait(10);

        engine.requestWindowReset();
        const qint64 after = kFftSize + 1 + kGapPairs;
        engine.feedIQ(tone(after, kFftSize + 16));
        twin.feedIQ(tone(after, kFftSize + 16));
        QCOMPARE(frames.count(), 2);
        QCOMPARE(twinFrames.count(), 2);

        const double floorDb = floorBelowToneDb(frameBins(frames, 1));
        const double twinFloorDb = floorBelowToneDb(frameBins(twinFrames, 1));
        QVERIFY2(twinFloorDb > kStepFloorDb,
                 qPrintable(QStringLiteral("the overlap seam left the floor at %1 dB")
                                .arg(twinFloorDb)));
        QVERIFY2(floorDb < kCleanFloorDb,
                 qPrintable(QStringLiteral("the frame after a gap has a floor at %1 dB")
                                .arg(floorDb)));
    }

    // NereusSDR: the gap goes through the map the samples take, so it
    // reaches the stream whose samples it broke and no other. -1 (Protocol
    // 1) is every stream; a hardware receiver no stream reads says nothing.
    void theGapReachesTheStreamItsSamplesTake()
    {
        ReceiverManager rm;
        const int first = rm.createReceiver();
        const int second = rm.createReceiver();
        QVERIFY(first >= 0 && second >= 0);
        rm.setDdcMapping(first, 2);
        rm.setDdcMapping(second, 3);
        rm.activateReceiver(first);
        rm.activateReceiver(second);

        QSignalSpy gaps(&rm, &ReceiverManager::iqSequenceGapForReceiver);
        QSignalSpy data(&rm, &ReceiverManager::iqDataForReceiver);

        rm.feedIqData(3, tone(0, 4));
        QCOMPARE(data.count(), 1);
        rm.noteIqSequenceGap(3);
        QCOMPARE(gaps.count(), 1);
        QCOMPARE(gaps.at(0).at(0).toInt(), data.at(0).at(0).toInt());
        QCOMPARE(gaps.at(0).at(0).toInt(), second);

        rm.noteIqSequenceGap(2);
        QCOMPARE(gaps.count(), 2);
        QCOMPARE(gaps.at(1).at(0).toInt(), first);

        rm.noteIqSequenceGap(-1);
        QCOMPARE(gaps.count(), 3);
        QCOMPARE(gaps.at(2).at(0).toInt(), -1);

        rm.noteIqSequenceGap(5);
        QCOMPARE(gaps.count(), 3);
    }

    // NereusSDR: Protocol 1. A gap in the EP6 sequence sends one gap for
    // every receiver, after the frames before it and before the samples of
    // the frame that showed it. Frames in sequence send none.
    void p1SequenceMismatchSendsOneGapBeforeTheSamples()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QStringList events;
        QObject ctx;
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &ctx,
                         [&events](int rx, const QVector<float>&) {
                             if (rx == 0) { events << QStringLiteral("iq"); }
                         });
        QObject::connect(&conn, &RadioConnection::iqSequenceGap, &ctx,
                         [&events](int rx) {
                             events << QStringLiteral("gap%1").arg(rx);
                         });
        conn.connectToRadio(p1Info(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(3);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 3, 3000);
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(2);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 5, 3000);

        QCOMPARE(events, (QStringList{QStringLiteral("iq"), QStringLiteral("iq"),
                                      QStringLiteral("iq"), QStringLiteral("gap-1"),
                                      QStringLiteral("iq"), QStringLiteral("iq")}));
        conn.disconnect();
        fake.stop();
    }

    // NereusSDR: Protocol 2. A DDC sequence error sends the gap for that
    // DDC, before the samples of the packet that showed it.
    void p2SequenceErrorSendsTheGapForItsDdcBeforeTheSamples()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection conn;
        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        QStringList events;
        QObject ctx;
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &ctx,
                         [&events](int ddc, const QVector<float>&) {
                             events << QStringLiteral("iq%1").arg(ddc);
                         });
        QObject::connect(&conn, &RadioConnection::iqSequenceGap, &ctx,
                         [&events](int ddc) {
                             events << QStringLiteral("gap%1").arg(ddc);
                         });
        conn.connectToRadio(fake.radioInfo());
        QTRY_VERIFY_WITH_TIMEOUT(fake.hasClient(), 3000);

        for (int i = 0; i < 3; ++i) {
            fake.sendDdc(2);
        }
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq2")), 3, 3000);
        fake.skipDdcSequence(3);
        fake.sendDdc(2);
        fake.sendDdc(2);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq2")), 5, 3000);

        QCOMPARE(events, (QStringList{QStringLiteral("iq2"), QStringLiteral("iq2"),
                                      QStringLiteral("iq2"), QStringLiteral("gap2"),
                                      QStringLiteral("iq2"), QStringLiteral("iq2")}));
        conn.disconnect();
        fake.stop();
    }

    // NereusSDR, 2026-10-23: Longpath's trigger (src/core/P1RadioConnection.cpp
    // :2715 [@383f058], `seq > m_ep6LastSeq + 1`). A repeated frame and a
    // frame from the past are counted as sequence errors but lose nothing,
    // so they send no gap; a forward jump afterwards still does.
    void p1RepeatedOrReorderedFrameSendsNoGap()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QStringList events;
        QObject ctx;
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &ctx,
                         [&events](int rx, const QVector<float>&) {
                             if (rx == 0) { events << QStringLiteral("iq"); }
                         });
        QObject::connect(&conn, &RadioConnection::iqSequenceGap, &ctx,
                         [&events](int rx) {
                             events << QStringLiteral("gap%1").arg(rx);
                         });
        conn.connectToRadio(p1Info(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(3);              // 0, 1, 2
        fake.skipEp6Sequence(kBackOne);
        fake.sendEp6Frames(2);              // 2 again, then 3
        fake.skipEp6Sequence(kBackThree);
        fake.sendEp6Frames(1);              // 1: from the past
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 6, 3000);
        QCOMPARE(events.count(QStringLiteral("gap-1")), 0);

        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 7, 3000);
        QCOMPARE(events.count(QStringLiteral("gap-1")), 1);
        QCOMPARE(events.at(events.size() - 2), QStringLiteral("gap-1"));
        conn.disconnect();
        fake.stop();
    }

    // NereusSDR, 2026-10-23: Longpath's trigger for Protocol 2
    // (src/core/P2RadioConnection.cpp:3234 [@383f058], `delta > 0`).
    void p2RepeatedOrReorderedPacketSendsNoGap()
    {
        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection conn;
        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        QStringList events;
        QObject ctx;
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &ctx,
                         [&events](int ddc, const QVector<float>&) {
                             events << QStringLiteral("iq%1").arg(ddc);
                         });
        QObject::connect(&conn, &RadioConnection::iqSequenceGap, &ctx,
                         [&events](int ddc) {
                             events << QStringLiteral("gap%1").arg(ddc);
                         });
        conn.connectToRadio(fake.radioInfo());
        QTRY_VERIFY_WITH_TIMEOUT(fake.hasClient(), 3000);

        for (int i = 0; i < 3; ++i) {
            fake.sendDdc(2);                // 0, 1, 2
        }
        fake.skipDdcSequence(kBackOne);
        fake.sendDdc(2);                    // 2 again
        fake.sendDdc(2);                    // 3
        fake.skipDdcSequence(kBackThree);
        fake.sendDdc(2);                    // 1: from the past
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq2")), 6, 3000);
        QCOMPARE(events.count(QStringLiteral("gap2")), 0);

        fake.skipDdcSequence(3);
        fake.sendDdc(2);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq2")), 7, 3000);
        QCOMPARE(events.count(QStringLiteral("gap2")), 1);
        QCOMPARE(events.at(events.size() - 2), QStringLiteral("gap2"));
        conn.disconnect();
        fake.stop();
    }

    // NereusSDR, 2026-10-23: Longpath's throttle
    // (src/core/P1RadioConnection.cpp:2721-2723 [@383f058]). Two losses
    // read one after the other, well inside 20 ms, send one gap; a loss
    // after the throttle has run out sends the next.
    void twoLossesWithinTheThrottleSendOneGap()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QStringList events;
        QObject ctx;
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &ctx,
                         [&events](int rx, const QVector<float>&) {
                             if (rx == 0) { events << QStringLiteral("iq"); }
                         });
        QObject::connect(&conn, &RadioConnection::iqSequenceGap, &ctx,
                         [&events](int rx) {
                             events << QStringLiteral("gap%1").arg(rx);
                         });
        conn.connectToRadio(p1Info(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(3);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 3, 3000);
        // Both losses are on the socket before this thread reads either.
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(1);
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 5, 3000);
        QCOMPARE(events, (QStringList{QStringLiteral("iq"), QStringLiteral("iq"),
                                      QStringLiteral("iq"), QStringLiteral("gap-1"),
                                      QStringLiteral("iq"), QStringLiteral("iq")}));

        // A lower bound: a busy machine only waits longer.
        QTest::qWait(static_cast<int>(RadioConnection::kIqSequenceGapMinIntervalMs) + 10);
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(events.count(QStringLiteral("iq")), 6, 3000);
        QCOMPARE(events.count(QStringLiteral("gap-1")), 2);
        QCOMPARE(events.at(events.size() - 2), QStringLiteral("gap-1"));
        conn.disconnect();
        fake.stop();
    }

    // NereusSDR, 2026-10-23, JJ's ruling (option a): a link that loses a
    // packet more often than once a window. Without the stall bound every
    // gap drops the window before it fills and no frame ever comes out (the
    // frozen waterfall). With it, a gap after kGapResetStallFrameIntervals
    // frame intervals with no frame is let through, and frames keep coming
    // at a steady rate: never more rounds apart than the bound plus one
    // window takes to pass.
    void aLossyLinkKeepsProducingFrames()
    {
        FFTEngine engine(0);
        engine.setFftSize(kFftSize);
        engine.setSampleRate(kBoundRateHz);
        engine.setOutputFps(kBoundFps);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        constexpr int kBatchPairs = kFftSize / 2 - 112;   // 400: a gap every 400
        constexpr int kRounds = 60;
        constexpr int kMaxRoundsApart =
            (FFTEngine::kGapResetStallFrameIntervals * kBoundIntervalPairs + kFftSize
             + kBatchPairs - 1) / kBatchPairs + 1;
        qint64 first = 0;
        int lastFrameRound = -1;
        int longestApart = 0;
        for (int round = 0; round < kRounds; ++round) {
            const int before = frames.count();
            engine.requestWindowReset();
            engine.feedIQ(tone(first, kBatchPairs));
            first += kBatchPairs + kGapPairs;
            if (frames.count() > before) {
                longestApart = std::max(longestApart, round - lastFrameRound);
                lastFrameRound = round;
            }
            QTest::qWait(kPastMinimumFrameGapMs);
        }
        longestApart = std::max(longestApart, kRounds - 1 - lastFrameRound);

        QVERIFY2(frames.count() >= kRounds / kMaxRoundsApart,
                 qPrintable(QStringLiteral("%1 frames in %2 rounds of a lossy link")
                                .arg(frames.count()).arg(kRounds)));
        QVERIFY2(longestApart <= kMaxRoundsApart,
                 qPrintable(QStringLiteral("%1 rounds without a frame (bound %2)")
                                .arg(longestApart).arg(kMaxRoundsApart)));
        QVERIFY(engine.windowResetsSkipped() > 0);
        // The gaps the bound did not let through still dropped the window.
        QVERIFY(engine.windowResets() > 0);
        QCOMPARE(engine.windowResets() + engine.windowResetsSkipped(),
                 quint64(kRounds));
    }

    // NereusSDR, 2026-10-23: the other side of the bound. On a link making
    // frames at its normal rate, one gap still drops the window as before:
    // it is counted as a reset, not skipped, and the next round, short of a
    // window, makes no frame.
    void aSingleGapOnAHealthyLinkStillDropsTheWindow()
    {
        FFTEngine engine(0);
        engine.setFftSize(kFftSize);
        engine.setSampleRate(kBoundRateHz);
        engine.setOutputFps(kBoundFps);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        qint64 first = 0;
        engine.feedIQ(tone(first, kFftSize + 1));
        first += kFftSize + 1;
        for (int round = 0; round < 4; ++round) {
            QTest::qWait(kPastMinimumFrameGapMs);
            engine.feedIQ(tone(first, kBoundIntervalPairs));
            first += kBoundIntervalPairs;
        }
        QCOMPARE(frames.count(), 5);   // one a frame interval

        QTest::qWait(kPastMinimumFrameGapMs);
        engine.feedIQ(tone(first, kBoundIntervalPairs / 2));
        first += kBoundIntervalPairs / 2 + kGapPairs;
        engine.requestWindowReset();
        engine.feedIQ(tone(first, kBoundIntervalPairs));
        first += kBoundIntervalPairs;
        QCOMPARE(engine.windowResets(), quint64(1));
        QCOMPARE(engine.windowResetsSkipped(), quint64(0));
        QCOMPARE(frames.count(), 5);   // 800 fresh pairs: short of a window

        // The window fills from the gap on, and its frame is clean.
        engine.feedIQ(tone(first, kFftSize - kBoundIntervalPairs + 1));
        QCOMPARE(frames.count(), 6);
        const double floorDb = floorBelowToneDb(frameBins(frames, 5));
        QVERIFY2(floorDb < kCleanFloorDb,
                 qPrintable(QStringLiteral("the frame after a gap has a floor at %1 dB")
                                .arg(floorDb)));
    }
};

QTEST_MAIN(TstFftWindowAfterGap)
#include "tst_fft_window_after_gap.moc"
