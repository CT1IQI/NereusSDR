// tests/tst_remote_waterfall_cadence.cpp
//
// no-port-check: NereusSDR-original. The remote waterfall's row cadence;
// Thetis has no remote display, so there is no upstream source to cite.
//
// A remote pan asks the Core for ceil(update period x fps / 1000) frames a
// row, so a 42 ms update period at 30 fps gets a row every 66.67 ms. The
// waterfall ticker, which takes one queued row a tick, ran at the 42 ms
// update period: one tick in three found no row, and the waterfall moved,
// moved and paused several times a second (JJ's window, 2026-10-21). These
// pin the ticker, the scroll glide and the time scale to the Core's row
// period on a remote pan, and the ticker's slots to a drift-free cadence
// for a period that is not whole milliseconds.

#include "gui/SpectrumWidget.h"
#include "gui/spectrum/WaterfallTicker.h"
#include "core/session/media/SpectrumEndpoint.h"

#include <QtTest>

using namespace NereusSDR;

namespace {

constexpr qint64 kMs = 1'000'000;
// Two frames a row at 30 fps, as RemoteMediaController computes it.
constexpr qint64 kTwoFrameRowNs = 2 * 1'000'000'000LL / 30;

SpectrumEndpointContext remoteContext(int fps, int framesPerLine)
{
    SpectrumEndpointContext context;
    context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
    context.exactCentreHz = 14200000.0;
    context.exactSpanHz = 48000.0;
    context.targetFps = fps;
    context.framesPerLine = framesPerLine;
    return context;
}

} // namespace

class TstRemoteWaterfallCadence : public QObject {
    Q_OBJECT

private slots:
    void tickSlotsDoNotDriftForAFractionalPeriod()
    {
        // Each tick fires when its timer says, and arms the next from there.
        qint64 elapsedNs = 0;
        for (qint64 tick = 1; tick <= 150; ++tick) {
            const int waitMs = WaterfallTicker::msUntilTick(elapsedNs, tick, kTwoFrameRowNs);
            elapsedNs += qint64(waitMs) * kMs;
            // Never before its slot, and never a whole millisecond after it.
            QVERIFY(elapsedNs >= tick * kTwoFrameRowNs);
            QVERIFY(elapsedNs - tick * kTwoFrameRowNs < kMs);
        }
        // 150 rows of 66.67 ms are 10 s, not 150 x 67 ms = 10.05 s.
        QVERIFY(qAbs(elapsedNs - 10'000 * kMs) < kMs);
    }

    void tickAlreadyDueWaitsNothing()
    {
        QCOMPARE(WaterfallTicker::msUntilTick(70 * kMs, 1, kTwoFrameRowNs), 0);
        QCOMPARE(WaterfallTicker::msUntilTick(0, 1, 42 * kMs), 42);
    }

    void tickerKeepsItsBounds()
    {
        WaterfallTicker ticker;
        ticker.setUpdatePeriodNs(kTwoFrameRowNs);
        QCOMPARE(ticker.updatePeriodNs(), kTwoFrameRowNs);
        ticker.setUpdatePeriodMs(2);
        QCOMPARE(ticker.updatePeriodNs(), WaterfallTicker::kMinPeriodNs);
        ticker.setUpdatePeriodNs(2'000 * kMs);
        QCOMPARE(ticker.updatePeriodNs(), WaterfallTicker::kMaxPeriodNs);
    }

    void tickerKeepsTickingOnItsThread()
    {
        // The timer is single-shot now: each tick must arm the next.
        QThread thread;
        auto* ticker = new WaterfallTicker();
        ticker->moveToWorkerThread(&thread);
        connect(&thread, &QThread::finished, ticker, &QObject::deleteLater);
        QAtomicInt ticks;
        connect(ticker, &WaterfallTicker::tick, ticker, [&ticks] { ticks.ref(); },
                Qt::DirectConnection);
        thread.start();
        ticker->setUpdatePeriodNs(WaterfallTicker::kMinPeriodNs);
        ticker->start();
        // Five ticks of 10 ms; the wait is generous for a loaded machine.
        QTRY_VERIFY_WITH_TIMEOUT(ticks.loadRelaxed() >= 5, 5000);
        ticker->stop();
        thread.quit();
        QVERIFY(thread.wait(5000));
    }

    void remotePanTicksAtTheCoresRowPeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);
        QCOMPARE(widget.waterfallRowPeriodMsForTest(), 42.0);

        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), kTwoFrameRowNs);
        QVERIFY(qAbs(widget.waterfallRowPeriodMsForTest() - 66.6667) < 0.001);

        // The same picture at one frame a row: the ticker follows it.
        widget.setRemoteSpectrumContext(remoteContext(30, 1), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 1'000'000'000LL / 30);

        // The update period setting no longer sets a remote pan's cadence.
        widget.setWfUpdatePeriodMs(50);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 1'000'000'000LL / 30);
    }

    void contextWithoutARowPeriodKeepsTheUpdatePeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        // An older Core sends no frame rate: nothing to tick at but the pan's own.
        widget.setRemoteSpectrumContext(remoteContext(0, 0), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);

        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), kTwoFrameRowNs);
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);
    }

    void glideSpansTheCoresRowPeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        widget.setDssRowDivider(1);
        // Local: a 42 ms push period is one whole row's glide.
        QVERIFY(qAbs(widget.dssScrollIncrementForTest(42) - 1.0f) < 1e-4f);
        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        // Remote: 42 ms is under two thirds of the way to the next row.
        QVERIFY(qAbs(widget.dssScrollIncrementForTest(42) - 0.63f) < 1e-4f);
    }
};

QTEST_MAIN(TstRemoteWaterfallCadence)
#include "tst_remote_waterfall_cadence.moc"
