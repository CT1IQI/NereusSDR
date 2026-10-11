// no-port-check: NereusSDR-original test.
//
// tst_radio_slice_settings_restore.cpp
//
// RadioModel's half of "settings come back after a restart" (settings
// restore audit 2026-10-11):
//
//   * a change is saved under the slice it was made on, not under the
//     active slice (gap 5), and that includes AGC-T AUTO and its offset,
//     which scheduled no save at all (gap 2), and the FM, DIG and RTTY
//     values, which had no save trigger either (gap 1);
//   * a slice restored before its WDSP channel exists pushes its whole
//     restored state to that channel when the channel is switched on, the
//     way slice A's is pushed at connect (gap 4).
//
// No radio and no audio device: a connection that sends nothing, and WDSP
// channels opened through the same seam tst_notch_channel_sync uses. The
// settings store is the test sandbox's (TestSandboxInit).

#include <QtTest/QtTest>

#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/DspControlThread.h"
#include "core/RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
class SilentRadioConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit SilentRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

namespace {

constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 7100000.0;

// Detaches a stack-injected connection however the scope is left.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

bool laneIdle(RadioModel& model)
{
    return model.receiveLane() == nullptr || model.receiveLane()->waitIdleForTest(600000);
}

QString sliceKey(int index, const char* key)
{
    return QStringLiteral("Slice%1/").arg(index) + QLatin1String(key);
}

} // namespace

class TestRadioSliceSettingsRestore : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── A change is saved under the slice it was made on ────────────────

    void aChangeToANonActiveSliceIsSavedUnderThatSlice_data()
    {
        QTest::addColumn<QByteArray>("property");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("stored");

        const auto row = [](const char* property, const QVariant& value, const char* key,
                            const char* stored) {
            QTest::newRow(property) << QByteArray(property) << value
                                    << QString::fromLatin1(key) << QString::fromLatin1(stored);
        };
        // Handlers that saved the active slice instead.
        row("afGain",       33,    "AfGain",       "33");
        row("rfGain",       61,    "RfGain",       "61");
        row("muted",        true,  "Muted",        "True");
        row("locked",       true,  "Locked",       "True");
        row("ritHz",        150,   "RitHz",        "150");
        row("snbEnabled",   true,  "SnbEnabled",   "True");
        row("anfEnabled",   true,  "AnfEnabled",   "True");
        row("nb1Threshold", 42,    "Nb1Threshold", "42");
        row("audioPan",     -0.5,  "AudioPan",     "-0.5");
        row("ssqlEnabled",  true,  "SsqlEnabled",  "True");
        row("apfTuneHz",    120,   "ApfTuneHz",    "120");
        row("binauralEnabled", true, "BinauralEnabled", "True");
        // AGC-T AUTO and its offset scheduled no save at all.
        row("autoAgcEnabled", true, "AgcAutoEnabled", "True");
        row("autoAgcOffset",  12.5, "AgcAutoOffset",  "12.5");
        // Neither did the FM, DIG and RTTY values.
        row("fmCtcssMode",    1,      "FmCtcssMode",    "1");
        row("fmCtcssValueHz", 123.0,  "FmCtcssValueHz", "123");
        row("fmOffsetHz",     600000, "FmOffsetHz",     "600000");
        row("fmTxMode",       QVariant::fromValue(FmTxMode::High), "FmTxMode", "0");
        row("fmReverse",      true,   "FmReverse",      "True");
        row("diglOffsetHz",   1500,   "DiglOffsetHz",   "1500");
        row("diguOffsetHz",   -1200,  "DiguOffsetHz",   "-1200");
        row("rttyMarkHz",     2125,   "RttyMarkHz",     "2125");
        row("rttyShiftHz",    850,    "RttyShiftHz",    "850");
    }

    void aChangeToANonActiveSliceIsSavedUnderThatSlice()
    {
        QFETCH(QByteArray, property);
        QFETCH(QVariant, value);
        QFETCH(QString, key);
        QFETCH(QString, stored);

        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};

        SliceModel* a = radio.sliceById(radio.addSlice());
        SliceModel* b = radio.sliceById(radio.addSlice());
        QVERIFY(a);
        QVERIFY(b);
        QCOMPARE(radio.activeSlice(), a);
        QCOMPARE(b->sliceIndex(), 1);
        // Anything the two adds scheduled is written and out of the way.
        radio.flushPendingSettingsSave();
        AppSettings::instance().clear();

        QVERIFY2(b->setProperty(property.constData(), value), property.constData());
        radio.flushPendingSettingsSave();

        auto& s = AppSettings::instance();
        const QString own = QStringLiteral("Slice1/") + key;
        QVERIFY2(s.contains(own), qPrintable(own + QStringLiteral(" was not saved")));
        QCOMPARE(s.value(own).toString(), stored);
        // The active slice did not change and is not what gets written.
        QVERIFY2(!s.contains(QStringLiteral("Slice0/LastBand")),
                 "the change was saved under the active slice");
    }

    // A manual AGC-T change still turns AUTO off (Thetis), and that is
    // saved for the same slice.
    void aManualThresholdChangeTurnsAutoOffAndSavesIt()
    {
        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};

        radio.addSlice();
        SliceModel* b = radio.sliceById(radio.addSlice());
        QVERIFY(b);
        b->setAutoAgcEnabled(true);
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(sliceKey(1, "AgcAutoEnabled")).toString(),
                 QStringLiteral("True"));

        b->setAgcThreshold(b->agcThreshold() - 7);
        QVERIFY(!b->autoAgcEnabled());
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(sliceKey(1, "AgcAutoEnabled")).toString(),
                 QStringLiteral("False"));
    }

    // ── A restored slice runs on what it shows ──────────────────────────

    // Slice B is restored while it has no WDSP channel (as the Core's
    // startup layout and a device's slice made again are), so no live push
    // reaches a channel. When its channel is switched on it must carry the
    // restored state, not WDSP's defaults.
    void aRestoredSliceBPushesItsStateToItsChannel()
    {
        // What slice B was saved with.
        {
            SliceModel saved;
            saved.setSliceIndex(1);
            saved.setFrequency(kSliceBFreqHz);
            saved.setAgcThreshold(-77);
            saved.setAgcHang(123);
            saved.setAgcSlope(3);
            saved.setAgcAttack(4);
            saved.setAgcDecay(321);
            saved.setAgcHangThreshold(55);
            saved.setAgcFixedGain(17);
            saved.setAgcMaxGain(88);
            saved.setNbMode(NbMode::NB);
            saved.setSnbEnabled(true);
            saved.setMuted(true);
            saved.setBinauralEnabled(true);
            saved.setApfEnabled(true);
            saved.setSsqlEnabled(true);
            saved.saveToSettings(Band::Band40m);
        }

        QTemporaryDir config;
        QVERIFY(config.isValid());
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine != nullptr);
        engine->setSynchronousInitForTest(true);
        QVERIFY(engine->initialize(config.path()));

        model.configureStreamPool(2, 2, kRateHz);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        QCOMPARE(b, 1);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        QVERIFY(engine->rxChannel(b) == nullptr);

        // Restored to the model before any channel exists.
        sliceB->restoreFromSettings(Band::Band40m);
        QCOMPARE(sliceB->agcThreshold(), -77);
        QCOMPARE(sliceB->agcHang(), 123);
        QCOMPARE(sliceB->agcDecay(), 321);
        QVERIFY(sliceB->streamIndex() >= 0);

        // The pool opens and switches on every bound slice's channel.
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        RxChannel* ch = engine->rxChannel(b);
        QVERIFY(ch != nullptr);
        QVERIFY(ch->isActive());
        QCOMPARE(ch->agcThreshold(), -77);
        QCOMPARE(ch->agcHang(), sliceB->agcHang());
        QCOMPARE(ch->agcSlope(), sliceB->agcSlope());
        QCOMPARE(ch->agcAttack(), sliceB->agcAttack());
        QCOMPARE(ch->agcDecay(), sliceB->agcDecay());
        QCOMPARE(ch->agcHangThreshold(), sliceB->agcHangThreshold());
        QCOMPARE(ch->agcFixedGain(), sliceB->agcFixedGain());
        QCOMPARE(ch->agcMaxGain(), sliceB->agcMaxGain());
        QCOMPARE(ch->nbMode(), NbMode::NB);
        QVERIFY(ch->snbEnabled());
        QVERIFY(ch->muted());
        QVERIFY(ch->binauralEnabled());
        QVERIFY(ch->apfEnabled());
        QVERIFY(ch->ssqlEnabled());
        QVERIFY(laneIdle(model));
    }
};

QTEST_MAIN(TestRadioSliceSettingsRestore)
#include "tst_radio_slice_settings_restore.moc"
