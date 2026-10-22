// =================================================================
// tests/tst_discovery_connect_hold.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original: no Thetis or AetherSDR upstream.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Review of PR 600, Important 2: a discovery walk running
//                 when a local connection starts sends nothing until that
//                 connection is Connected (or has failed) and the
//                 post-disconnect quiet period has passed, then finishes
//                 every NIC. Driven through RadioModel::connectToRadio()
//                 against P1FakeRadio, in the startup order (the walk, then
//                 the connect on the next turn). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QFile>
#include <QObject>
#include <QSignalSpy>
#include <QTimer>
#include <QUdpSocket>
#include <QtTest>

#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "fakes/FakeAudioBus.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {

// Counts the walk's P1 probes on a loopback port and never answers. The
// walk is pointed at it with setScanNicsForTest(), so nothing leaves the
// machine.
class ProbeCounter : public QObject {
    Q_OBJECT
public:
    explicit ProbeCounter(QObject* parent = nullptr) : QObject(parent)
    {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, [this]() {
            while (m_socket->hasPendingDatagrams()) {
                QByteArray buf;
                buf.resize(int(m_socket->pendingDatagramSize()));
                m_socket->readDatagram(buf.data(), buf.size());
                if (buf.size() >= 3 && static_cast<quint8>(buf[0]) == 0xEF
                    && static_cast<quint8>(buf[1]) == 0xFE
                    && static_cast<quint8>(buf[2]) == 0x02) {
                    ++m_p1Probes;
                }
            }
        });
    }
    int p1Probes() const { return m_p1Probes; }

    // Two "NICs", both this loopback port, no general broadcast.
    QList<RadioDiscovery::ScanNic> nics() const
    {
        RadioDiscovery::ScanNic nic;
        nic.name = QStringLiteral("loopback-test");
        nic.local = QHostAddress::LocalHost;
        nic.directed = QHostAddress::LocalHost;
        nic.generalBroadcast = false;
        nic.port = m_socket->localPort();
        return {nic, nic};
    }

private:
    QUdpSocket* m_socket {nullptr};
    int m_p1Probes {0};
};

void installOpenAudioBuses(AudioEngine& engine)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;
    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("Fake speakers"));
    auto txInput = std::make_unique<FakeAudioBus>(QStringLiteral("Fake TX input"));
    speakers->open(format);
    txInput->open(format);
    engine.setSpeakersBusForTest(std::move(speakers));
    engine.setTxInputBusForTest(std::move(txInput));
}

RadioInfo infoFor(const P1FakeRadio& fake)
{
    RadioInfo info;
    info.address = fake.localAddress();
    info.port = fake.localPort();
    info.boardType = HPSDRHW::HermesLite;
    info.protocol = ProtocolVersion::Protocol1;
    info.macAddress = QStringLiteral("aa:bb:cc:11:22:33");
    info.firmwareVersion = 72;
    info.name = QStringLiteral("connect hold fake");
    return info;
}

// What the walk and the hold looked like when the model first left
// Disconnected (the connection exists and is starting) and when it next
// reached `end`. Attempts are counted where the walk sends them
// (scanStatsForTest), so a probe still in flight cannot blur the count.
struct ConnectRecord {
    int  attemptsAtStart {-1};
    bool holdAtStart {false};
    bool walkRunningAtStart {false};
    int  attemptsAtEnd {-1};
    bool ended {false};
};

void watchConnect(RadioModel& model, const RadioDiscovery& disc, ConnectionState end,
                  ConnectRecord& rec, QObject* context)
{
    QObject::connect(&model, &RadioModel::connectionStateChanged, context,
                     [&disc, end, &rec](ConnectionState s) {
        if (rec.attemptsAtStart < 0 && s != ConnectionState::Disconnected) {
            rec.attemptsAtStart = disc.scanStatsForTest().attempts;
            rec.holdAtStart = RadioDiscovery::connectHoldActive();
            rec.walkRunningAtStart = disc.isScanRunning();
        }
        if (rec.attemptsAtStart >= 0 && !rec.ended && s == end) {
            rec.attemptsAtEnd = disc.scanStatsForTest().attempts;
            rec.ended = true;
        }
    });
}

} // namespace

class TstDiscoveryConnectHold : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("discovery-connect-hold-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init() { RadioDiscovery::clearHoldOffForTest(); }

    void cleanupTestCase()
    {
        RadioDiscovery::clearHoldOffForTest();
        QFile::remove(AppSettings::instance().filePath());
    }

    // The startup order (MainWindow::startInitialConnection): the walk
    // starts, and the auto-connect runs on the next turn while the walk
    // still has attempts and NICs to go. From the moment the connection
    // exists until it is Connected, and then for the quiet period, the walk
    // sends nothing; then it resumes the same walk and finishes every NIC.
    void aWalkHoldsItsSendsWhileAConnectStartsThenFinishes()
    {
        ProbeCounter radioSide;
        RadioDiscovery disc;
        disc.setScanNicsForTest(radioSide.nics());
        disc.setTimingForTest({2, 3, 100});   // 4 attempts, at least 1.2 s
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        P1FakeRadio fake;
        fake.start();
        RadioModel model(RadioModel::Role::Local);
        model.audioEngine()->setStartInitializerForTest(installOpenAudioBuses);
        model.wdspEngine()->setSynchronousInitForTest(true);
        ConnectRecord rec;
        watchConnect(model, disc, ConnectionState::Connected, rec, this);

        disc.startDiscovery();
        const RadioInfo info = infoFor(fake);
        QTimer::singleShot(0, &model, [&model, info]() { model.connectToRadio(info); });

        QTRY_VERIFY_WITH_TIMEOUT(model.connectionState() == ConnectionState::Connected, 15000);
        QVERIFY(rec.ended);
        QVERIFY2(rec.walkRunningAtStart, "the walk had ended before the connect; nothing tested");
        QVERIFY(rec.attemptsAtStart >= 1);
        QVERIFY2(rec.holdAtStart, "discovery was not held while the connection started");
        QCOMPARE(rec.attemptsAtEnd, rec.attemptsAtStart);
        QVERIFY(!RadioDiscovery::connectHoldActive());
        QVERIFY2(disc.holdOffRemainingMs() > 0, "no quiet period after Connected");

        // The quiet period only falls: an attempt counted here was sent
        // after a read that saw time left, which is the case asserted
        // against (as in tst_radio_discovery_probe).
        while (disc.holdOffRemainingMs() > 0) {
            QCOMPARE(disc.scanStatsForTest().attempts, rec.attemptsAtStart);
            QCOMPARE(finished.count(), 0);
            QTest::qWait(10);
        }

        QVERIFY(finished.wait(5000));
        QTest::qWait(200);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(disc.scanStatsForTest().attempts, 4);
        QTRY_COMPARE_WITH_TIMEOUT(radioSide.p1Probes(), 4, 2000);
    }

    // A connect that fails (the radio never answers; the P1 connect
    // watchdog reports Disconnected) releases the hold, the quiet period
    // follows, and the walk then finishes.
    void aFailedConnectReleasesTheHold()
    {
        ProbeCounter radioSide;
        RadioDiscovery disc;
        disc.setScanNicsForTest(radioSide.nics());
        disc.setTimingForTest({2, 3, 100});
        QSignalSpy finished(&disc, &RadioDiscovery::discoveryFinished);

        P1FakeRadio fake;
        fake.start();
        fake.goSilent();
        RadioModel model(RadioModel::Role::Local);
        model.audioEngine()->setStartInitializerForTest(installOpenAudioBuses);
        model.wdspEngine()->setSynchronousInitForTest(true);
        ConnectRecord rec;
        watchConnect(model, disc, ConnectionState::Disconnected, rec, this);

        disc.startDiscovery();
        const RadioInfo info = infoFor(fake);
        QTimer::singleShot(0, &model, [&model, info]() { model.connectToRadio(info); });

        QTRY_VERIFY_WITH_TIMEOUT(rec.ended, 15000);
        QVERIFY2(rec.walkRunningAtStart, "the walk had ended before the connect; nothing tested");
        QVERIFY2(rec.holdAtStart, "discovery was not held while the connection started");
        QCOMPARE(rec.attemptsAtEnd, rec.attemptsAtStart);
        QCOMPARE(finished.count(), 0);
        QVERIFY2(!RadioDiscovery::connectHoldActive(), "the failed connect left discovery held");
        QVERIFY(disc.holdOffRemainingMs() > 0);

        while (disc.holdOffRemainingMs() > 0) {
            QCOMPARE(disc.scanStatsForTest().attempts, rec.attemptsAtStart);
            QTest::qWait(10);
        }
        QVERIFY(finished.wait(5000));
        QCOMPARE(finished.count(), 1);
        QCOMPARE(disc.scanStatsForTest().attempts, 4);
        QVERIFY(!RadioDiscovery::connectHoldActive());
    }
};

QTEST_MAIN(TstDiscoveryConnectHold)
#include "tst_discovery_connect_hold.moc"
