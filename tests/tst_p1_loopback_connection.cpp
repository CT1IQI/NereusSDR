// tests/tst_p1_loopback_connection.cpp
//
// Phase 3I Task 9 — end-to-end integration test for P1RadioConnection.
// Uses P1FakeRadio (loopback UDP) to verify:
//   1. State transitions: Disconnected → Connecting → Connected
//   2. iqDataReceived emits correctly-shaped interleaved I/Q QVector<float>
//   3. disconnect() stops the fake's "running" state
//   4. the radio's frames reach this connection's socket, and no other
//      socket on the machine bound to the address the radio answers can
//      take them (tst_slice_cap_every_path's lost reconnect)
//   5. onReadyRead reads a bounded batch per call, so the EP2 pacer keeps
//      sending while EP6 arrives faster than it is read (2026-10-14 HL2
//      TUNE stall)
//
// Uses QTEST_MAIN (not APPLESS_MAIN) — requires QCoreApplication for socket I/O.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QElapsedTimer>
#include <atomic>
#include <thread>
#include "core/P1RadioConnection.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"
#include "core/HermesLiteBandwidthMonitor.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {
// Issue #258 regression — message handler that counts "ep6 stream established"
// log lines emitted during the test.  Installed once per test method (init())
// and uninstalled (cleanup()) so the count is fresh between cases.
static std::atomic<int> g_ep6EstablishedLogCount{0};
static QtMessageHandler g_previousMsgHandler = nullptr;

static void countEp6EstablishedLogs(QtMsgType type,
                                    const QMessageLogContext& ctx,
                                    const QString& msg)
{
    if (msg.contains(QStringLiteral("Connected, ep6 stream established"))) {
        g_ep6EstablishedLogCount.fetch_add(1);
    }
    if (g_previousMsgHandler) {
        g_previousMsgHandler(type, ctx, msg);
    }
}
} // namespace

// ---------------------------------------------------------------------------
// EP6 flood check, reusable by any change to the P1 receive path.
//
// Ep6Flood builds `count` ep6 datagrams numbered in sequence from
// `firstSeq`, each sample of each receiver carrying its own 24-bit I and Q
// (frame, sample and receiver are all encoded in the value, Q negative), and
// records every frame and every iqDataReceived the connection emits. send()
// writes them straight to the connection's port in waves of `waveSize`; a
// wave equal to `count` is one burst. verify() then checks the receive path
// end to end:
//   - every frame is processed, with none dropped (frameReceived count, and
//     no new EP6 sequence gap on the bandwidth monitor);
//   - they arrive in sequence order (emission k matches frame k);
//   - the I/Q delivered on iqDataReceived is sample for sample what was sent.
// ---------------------------------------------------------------------------
class Ep6Flood {
public:
    Ep6Flood(P1RadioConnection& conn, int count, quint32 firstSeq, int numRx)
        : m_conn(conn), m_count(count), m_firstSeq(firstSeq), m_numRx(numRx)
    {
        QObject::connect(&conn, &RadioConnection::frameReceived, &m_ctx,
                         [this] { ++m_framesSeen; });
        QObject::connect(&conn, &RadioConnection::iqDataReceived, &m_ctx,
                         [this](int rx, const QVector<float>& samples) {
                             m_iq.append(qMakePair(rx, samples));
                         });
    }

    static int samplesPerSubframe(int numRx) { return 504 / (6 * numRx + 2); }

    // The 24-bit value a sample carries: frame, sample and receiver in its
    // bits, so a dropped, repeated or swapped frame or sample shows.
    static qint32 iValue(int frame, int sample, int rx) {
        return (qint32(frame) << 11) | (qint32(sample) << 3) | qint32(rx);
    }
    static qint32 qValue(int frame, int sample, int rx) {
        return -(iValue(frame, sample, rx) + 1);
    }

    QByteArray frame(int f) const {
        QByteArray d(1032, '\0');
        auto* b = reinterpret_cast<quint8*>(d.data());
        const quint32 seq = m_firstSeq + quint32(f);
        b[0] = 0xEF; b[1] = 0xFE; b[2] = 0x01; b[3] = 0x06;
        b[4] = quint8(seq >> 24); b[5] = quint8(seq >> 16);
        b[6] = quint8(seq >> 8);  b[7] = quint8(seq);
        b[8] = b[9] = b[10] = 0x7F;
        b[520] = b[521] = b[522] = 0x7F;
        const int slot = 6 * m_numRx + 2;
        const int spr = samplesPerSubframe(m_numRx);
        auto put24 = [b](int off, qint32 v) {
            const quint32 u = quint32(v) & 0xFFFFFFu;
            b[off] = quint8(u >> 16); b[off + 1] = quint8(u >> 8); b[off + 2] = quint8(u);
        };
        for (int sub = 0; sub < 2; ++sub) {
            const int base = sub == 0 ? 16 : 528;
            for (int s = 0; s < spr; ++s) {
                for (int r = 0; r < m_numRx; ++r) {
                    const int off = base + s * slot + r * 6;
                    const int sample = sub * spr + s;
                    put24(off, iValue(f, sample, r));
                    put24(off + 3, qValue(f, sample, r));
                }
            }
        }
        return d;
    }

    // Sends every frame to `port` on the loopback, `waveSize` at a time,
    // waiting after each wave until the connection has processed it.
    bool send(quint16 port, int waveSize) {
        QUdpSocket out;
        if (!out.bind(QHostAddress::LocalHost, 0)) { return false; }
        int sent = 0;
        while (sent < m_count) {
            const int n = std::min(waveSize, m_count - sent);
            for (int i = 0; i < n; ++i) {
                if (out.writeDatagram(frame(sent + i), QHostAddress::LocalHost, port) != 1032) {
                    return false;
                }
            }
            sent += n;
            if (sent < m_count && !waitProcessed(sent, 5000)) { return false; }
        }
        return true;
    }

    bool waitProcessed(int frames, int timeoutMs) {
        QElapsedTimer t;
        t.start();
        while (m_framesSeen < frames && t.elapsed() < timeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        return m_framesSeen >= frames;
    }

    int framesSeen() const { return m_framesSeen; }

    // Empty when the flood came through whole; otherwise the first fault.
    QString verify() const {
        if (m_framesSeen != m_count) {
            return QString("%1 of %2 frames processed").arg(m_framesSeen).arg(m_count);
        }
        if (m_iq.size() != m_count * m_numRx) {
            return QString("%1 I/Q emissions for %2 frames of %3 receivers")
                .arg(m_iq.size()).arg(m_count).arg(m_numRx);
        }
        const int perFrame = 2 * samplesPerSubframe(m_numRx);
        for (int f = 0; f < m_count; ++f) {
            for (int r = 0; r < m_numRx; ++r) {
                const auto& [rx, v] = m_iq.at(f * m_numRx + r);
                if (rx != r) {
                    return QString("frame %1: emission for rx %2, expected rx %3").arg(f).arg(rx).arg(r);
                }
                if (v.size() != 2 * perFrame) {
                    return QString("frame %1 rx %2: %3 floats, expected %4")
                        .arg(f).arg(r).arg(v.size()).arg(2 * perFrame);
                }
                for (int s = 0; s < perFrame; ++s) {
                    const float ei = float(iValue(f, s, r)) / 8388608.0f;
                    const float eq = float(qValue(f, s, r)) / 8388608.0f;
                    if (v[2 * s] != ei || v[2 * s + 1] != eq) {
                        return QString("frame %1 rx %2 sample %3: got (%4, %5), sent (%6, %7)")
                            .arg(f).arg(r).arg(s).arg(v[2 * s]).arg(v[2 * s + 1]).arg(ei).arg(eq);
                    }
                }
            }
        }
        return {};
    }

private:
    P1RadioConnection& m_conn;
    int m_count;
    quint32 m_firstSeq;
    int m_numRx;
    QObject m_ctx;
    int m_framesSeen{0};
    QList<QPair<int, QVector<float>>> m_iq;
};

class TestP1LoopbackConnection : public QObject {
    Q_OBJECT

private:
    // Every address: dual-stack Any, or IPv4 Any when Qt keeps the IPv4
    // socket a failed bind to an IPv4 address left behind.
    static bool listensEverywhere(const QHostAddress& a) {
        return a == QHostAddress(QHostAddress::Any)
            || a == QHostAddress(QHostAddress::AnyIPv4);
    }

    // The receive buffer the kernel grants a socket asking what
    // P1RadioConnection asks (0x400000); the kernel may cap it.
    static int expectedReceiveBuffer() {
        QUdpSocket probe;
        if (!probe.bind(QHostAddress::Any, 0)) { return -2; }
        probe.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                              QVariant(0x400000));
        return probe.socketOption(QAbstractSocket::ReceiveBufferSizeSocketOption).toInt();
    }

    RadioInfo makeInfo(P1FakeRadio& fake) const {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        return info;
    }

private slots:
    // Test 1: Full RX path end-to-end
    void rxPathEndToEnd() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();

        QSignalSpy stateSpy(&conn, &RadioConnection::connectionStateChanged);
        QSignalSpy dataSpy(&conn,  &RadioConnection::iqDataReceived);

        conn.connectToRadio(makeInfo(fake));

        // Wait for Connected state (timeout 3s)
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        // Wait for the fake to process the metis-start datagram.
        // Both sockets are in the same thread — event loop must tick for the
        // fake's readyRead to fire after connectToRadio() sends the start packet.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Fake sends 10 ep6 frames
        fake.sendEp6Frames(10);

        // Wait for first iqDataReceived emission
        QTRY_VERIFY_WITH_TIMEOUT(dataSpy.count() >= 1, 3000);

        // Verify sample format — hwReceiverIndex=0, interleaved I/Q floats.
        // Use first() rather than last(): the fake streams 10 ep6 frames and
        // dataSpy may have captured additional emissions between QTRY_VERIFY
        // returning and this line running (e.g. rx=1 from a subsequent frame
        // if the connection ever fans out to multiple DDCs). The invariant
        // the test checks is "the first iqDataReceived emission is rx=0 with
        // the I=0.5 / Q=0 fake payload," which first() encodes unambiguously.
        auto args    = dataSpy.first();
        int  rxIdx   = args.at(0).toInt();
        QCOMPARE(rxIdx, 0);

        auto samples = args.at(1).value<QVector<float>>();
        QVERIFY(samples.size() > 0);
        QVERIFY((samples.size() % 2) == 0);  // interleaved I/Q pairs

        // The fake emits I=0.5, Q=0.0. Verify at least one I sample is ~0.5.
        // samples[0] = I0, samples[1] = Q0
        QVERIFY(qAbs(samples[0] - 0.5f) < 0.001f);
        QVERIFY(qAbs(samples[1] - 0.0f) < 0.001f);

        conn.disconnect();
        fake.stop();
    }

    // R-R3-21: Diagnostics > Connection Quality's "EP6 sequence gaps".
    // Each ep6 frame whose sequence number is not one past the last counts
    // one error, as P1RadioConnection::onReadyRead documents.
    void ep6SequenceGapsAreCounted() {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        HermesLiteBandwidthMonitor bw;
        P1RadioConnection conn;
        conn.init();
        conn.setBandwidthMonitor(&bw);
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        fake.sendEp6Frames(5);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 5, 3000);
        const int before = bw.ep6SequenceErrorCount();

        // In-order frames add nothing.
        fake.sendEp6Frames(3);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 8, 3000);
        QCOMPARE(bw.ep6SequenceErrorCount(), before);

        // A jump in the sequence counts one gap.
        fake.skipEp6Sequence(4);
        fake.sendEp6Frames(2);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 10, 3000);
        QCOMPARE(bw.ep6SequenceErrorCount(), before + 1);

        conn.disconnect();
        fake.stop();
    }

    // (Removed 2026-04-13) firmwareBelowMinimumRefusesConnect — the firmware
    // refusal path was removed from P1RadioConnection because Thetis enforces
    // no equivalent floor for the boards the previous test covered. See
    // BoardCapabilities.cpp file-header comment for the audit details.

    // Test 3: disconnect() stops the radio stream
    void disconnectStopsData() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();

        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        // Wait for the fake to register the metis-start before issuing
        // metis-stop — otherwise the stop can race ahead of the start in
        // the fake's event loop and m_running ends up true.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        conn.disconnect();
        QCOMPARE(conn.state(), ConnectionState::Disconnected);

        // The fake should have received a metis-stop and cleared m_running.
        // Use QTRY_VERIFY because the stop is processed asynchronously by
        // the fake's readyRead slot one event-loop turn after disconnect().
        QTRY_VERIFY_WITH_TIMEOUT(!fake.isRunning(), 1000);

        fake.stop();
    }

    // tst_slice_cap_every_path once lost a reconnect: the fake streamed, and
    // P1RadioConnection's 2 s connect watchdog still saw no ep6. The
    // connection bound its socket to every address (Any) on a port the OS
    // chose. On macOS the OS can hand such a socket a port that another
    // socket already holds on one address (127.0.0.1, which every test fake
    // and some of the app's own helpers bind), and a datagram to that
    // address and port goes to the socket bound to the address, not to the
    // connection. Another program took the radio's frames. The connection
    // now binds the address itself, as Thetis does (the cites are at
    // P1RadioConnection::bindToRadioFacingAddress), where no other socket
    // can share the port.
    //
    // The OS will not hand the connection a chosen port, so this test binds
    // the other socket second, the same sharing in the other order: asked
    // with ShareAddress (SO_REUSEADDR), macOS lets it share a port bound to
    // Any, and refuses it a port already bound to the same address.
    // The collision reproduces only on macOS: a pass on Linux or Windows
    // does not cover the fix.
    void anotherSocketOnTheRadiosAddressCannotTakeItsFrames() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.clientPort() != 0, 3000);
        const quint16 hostPort = fake.clientPort();

        QUdpSocket other;
        const bool otherBound =
            other.bind(fake.localAddress(), hostPort, QAbstractSocket::ShareAddress);

        frames.clear();
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 5, 2000);
        QVERIFY2(!otherBound || !other.hasPendingDatagrams(),
                 "the radio's frames went to another socket");

        conn.disconnect();
        fake.stop();
    }

    // With no route to the radio, connectToRadio warns and listens on every
    // address. disconnect() closes the socket, so on a reconnect the
    // fallback must bind it again, with the buffer sizes: before, it left
    // the socket closed and Qt bound it on the first send without them.
    void noRouteOnAReconnectBindsEveryAddressWithTheBuffers() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        conn.disconnect();
        QTRY_VERIFY_WITH_TIMEOUT(!fake.isRunning(), 1000);

        conn.setRadioFacingAddressForTest(QHostAddress());
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^P1: no local address reaches")));
        conn.connectToRadio(makeInfo(fake));

        QCOMPARE(conn.socketStateForTest(), QAbstractSocket::BoundState);
        QVERIFY(listensEverywhere(conn.socketAddressForTest()));
        QCOMPARE(conn.socketReceiveBufferForTest(), expectedReceiveBuffer());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        conn.disconnect();
        fake.stop();
    }

    // An address this host cannot bind (TEST-NET-1, RFC 5737) takes the
    // same fallback, with its own warning.
    void anUnbindableAddressBindsEveryAddressWithTheBuffers() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.setRadioFacingAddressForTest(QHostAddress(QStringLiteral("192.0.2.1")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^P1: could not listen on")));
        conn.connectToRadio(makeInfo(fake));

        QCOMPARE(conn.socketStateForTest(), QAbstractSocket::BoundState);
        QVERIFY(listensEverywhere(conn.socketAddressForTest()));
        QCOMPARE(conn.socketReceiveBufferForTest(), expectedReceiveBuffer());
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        conn.disconnect();
        fake.stop();
    }

    // Issue #258 regression — when a burst of N 1032-byte ep6 datagrams
    // lands in a single readyRead batch while state is still Connecting,
    // the "P1: Connected, ep6 stream established" log line must fire
    // EXACTLY ONCE, not once per datagram.  Field repro: first connect
    // emitted 181 spurious lines in <10 ms because cs was a const local
    // snapshot read outside the drain loop.
    void firstConnectLogFiresExactlyOncePerBatch() {
        P1FakeRadio fake;
        // Disable the 10 ms auto-stream timer so we have exclusive control
        // over when ep6 frames arrive.  Otherwise the first auto-stream
        // tick would promote state to Connected before our burst lands,
        // masking the bug (state==Connected -> log branch not taken).
        fake.setAutoStreamEnabled(false);
        fake.start();

        // Install the counting message handler AFTER fake.start() so the
        // fake's own diagnostics aren't counted (they don't match the
        // substring, but belt-and-braces).
        g_ep6EstablishedLogCount.store(0);
        g_previousMsgHandler = qInstallMessageHandler(countEp6EstablishedLogs);

        P1RadioConnection conn;
        conn.init();

        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        conn.connectToRadio(info);

        // Wait for the fake to receive metis-start (m_running flips true)
        // — that means conn is now in Connecting waiting for first ep6.
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        QCOMPARE(conn.state(), ConnectionState::Connecting);

        // Burst-send 100 ep6 frames in a tight loop.  All 100 land in
        // the OS UDP receive queue before conn's event loop ticks; when
        // onReadyRead fires it drains all 100 in one while-loop pass.
        fake.sendEp6Frames(100);

        // Pump the event loop until the conn transitions to Connected
        // (i.e. at least the first of the 100 frames has been drained
        // through onReadyRead).
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        // Bug A: pre-fix this was 100 (once per datagram in the batch).
        // Post-fix: 1.
        QCOMPARE(g_ep6EstablishedLogCount.load(), 1);

        qInstallMessageHandler(g_previousMsgHandler);
        g_previousMsgHandler = nullptr;

        conn.disconnect();
        fake.stop();
    }

    // Verifies EP2 (host→radio) cadence matches the spec's 48 kHz / 126
    // samples-per-packet rate of ~381 pps. Before the pacer fix the rate
    // was ~40 pps (once per 25 ms watchdog tick), starving the radio's
    // audio DAC. See issue #38 pcap analysis.
    void ep2PaceRateMatchesAudioClock() {
        P1FakeRadio fake;
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.connectToRadio(makeInfo(fake));
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        // Baseline count after connection is up — discard discovery/start framing.
        const int baseline = fake.ep2FramesReceived();

        // Sample for 500 ms.
        QTest::qWait(500);

        const int delta = fake.ep2FramesReceived() - baseline;
        // At 380.95 pps ideal we expect ~190 packets in 500 ms. Windows QTimer
        // jitter + catch-up loop settle the observed rate around 300-400 pps,
        // so assert a floor of 100 packets (200 pps) which is still 5x the
        // broken 40 pps watchdog cadence.
        QVERIFY2(delta >= 100,
            qPrintable(QString("EP2 rate too slow: %1 packets in 500 ms (=%2 pps)")
                .arg(delta).arg(delta * 2)));

        conn.disconnect();
        fake.stop();
    }

    // 2026-10-14, Pi 4 Core, HL2 keyed: the connection thread read EP6
    // slower than the HL2 sent it, onReadyRead's loop never emptied the
    // socket, and the EP2 pacer on the same thread sent nothing for 12.9 s.
    // One call now reads at most kMaxEp6DatagramsPerRead datagrams and
    // leaves the rest, which are read without any new datagram arriving.
    void readLoopIsBoundedPerCall() {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 1, 3000);
        frames.clear();

        // Four batches' worth, queued on the socket before this thread's
        // event loop runs again (sendEp6Frames returns once they are
        // written; loopback delivers them at once).
        const int cap = P1RadioConnection::maxEp6DatagramsPerReadForTest();
        const int sent = cap * 4;
        fake.sendEp6Frames(sent);

        conn.onReadyReadForTest();
        QCOMPARE(frames.count(), cap);
        QVERIFY2(conn.socketHasPendingDatagramsForTest(),
                 "one onReadyRead call read every pending datagram");

        // No new arrivals: the read notifier brings the rest in.
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), sent, 3000);
        QVERIFY(!conn.socketHasPendingDatagramsForTest());

        conn.disconnect();
        fake.stop();
    }

    // A flood well past the bound: 640 datagrams (40 batches) in one burst,
    // which a 4 MB receive buffer holds without the kernel dropping any.
    // One onReadyRead call leaves datagrams pending, and the receive path is
    // otherwise unchanged: every frame processed, in sequence order, with the
    // I/Q delivered sample for sample as sent.
    void ep6FloodIsReceivedWhole() {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        HermesLiteBandwidthMonitor bw;
        P1RadioConnection conn;
        conn.init();
        conn.setBandwidthMonitor(&bw);

        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        // The fake's one frame carries sequence 0 and connects.
        QSignalSpy first(&conn, &RadioConnection::frameReceived);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(first.count(), 1, 3000);
        const int gapsBefore = bw.ep6SequenceErrorCount();

        constexpr int kFlood = 640;
        // Room for the whole burst with room to spare for kernel overhead.
        QVERIFY2(conn.socketReceiveBufferForTest() >= 2 * kFlood * 1032,
                 qPrintable(QString("receive buffer %1 too small for one burst")
                                .arg(conn.socketReceiveBufferForTest())));

        Ep6Flood flood(conn, kFlood, 1, conn.activeRxCountForTest());
        QVERIFY(flood.send(fake.clientPort(), kFlood));

        // Bound: one call reads one batch and leaves the rest pending.
        conn.onReadyReadForTest();
        QCOMPARE(flood.framesSeen(), P1RadioConnection::maxEp6DatagramsPerReadForTest());
        QVERIFY(conn.socketHasPendingDatagramsForTest());

        // Receive unchanged.
        QVERIFY(flood.waitProcessed(kFlood, 5000));
        const QString fault = flood.verify();
        QVERIFY2(fault.isEmpty(), qPrintable(fault));
        QCOMPARE(bw.ep6SequenceErrorCount(), gapsBefore);
        QVERIFY(!conn.socketHasPendingDatagramsForTest());

        conn.disconnect();
        fake.stop();
    }

    // The stall itself: EP6 arrives faster than this thread reads it (each
    // frame costs 200 us here, about 5,000 a second, while a feeder sends
    // about twice that), and the EP2 pacer must still send while it lasts.
    // Before the bound, onReadyRead never returned and almost no EP2 frame
    // reached the radio until the feeder stopped.
    void ep2PacerSendsWhileEp6OutrunsTheReader() {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);

        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(frames.count(), 1, 3000);

        // A CPU-short reader: every frame read costs 200 us on this thread.
        QObject slowReader;
        connect(&conn, &RadioConnection::frameReceived, &slowReader, [] {
            QElapsedTimer t;
            t.start();
            while (t.nsecsElapsed() < 200000) {
            }
        });

        constexpr int kFeedMs = 500;
        std::atomic<int> ep2AtFeedStart{0};
        std::atomic<int> ep2AtFeedEnd{0};
        std::thread feeder([&fake, &ep2AtFeedStart, &ep2AtFeedEnd] {
            QElapsedTimer t;
            t.start();
            // Let the reader fall behind before counting.
            while (t.elapsed() < 100) {
                fake.sendEp6Frames(20);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            ep2AtFeedStart.store(fake.ep2FramesReceived());
            while (t.elapsed() < 100 + kFeedMs) {
                fake.sendEp6Frames(20);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            ep2AtFeedEnd.store(fake.ep2FramesReceived());
        });

        // Run this thread's event loop while the feeder runs.
        QTest::qWait(100 + kFeedMs + 50);
        feeder.join();
        QObject::disconnect(&conn, &RadioConnection::frameReceived, &slowReader, nullptr);

        // 381 frames a second would be about 190 in 500 ms; ask for a fifth.
        const int ep2During = ep2AtFeedEnd.load() - ep2AtFeedStart.load();
        QVERIFY2(ep2During >= 40,
                 qPrintable(QString("only %1 EP2 frames reached the radio in %2 ms "
                                    "while EP6 outran the reader")
                                .arg(ep2During).arg(kFeedMs)));

        // The backlog left when the feeder stops is read with no new arrivals.
        QTRY_VERIFY_WITH_TIMEOUT(!conn.socketHasPendingDatagramsForTest(), 5000);

        conn.disconnect();
        fake.stop();
    }
};

QTEST_MAIN(TestP1LoopbackConnection)
#include "tst_p1_loopback_connection.moc"
