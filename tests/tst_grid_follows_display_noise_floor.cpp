// no-port-check: NereusSDR test of the port of Thetis's grid noise-floor
// tracking. Thetis moves the grid minimum to the display's own noise floor
// every 500 ms on tmrAutoAGC, not while transmitting, and only when the
// floor is "good" (not in fast attack):
//   console.cs:46136-46167 [v2.10.3.15] tmrAutoAGC_Tick
//   display.cs:5398-5404, 4670-4677, 925-934 [v2.10.3.15]
// The pan's own noise floor (NoiseFloorFollower) drives it, so it works with
// Clarity off and in a remote window; while Clarity feeds a pan its
// estimate, Clarity keeps driving that pan's grid (the add-on is kept).
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A pan reads the band from its first
//                                    frame with data, also at Log
//                                    Recursive. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A remote pan at Log Recursive reads
//                                    the band from its first floor, new
//                                    and across a Core restart.
//                                    AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include "core/session/media/DisplayCodec.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/spectrum/DisplayFollowers.h"
#include "gui/SpectrumWidget.h"

#include <cmath>
#include <optional>
#include <random>

using namespace NereusSDR;

class TestGridFollowsDisplayNoiseFloor : public QObject
{
    Q_OBJECT

private:
    static constexpr double kPanCentreHz = 14200000.0;
    static constexpr double kPanSpanHz   = 800.0;

    static void configure(SpectrumWidget& w)
    {
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        w.setDbmRange(-150.0f, -40.0f);
        w.setNFOffsetGridFollow(-5);
        w.setMaintainNFAdjustDelta(false);
        w.setAdjustGridMinToNoiseFloor(true);
        // Out of fast attack, at a known floor.
        w.setNoiseFloorFastAttack(false);
        w.setMeasuredNoiseFloorForTest(-120.0f);
    }

    static void feed(SpectrumWidget& w)
    {
        const QVector<float> bins(4096, 1e-12f);   // a flat floor
        w.updateSpectrumLinear(0, bins, 2.0, -10.0);
    }

private slots:
    void gridMinFollowsTheDisplayNoiseFloorWithoutClarity()
    {
        SpectrumWidget w;
        configure(w);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QVERIFY(std::abs(expected - w.gridMin()) >= 2);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax(), -40);   // Maintain grid range is off
    }

    void maintainGridRangeMovesTheMaxToo()
    {
        SpectrumWidget w;
        configure(w);
        w.setMaintainNFAdjustDelta(true);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax() - w.gridMin(), 110);
    }

    void holdsWhileTransmittingAndInFastAttack()
    {
        SpectrumWidget keyed;
        configure(keyed);
        keyed.setMoxOverlay(true);
        feed(keyed);
        SpectrumWidget attacking;
        configure(attacking);
        attacking.setNoiseFloorFastAttack(true);
        feed(attacking);
        QTest::qWait(1200);
        QCOMPARE(attacking.gridMin(), -150);
        keyed.setMoxOverlay(false);
        // The receive grid the pan keeps while keyed.
        QCOMPARE(keyed.gridMin(), -150);
    }

    void aPanClarityFeedsKeepsClaritysGrid()
    {
        SpectrumWidget w;
        configure(w);
        w.testApplyNoiseFloor(-100.0f);   // Clarity's estimate
        QCOMPARE(w.gridMin(), -105);
        feed(w);
        QTest::qWait(700);
        QCOMPARE(w.gridMin(), -105);
    }

    // JJ 2026-10-23: after a reconnect the window drew the waterfall bright
    // for seconds while its floor climbed from -200 dBm. A pan that starts,
    // or that drew frames with nothing in them while its stream was down,
    // reads the band from its first frame with data.
    void aPanReadsTheBandFromItsFirstFrameWithData()
    {
        SpectrumWidget w;
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        const QVector<float> nothing(4096, 0.0f);
        for (int frame = 0; frame < 5; ++frame) {
            w.updateSpectrumLinear(0, nothing, 2.0, -10.0);
        }
        QCOMPARE(w.nfLerpAverageForTest(), -200.0f);
        feed(w);
        const float first = w.nfLerpAverageForTest();
        for (int frame = 0; frame < 300; ++frame) {
            feed(w);
        }
        const float settled = w.nfLerpAverageForTest();
        QVERIFY2(settled > -150.0f, qPrintable(QString::number(settled)));
        QVERIFY2(std::abs(first - settled) < 1.0f,
                 qPrintable(QStringLiteral("first %1, settled %2").arg(first).arg(settled)));
        // The stream drops: frames with nothing leave the floor alone.
        for (int frame = 0; frame < 100; ++frame) {
            w.updateSpectrumLinear(0, nothing, 2.0, -10.0);
        }
        QCOMPARE(w.nfLerpAverageForTest(), settled);
    }

    // PR 628 review: at Log Recursive (the default) the average starts at
    // -160 dBm, so a fresh pan's first rows read below the band. The floor
    // is within 3 dB of where it settles from the first frame, at 30 ms and
    // at 650 ms.
    void aPanAtLogRecursiveReadsTheBandFromItsFirstFrame_data()
    {
        QTest::addColumn<int>("averageTimeMs");
        QTest::newRow("30 ms") << 30;
        QTest::newRow("650 ms") << 650;
    }

    void aPanAtLogRecursiveReadsTheBandFromItsFirstFrame()
    {
        QFETCH(int, averageTimeMs);
        const auto fresh = [averageTimeMs](SpectrumWidget& w) {
            w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
            w.setDdcCenterFrequency(kPanCentreHz);
            w.setSampleRate(kPanSpanHz);
            w.setSpectrumDetector(SpectrumDetector::Peak);
            w.setSpectrumAveraging(SpectrumAveraging::LogRecursive);
            w.setSpectrumAverageTimeMs(averageTimeMs);
        };
        SpectrumWidget settledPan;
        fresh(settledPan);
        for (int frame = 0; frame < 600; ++frame) {
            feed(settledPan);
        }
        const float settled = settledPan.nfLerpAverageForTest();
        QVERIFY2(settled > -150.0f, qPrintable(QString::number(settled)));

        SpectrumWidget w;
        fresh(w);
        for (int frame = 0; frame < 120; ++frame) {
            feed(w);
            QVERIFY2(std::abs(w.nfLerpAverageForTest() - settled) < 3.0f,
                     qPrintable(QStringLiteral("frame %1: %2, settled %3")
                                    .arg(frame).arg(w.nfLerpAverageForTest()).arg(settled)));
        }
    }

    // PR 628 re-review: a remote window's own floor line reads the Core's
    // averaged trace, which at Log Recursive starts again from -160 dBm on
    // a new picture. A real endpoint at Log Recursive, through the codec,
    // into a window: a brand-new pan, and a running window across a Core
    // restart, read within 3 dB of the settled floor on every frame they
    // have one, at 30 ms and 650 ms.
    void aRemotePanAtLogRecursiveReadsTheBandFromTheStart_data()
    {
        QTest::addColumn<int>("averageTimeMs");
        QTest::addColumn<bool>("coreRestarts");
        QTest::newRow("new pan, 30 ms") << 30 << false;
        QTest::newRow("new pan, 650 ms") << 650 << false;
        QTest::newRow("Core restart, 30 ms") << 30 << true;
        QTest::newRow("Core restart, 650 ms") << 650 << true;
    }

    void aRemotePanAtLogRecursiveReadsTheBandFromTheStart()
    {
        QFETCH(int, averageTimeMs);
        QFETCH(bool, coreRestarts);
        constexpr int kFps = 30;
        SpectrumEndpointSourceContext source;
        source.source = {2, FftTier::Wide};
        source.sourceGeneration = 11;
        source.fftBins = 1024;
        source.centreHz = kPanCentreHz;
        source.sampleRateHz = 192'000.0;
        source.contextGeneration = 4;
        SpectrumEndpointRequest request;
        request.endpointId = 91;
        request.source = source.source;
        request.centreHz = source.centreHz;
        request.spanHz = 96'000.0;
        request.pixels = 128;
        request.targetFps = kFps;
        request.framesPerLine = 1;
        // The window a pan with a -150 to -50 dBm scale asks for.
        request.minDbm = -150.0f;
        request.maxDbm = -50.0f;
        request.trace.detector = SpectrumDetectorMode::Peak;
        request.trace.averageMode = 3;
        request.trace.averageAlpha = averageAlphaForTimeMs(averageTimeMs, kFps);
        request.waterfall.detector = SpectrumDetectorMode::Peak;
        request.waterfall.averageMode = 0;

        // The Core: an endpoint and its encoder, started again on a restart.
        std::optional<SpectrumEndpoint> endpoint;
        std::optional<DisplayCodecEncoder> encoder;
        const auto startCore = [&] {
            endpoint.emplace();
            encoder.emplace();
            QVERIFY(endpoint->configure(request, source));
        };
        std::mt19937 rng(628);
        std::exponential_distribution<float> noise(1.0f);
        qint64 producedAtNs = 0;
        DisplayCodecDecoder decoder;
        // One frame from the Core into the window, through the codec.
        const auto deliver = [&](SpectrumWidget& w) {
            DaemonSpectrumFrame input;
            input.source = source.source;
            input.generation = source.sourceGeneration;
            input.centreHz = source.centreHz;
            input.sampleRateHz = source.sampleRateHz;
            producedAtNs += 34'000'000;
            input.producedAtNs = producedAtNs;
            input.windowEnb = 1.0;
            input.dbmOffset = 0.0;
            input.binsLinear.resize(source.fftBins);
            for (float& bin : input.binsLinear) { bin = 1.0e-12f * noise(rng); }
            input.binsLinear[300] = 1.0e-5f;
            const std::optional<DisplayCodecFrame> reduced = endpoint->consume(input, 0.0);
            QVERIFY(reduced.has_value());
            const QByteArray packet = encoder->encode(*reduced);
            QVERIFY(!packet.isEmpty());
            const DisplayCodecDecodeResult decoded = decoder.decode(packet);
            QVERIFY(decoded.disposition == DisplayCodecDisposition::Accepted);
            QVERIFY(w.updateRemoteSpectrum(decoded.frame));
        };
        SpectrumWidget w;
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::LogRecursive);
        w.setSpectrumAverageTimeMs(averageTimeMs);
        startCore();
        QVERIFY(!QTest::currentTestFailed());
        w.setRemoteSpectrumContext(endpoint->context(), source.centreHz, source.sampleRateHz);

        // Before a restart, or the reference for a new pan: 10 s, the last
        // 3 s of which is the settled floor.
        QVector<float> floors;
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
        }
        double settled = 0.0;
        for (int frame = 7 * kFps; frame < 10 * kFps; ++frame) { settled += floors.at(frame); }
        settled /= 3 * kFps;
        QVERIFY2(settled > -140.0 && settled < -110.0, qPrintable(QString::number(settled)));

        if (coreRestarts) {
            // The Core restarts: a new endpoint and encoder for the same
            // pan, a new picture, and frames from sequence 0 again.
            source.sourceGeneration = 1;
            source.contextGeneration = 1;
            startCore();
            decoder.reset();
            QVERIFY(!QTest::currentTestFailed());
            w.setRemoteSpectrumContext(endpoint->context(), source.centreHz,
                                       source.sampleRateHz, 0, false);
            floors.clear();
            for (int frame = 0; frame < 6 * kFps; ++frame) {
                deliver(w);
                QVERIFY(!QTest::currentTestFailed());
                floors.append(w.nfLerpAverageForTest());
            }
        }
        // A new pan has no floor until it has a frame it can read; it has
        // one within half a second.
        int first = 0;
        while (first < floors.size() && floors.at(first) <= -199.0f) { ++first; }
        double worst = 0.0;
        for (int frame = first; frame < floors.size(); ++frame) {
            worst = std::max(worst, std::abs(floors.at(frame) - settled));
        }
        qInfo("%s: settled %.2f dBm, first floor on frame %d, worst %.2f dB off",
              QTest::currentDataTag(), settled, first, worst);
        QVERIFY2(first < kFps / 2, qPrintable(QString::number(first)));
        for (int frame = first; frame < floors.size(); ++frame) {
            QVERIFY2(std::abs(floors.at(frame) - settled) < 3.0,
                     qPrintable(QStringLiteral("frame %1: %2, settled %3")
                                    .arg(frame).arg(floors.at(frame)).arg(settled)));
        }
    }

    void offDoesNothing()
    {
        SpectrumWidget w;
        configure(w);
        w.setAdjustGridMinToNoiseFloor(false);
        feed(w);
        QTest::qWait(1200);
        QCOMPARE(w.gridMin(), -150);
    }
};

QTEST_MAIN(TestGridFollowsDisplayNoiseFloor)
#include "tst_grid_follows_display_noise_floor.moc"
