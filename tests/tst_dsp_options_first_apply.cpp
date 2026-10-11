// no-port-check: NereusSDR-original test — no Thetis source ported here.
// Comments below cite Thetis source paths for context only.
//
// Every receive channel takes the DSP > Options buffer size of its slice's
// mode before it runs a block, and again whenever that slice's mode changes.
// The filter size and filter type of that mode go with it.
//
// WDSP opens each receive channel with a DSP buffer of 4096 samples at
// 48,000 (cmaster.c:72-86 [v2.10.3.15]), 85 ms of signal per block. Thetis
// forces the configured size (64 by default, radio.cs:521-522) into every
// receiver once the channels exist (console.cs:2681-2692 SyncDSP) and sets
// each receiver from its own mode after that (console.cs:38975-38984).
//
// NereusSDR applied the sizes only when a slice's mode changed, and only to
// the first receiver: the others never had the engine handle the apply
// needs. On the Rock 5C on 2026-10-10 every receiver ran 85 ms blocks from
// the Core's start, and the first took its 64 only when its mode was next
// changed (the Core's log has one "bufSize: 4096 -> 64" line for the day).
//
// Real channels: openRxChannelPool runs createRxChannel's OpenChannel, as in
// tst_notch_channel_sync.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/DspControlThread.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kRateHz = 192000;
constexpr int kOpenedBlock = 4096;   // createRxChannel's dsp_size
constexpr int kPhoneBlock = 256;
constexpr int kCwBlock = 512;

bool laneIdle(RadioModel& model)
{
    return model.receiveLane() == nullptr || model.receiveLane()->waitIdleForTest(600000);
}

void saveBlockSizes()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("DspOptionsBufferSizePhoneRx"), QString::number(kPhoneBlock));
    s.setValue(QStringLiteral("DspOptionsBufferSizeCwRx"), QString::number(kCwBlock));
}

} // namespace

class TestDspOptionsFirstApply : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // Slices whose modes were set before any channel existed (a Core
    // restoring its slices, then connecting): each channel starts with the
    // block size of its own slice's mode.
    void each_receiver_starts_with_its_modes_block_size()
    {
        saveBlockSizes();
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setDspMode(DSPMode::USB);
        const int b = model.addSlice();
        model.sliceById(b)->setDspMode(DSPMode::CWU);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(engine->rxChannel(b) != nullptr);
        QCOMPARE(engine->rxChannel(a)->dspBlockSize(), kPhoneBlock);
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), kCwBlock);
    }

    // With nothing saved, the default of 64 (RxChannel::onModeChanged; From
    // Thetis radio.cs:521-522 [v2.10.3.15]), not the 4096 it was opened with.
    void a_receiver_starts_with_the_default_block_size()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QCOMPARE(engine->rxChannel(a)->dspBlockSize(), 64);
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), 64);
    }

    // A slice added once the pool is open binds to a channel that has been
    // sitting open at 4096.
    void a_slice_added_later_starts_with_its_modes_block_size()
    {
        saveBlockSizes();
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setDspMode(DSPMode::USB);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        const int b = model.addSlice();
        QVERIFY(model.sliceById(b)->streamIndex() >= 0);
        QVERIFY(laneIdle(model));
        QVERIFY(engine->rxChannel(b) != nullptr);
        const int expected =
            dspOptionsModeGroup(model.sliceById(b)->dspMode()) == QLatin1String("Cw")
                ? kCwBlock : kPhoneBlock;
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), expected);
    }

    // A channel no slice uses is left as it was opened.
    void a_channel_without_a_slice_keeps_the_size_it_was_opened_with()
    {
        saveBlockSizes();
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QVERIFY(engine->rxChannel(a) != nullptr);
        QVERIFY(engine->rxChannel(4) != nullptr);
        QCOMPARE(engine->rxChannel(4)->dspBlockSize(), kOpenedBlock);
    }

    // The filter type goes with the block size. WDSP opens a channel linear
    // phase (RXA.c create_nbp, mp 0); with nothing saved a receiver starts
    // Low Latency, as every Thetis receiver does (radio.cs:559-560, forced
    // in by SyncAll, radio.cs:384-388 [v2.10.3.15]). Before this the first
    // receiver took it at its first mode change and the others never did.
    void a_receiver_starts_with_the_default_filter_type()
    {
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        const int b = model.addSlice();
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QVERIFY(!engine->rxChannel(a)->filterTypeLinearPhase());
        QVERIFY(!engine->rxChannel(b)->filterTypeLinearPhase());
        // A channel no slice uses is left as it was opened.
        QVERIFY(engine->rxChannel(4)->filterTypeLinearPhase());
    }

    // Each receiver takes the type saved for its own slice's mode.
    void each_receiver_starts_with_its_modes_filter_type()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("DspOptionsFilterTypeCwRx"), QStringLiteral("Linear Phase"));
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setDspMode(DSPMode::USB);
        const int b = model.addSlice();
        model.sliceById(b)->setDspMode(DSPMode::CWU);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        QVERIFY(!engine->rxChannel(a)->filterTypeLinearPhase());
        QVERIFY(engine->rxChannel(b)->filterTypeLinearPhase());
    }

    // A mode change reaches a receiver other than the first. The slice's
    // mode handler (RadioModel::wireSliceSignals, wired only with a
    // connection) calls the channel's onModeChanged; a channel without the
    // engine handle returned from it having applied nothing.
    void a_mode_change_reaches_every_receiver()
    {
        saveBlockSizes();
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setDspMode(DSPMode::USB);
        const int b = model.addSlice();
        model.sliceById(b)->setDspMode(DSPMode::USB);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), kPhoneBlock);

        engine->rxChannel(b)->onModeChanged(DSPMode::CWU);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), kCwBlock);
        QCOMPARE(engine->rxChannel(a)->dspBlockSize(), kPhoneBlock);

        engine->rxChannel(b)->onModeChanged(DSPMode::USB);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), kPhoneBlock);
    }

    // A size picked on the local DSP > Options page (DspOptionsPage's combo
    // handler calls rebuildDspOptionsForMode with the combo's mode) reaches
    // every receiver whose slice is in that group, whatever mode the first
    // slice is in, and no receiver in another group.
    void a_setup_page_change_reaches_every_receiver_in_its_group()
    {
        saveBlockSizes();
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;

        model.configureStreamPool(5, 5, kRateHz);
        const int a = model.addSlice();
        model.sliceById(a)->setDspMode(DSPMode::USB);
        const int b = model.addSlice();
        model.sliceById(b)->setDspMode(DSPMode::LSB);
        const int c = model.addSlice();
        model.sliceById(c)->setDspMode(DSPMode::CWU);
        model.openRxChannelPool(5, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), kPhoneBlock);
        QCOMPARE(engine->rxChannel(c)->dspBlockSize(), kCwBlock);

        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("DspOptionsBufferSizePhoneRx"), QStringLiteral("128"));
        model.rebuildDspOptionsForMode(DSPMode::USB);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(a)->dspBlockSize(), 128);
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), 128);
        QCOMPARE(engine->rxChannel(c)->dspBlockSize(), kCwBlock);

        // The CW size, with the first slice in a Phone mode.
        s.setValue(QStringLiteral("DspOptionsBufferSizeCwRx"), QStringLiteral("1024"));
        model.rebuildDspOptionsForMode(DSPMode::CWU);
        QVERIFY(laneIdle(model));
        QCOMPARE(engine->rxChannel(c)->dspBlockSize(), 1024);
        QCOMPARE(engine->rxChannel(a)->dspBlockSize(), 128);
        QCOMPARE(engine->rxChannel(b)->dspBlockSize(), 128);
    }
};

QTEST_MAIN(TestDspOptionsFirstApply)
#include "tst_dsp_options_first_apply.moc"
