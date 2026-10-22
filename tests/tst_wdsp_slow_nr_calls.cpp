// no-port-check: NereusSDR-original test of the slow noise-reduction call
// accounting (dsplock.c WdspNoteNrCallNs / TakeChannelSlowNrCalls) and the
// Core log line RadioModel builds from it, 2026-10-22.
#include <QtTest/QtTest>

#include "core/wdsp_api.h"
#include "models/RadioModel.h"

using NereusSDR::RadioModel;

namespace {

constexpr int kChannel = 5;
constexpr long long kMsNs = 1'000'000LL;

} // namespace

class TstWdspSlowNrCalls : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        long long count = 0;
        long long maxUs = 0;
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, &maxUs), 0);
    }

    void countsOnlyCallsOverFiftyMilliseconds()
    {
        WdspNoteNrCallNs(kChannel, 2 * kMsNs);
        WdspNoteNrCallNs(kChannel, 50 * kMsNs);       // not over 50 ms
        WdspNoteNrCallNs(kChannel, 50 * kMsNs + 1000);
        WdspNoteNrCallNs(kChannel, 1200 * kMsNs);
        WdspNoteNrCallNs(kChannel, 75 * kMsNs);
        long long count = -1;
        long long maxUs = -1;
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, &maxUs), 0);
        QCOMPARE(count, 3LL);
        QCOMPARE(maxUs, 1200000LL);
    }

    void aTakeStartsTheNextInterval()
    {
        WdspNoteNrCallNs(kChannel, 300 * kMsNs);
        long long count = 0;
        long long maxUs = 0;
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, &maxUs), 0);
        QCOMPARE(count, 1LL);
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, &maxUs), 0);
        QCOMPARE(count, 0LL);
        QCOMPARE(maxUs, 0LL);
    }

    void channelsAreCountedApart()
    {
        WdspNoteNrCallNs(kChannel + 1, 90 * kMsNs);
        long long count = 0;
        long long maxUs = 0;
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, &maxUs), 0);
        QCOMPARE(count, 0LL);
        QCOMPARE(TakeChannelSlowNrCalls(kChannel + 1, &count, &maxUs), 0);
        QCOMPARE(count, 1LL);
        QCOMPARE(maxUs, 90000LL);
    }

    void rejectsInvalidChannelsAndNullPointers()
    {
        long long count = 7;
        long long maxUs = 7;
        WdspNoteNrCallNs(-1, 900 * kMsNs);   // ignored, no crash
        QCOMPARE(TakeChannelSlowNrCalls(-1, &count, &maxUs), -1);
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, nullptr, &maxUs), -1);
        QCOMPARE(TakeChannelSlowNrCalls(kChannel, &count, nullptr), -1);
    }

    void logLineNamesReceiverModeAndTime()
    {
        QCOMPARE(RadioModel::slowNrCallLogLine(0, QStringLiteral("NNR Premium"), 3, 1012345),
                 QStringLiteral("Receiver 0: a noise reduction call took 1012.3 ms "
                                "(NR mode NNR Premium); 3 calls over 50 ms since the last report"));
    }
};

QTEST_GUILESS_MAIN(TstWdspSlowNrCalls)
#include "tst_wdsp_slow_nr_calls.moc"
