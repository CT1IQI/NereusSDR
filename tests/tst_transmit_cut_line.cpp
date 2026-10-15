// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_transmit_cut_line.cpp  (NereusSDR)
// =================================================================
//
// The transmit cut from a thread other than main (2026-10-15):
// RadioConnection's cut latch and TransmitCutLine, held by RadioModel.
//
//   cutRunsOnTheConnectionThreadWhileMainSleeps: cutTransmit() from a
//     std::thread while the main thread sleeps 500 ms runs MOX off and relay
//     off on the connection's own QThread within 50 ms; a second cut adds
//     nothing.
//   lineCutsAKeyedModelThenCatchUpRestores: with MOX keyed, the model's
//     cut line, cut from a std::thread while main sleeps 500 ms, closes the
//     RF gate and reaches the connection within 50 ms; catchUpTransmitCut
//     then gives one MOX-off walk and one transmitStopped with its text, no
//     key reaches the connection, the cut clears, and the next key reaches
//     the connection and opens the RF gate again.
//   catchUpWithNothingKeyedEmitsNothing: catchUpTransmitCut with nothing
//     keyed emits nothing and clears the cut.
//   clearedTargetsAreNotCut: once the model lets go of its connection and
//     TX channel, a cut reaches neither.
//
// REALTIME: wall-clock bounds while the main thread is held.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QThread>

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "RealtimeTestLoad.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/safety/TransmitCutLine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kMainHoldMs = 500;
constexpr double kReachBoundMs = 50.0;
// The bound tst_tx_link_loss_each_path uses (kBoundMs).
constexpr qint64 kCutBoundMs = RemoteTxWatchdog::kLinkLossDeadlineMs + 100;

double msBetween(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Records MOX and relay writes with their thread and time, from any thread.
class RecordingConnection : public RadioConnection {
    Q_OBJECT
public:
    struct Entry {
        QString what;
        QThread* thread{nullptr};
        Clock::time_point at;
    };

    RecordingConnection() : RadioConnection(nullptr)
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
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override { add(on ? QStringLiteral("MOX on") : QStringLiteral("MOX off")); }
    void setTrxRelay(bool on) override
    {
        add(on ? QStringLiteral("relay on") : QStringLiteral("relay off"));
    }
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}

    std::vector<Entry> entries() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries;
    }
    // Entries recorded from index `from` on.
    std::vector<Entry> since(std::size_t from) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (from >= m_entries.size()) {
            return {};
        }
        return std::vector<Entry>(m_entries.begin() + static_cast<std::ptrdiff_t>(from),
                                  m_entries.end());
    }
    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

private:
    void add(const QString& what)
    {
        const Clock::time_point now = Clock::now();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back({what, QThread::currentThread(), now});
    }

    mutable std::mutex m_mutex;
    std::vector<Entry> m_entries;
};

const RecordingConnection::Entry* findEntry(const std::vector<RecordingConnection::Entry>& list,
                                            const QString& what)
{
    for (const auto& e : list) {
        if (e.what == what) {
            return &e;
        }
    }
    return nullptr;
}

void pump(int passes = 8)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// Calls `fn` on a std::thread of its own, recording when it began, while the
// main thread sleeps kMainHoldMs without running its event loop.
Clock::time_point runOffMainWhileMainSleeps(const std::function<void()>& fn)
{
    std::atomic<qint64> startedNs{0};
    std::thread worker([&]() {
        startedNs.store(Clock::now().time_since_epoch().count());
        fn();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(kMainHoldMs));
    worker.join();
    return Clock::time_point(Clock::duration(startedNs.load()));
}

// A local model with a slice on 20 m USB, a recording connection on a
// QThread of its own (as the radio connection lives), a TX channel wrapper
// with no WDSP channel behind it wired for keying, and MoxController's walk
// driven by the event loop.
struct Rig {
    RadioModel model;
    RecordingConnection conn;
    QThread connThread;
    TxChannel tx{WdspEngine::kTxChannelId};

    Rig()
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        conn.moveToThread(&connThread);
        connThread.start();
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        if (SliceModel* slice = model.activeSlice()) {
            slice->setDspMode(DSPMode::USB);
            slice->setFrequency(14'200'000.0);
        }
        model.injectTxChannelForTest(&tx);
        model.wireTxChannelKeyingForTest();
    }
    ~Rig()
    {
        model.injectTxChannelForTest(nullptr);
        model.injectConnectionForTest(nullptr);
        connThread.quit();
        connThread.wait();
        AppSettings::instance().clear();
    }
};

} // namespace

class TestTransmitCutLine : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup()
    {
        AppSettings::instance().clear();
        RealtimeTestLoad::printLoadAverageIfFailed();
    }

    void cutRunsOnTheConnectionThreadWhileMainSleeps()
    {
        RecordingConnection conn;
        QThread connThread;
        conn.moveToThread(&connThread);
        connThread.start();

        const Clock::time_point cutAt = runOffMainWhileMainSleeps([&conn]() {
            conn.cutTransmit();
        });

        const auto entries = conn.entries();
        const auto* moxOff = findEntry(entries, QStringLiteral("MOX off"));
        const auto* relayOff = findEntry(entries, QStringLiteral("relay off"));
        qInfo("cut: MOX off +%.2f ms, relay off +%.2f ms (limit %.0f), entries %zu",
              moxOff ? msBetween(cutAt, moxOff->at) : -1.0,
              relayOff ? msBetween(cutAt, relayOff->at) : -1.0, kReachBoundMs, entries.size());
        QVERIFY2(moxOff && relayOff, "the cut did not reach the connection while main slept");
        QCOMPARE(entries.size(), std::size_t(2));
        QCOMPARE(entries.at(0).what, QStringLiteral("MOX off"));
        QCOMPARE(entries.at(1).what, QStringLiteral("relay off"));
        QCOMPARE(moxOff->thread, &connThread);
        QCOMPARE(relayOff->thread, &connThread);
        QVERIFY2(msBetween(cutAt, moxOff->at) < kReachBoundMs, "MOX off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < kReachBoundMs, "relay off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < double(kCutBoundMs), "past the cut bound");
        QVERIFY(conn.transmitCut());

        // A second cut adds nothing.
        std::thread again([&conn]() { conn.cutTransmit(); });
        again.join();
        QTest::qWait(100);
        QCOMPARE(conn.count(), std::size_t(2));
        QVERIFY(conn.transmitCut());

        // Cleared on its own thread; a later cut runs again.
        QMetaObject::invokeMethod(&conn, &RadioConnection::clearTransmitCut,
                                  Qt::QueuedConnection);
        QTRY_VERIFY_WITH_TIMEOUT(!conn.transmitCut(), 2000);
        conn.cutTransmit();
        QTRY_COMPARE_WITH_TIMEOUT(conn.count(), std::size_t(4), 2000);

        connThread.quit();
        connThread.wait();
    }

    void lineCutsAKeyedModelThenCatchUpRestores()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.mox() && rig.tx.isRfGateOpen()
                                     && findEntry(rig.conn.entries(), QStringLiteral("MOX on")),
                                 5000);

        QSignalSpy moxSpy(rig.model.moxController(), &MoxController::moxStateChanged);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        const std::size_t mark = rig.conn.count();
        const std::shared_ptr<TransmitCutLine> line = rig.model.transmitCutLine();
        QVERIFY(line != nullptr);

        const Clock::time_point cutAt = runOffMainWhileMainSleeps([line]() { line->cut(); });

        // Before the main thread runs anything: RF shut, the connection cut.
        const bool gateOpenAfterCut = rig.tx.isRfGateOpen();
        const auto afterCut = rig.conn.since(mark);
        const auto* moxOff = findEntry(afterCut, QStringLiteral("MOX off"));
        const auto* relayOff = findEntry(afterCut, QStringLiteral("relay off"));
        qInfo("line cut: MOX off +%.2f ms, relay off +%.2f ms (limit %.0f)",
              moxOff ? msBetween(cutAt, moxOff->at) : -1.0,
              relayOff ? msBetween(cutAt, relayOff->at) : -1.0, kReachBoundMs);
        QVERIFY2(!gateOpenAfterCut, "the cut left the RF gate open");
        QVERIFY2(moxOff && relayOff, "the cut did not reach the connection while main slept");
        QCOMPARE(moxOff->thread, &rig.connThread);
        QVERIFY2(msBetween(cutAt, moxOff->at) < kReachBoundMs, "MOX off was late");
        QVERIFY2(msBetween(cutAt, relayOff->at) < kReachBoundMs, "relay off was late");
        QVERIFY(rig.conn.transmitCut());
        QVERIFY(rig.model.mox());   // the model has not caught up yet

        const QString text = QStringLiteral("Remote link lost");
        rig.model.catchUpTransmitCut(text);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.mox()
                                     && rig.model.moxController()->state() == MoxState::Rx,
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 2000);
        pump();
        QTest::qWait(50);
        pump();

        int moxOffEdges = 0;
        for (const auto& args : moxSpy) {
            QCOMPARE(args.at(0).toBool(), false);
            ++moxOffEdges;
        }
        QCOMPARE(moxOffEdges, 1);
        QCOMPARE(stopped.count(), 1);
        QCOMPARE(stopped.at(0).at(0).toString(), text);
        QVERIFY2(!findEntry(rig.conn.since(mark), QStringLiteral("MOX on")),
                 "a key reached the connection after the cut");
        QVERIFY(!rig.tx.isRfGateOpen());

        // The next key keys normally: the connection hears it and the RF
        // gate the cut closed opens again.
        const std::size_t beforeKey = rig.conn.count();
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(findEntry(rig.conn.since(beforeKey), QStringLiteral("MOX on")),
                                 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 5000);
        QVERIFY(rig.model.mox());
        rig.model.moxController()->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->state() == MoxState::Rx, 5000);
    }

    void catchUpWithNothingKeyedEmitsNothing()
    {
        Rig rig;
        pump();
        QSignalSpy moxSpy(rig.model.moxController(), &MoxController::moxStateChanged);
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.model.transmitCutLine()->cut();
        QVERIFY(rig.conn.transmitCut());
        QTRY_VERIFY_WITH_TIMEOUT(findEntry(rig.conn.entries(), QStringLiteral("relay off")), 2000);
        const std::size_t mark = rig.conn.count();

        rig.model.catchUpTransmitCut(QStringLiteral("Remote link lost"));
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut(), 2000);
        pump();
        QTest::qWait(50);
        pump();

        QCOMPARE(moxSpy.count(), 0);
        QCOMPARE(stopped.count(), 0);
        QCOMPARE(rig.conn.count(), mark);
        QVERIFY(!rig.model.mox());
    }

    void clearedTargetsAreNotCut()
    {
        Rig rig;
        rig.model.moxController()->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.tx.isRfGateOpen(), 5000);
        const std::shared_ptr<TransmitCutLine> line = rig.model.transmitCutLine();

        rig.model.injectTxChannelForTest(nullptr);
        rig.model.injectConnectionForTest(nullptr);
        const std::size_t mark = rig.conn.count();
        line->cut();
        QTest::qWait(50);
        QVERIFY(rig.tx.isRfGateOpen());
        QVERIFY(!rig.conn.transmitCut());
        QCOMPARE(rig.conn.count(), mark);

        // Back on the model, the next cut reaches both.
        rig.model.injectConnectionForTest(&rig.conn);
        rig.model.injectTxChannelForTest(&rig.tx);
        line->cut();
        QVERIFY(!rig.tx.isRfGateOpen());
        QVERIFY(rig.conn.transmitCut());
        rig.model.catchUpTransmitCut(QString());
        QTRY_VERIFY_WITH_TIMEOUT(!rig.conn.transmitCut() && !rig.model.mox(), 5000);
    }
};

QTEST_MAIN(TestTransmitCutLine)
#include "tst_transmit_cut_line.moc"
