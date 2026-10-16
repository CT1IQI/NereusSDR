// no-port-check: test-only — exercises RadioConnection base-class rolling-window
// byte-rate counters. No Thetis logic is ported here; NereusSDR-original.
#include <QtTest/QtTest>
#include "core/RadioConnection.h"

#include <atomic>
#include <thread>

using namespace NereusSDR;

// Minimal concrete subclass so we can instantiate the abstract base.
class NullRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    NullRadioConnection() : RadioConnection() {}
    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
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
    void setWatchdogEnabled(bool) override {}
};

class TstRadioConnectionByteRates : public QObject {
    Q_OBJECT

private slots:
    void initialRatesAreZero() {
        NullRadioConnection rc;
        QCOMPARE(rc.txByteRate(1000), 0.0);
        QCOMPARE(rc.rxByteRate(1000), 0.0);
    }

    void recordBytesAccumulatesInWindow() {
        NullRadioConnection rc;
        rc.recordBytesReceived(1000);
        rc.recordBytesReceived(2000);
        // 3000 bytes within last 1 second → 24 kbps → 0.024 Mbps
        const double mbps = rc.rxByteRate(1000);
        QVERIFY(mbps >= 0.020 && mbps <= 0.030);
    }

    void recordBytesAgesOutOfWindow() {
        NullRadioConnection rc;
        rc.recordBytesReceived(10000);
        QTest::qWait(1100);
        QCOMPARE(rc.rxByteRate(1000), 0.0);
    }

    // The connection thread records while the main thread reads the rates
    // (the readouts): every byte recorded is counted, and under
    // ThreadSanitizer the two do not race.
    void recordAndReadFromTwoThreads() {
        NullRadioConnection rc;
        constexpr int kRecords = 20000;
        std::atomic<bool> done{false};
        std::thread writer([&] {
            for (int i = 0; i < kRecords; ++i) {
                rc.recordBytesSent(10);
                rc.recordBytesReceived(20);
            }
            done.store(true);
        });
        int reads = 0;
        while (!done.load()) {
            QVERIFY(rc.txByteRate(5000) >= 0.0);
            QVERIFY(rc.rxByteRate(5000) >= 0.0);
            ++reads;
        }
        writer.join();
        // All of it inside the 5 s window: Mbps = bytes * 8 / 5000 / 1e3.
        QCOMPARE(rc.txByteRate(5000), kRecords * 10 * 8.0 / (5000 * 1000.0));
        QCOMPARE(rc.rxByteRate(5000), kRecords * 20 * 8.0 / (5000 * 1000.0));
        QVERIFY(reads > 0);
    }

    void txAndRxAreIndependent() {
        NullRadioConnection rc;
        rc.recordBytesSent(5000);
        rc.recordBytesReceived(2000);
        QVERIFY(rc.txByteRate(1000) > rc.rxByteRate(1000));
    }
};

QTEST_APPLESS_MAIN(TstRadioConnectionByteRates)
#include "tst_radio_connection_byte_rates.moc"
