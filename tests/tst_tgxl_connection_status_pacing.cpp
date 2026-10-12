// =================================================================
// tests/tst_tgxl_connection_status_pacing.cpp  (NereusSDR)
// =================================================================
// NereusSDR-native test. One `status` in flight on the Tuner Genius XL
// link: the poll does not ask again while the tuner has not answered
// the one before (GeniusStatusGate.h). The tuner's answer shape,
// `S<seq>|status ...`, is from captures/flex-tgxl-direct-NOTES.md:73.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12  Created by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include "core/AppSettings.h"
#include "core/TgxlConnection.h"

namespace {

// A tuner on loopback that answers only when the test tells it to. It
// records every `status` it is sent.
class FakeTuner : public QObject {
public:
    FakeTuner() { QVERIFY(m_server.listen(QHostAddress::LocalHost, 0)); }
    quint16 port() const { return m_server.serverPort(); }
    // Accepts the next connection and sends the version banner.
    bool greet()
    {
        if (!QTest::qWaitFor([this] { return m_server.hasPendingConnections(); }, 3000)) {
            return false;
        }
        m_peer = m_server.nextPendingConnection();
        m_buffer.clear();
        connect(m_peer, &QTcpSocket::readyRead, this, [this] {
            m_buffer += m_peer->readAll();
            int end = 0;
            while ((end = m_buffer.indexOf('\n')) >= 0) {
                const QString line = QString::fromUtf8(m_buffer.left(end)).trimmed();
                m_buffer.remove(0, end + 1);
                if (line.endsWith(QStringLiteral("|status"))) {
                    m_statusSeqs.push_back(line.mid(1, line.indexOf('|') - 1).toUInt());
                }
            }
        });
        m_peer->write("V1.2.17\n");
        m_peer->flush();
        return true;
    }
    int statusCount() const { return int(m_statusSeqs.size()); }
    quint32 newestStatusSeq() const { return m_statusSeqs.last(); }
    // The tuner's answer to the newest `status`.
    void answerStatus() { sendStatusLine(m_statusSeqs.last()); }
    // A status line carrying this sequence number.
    void sendStatusLine(quint32 seq)
    {
        send(QStringLiteral("S%1|status fwd=0.00 peak=0.00 max=0.00 swr=-60.0000 pttA=0 "
                            "state=1 active=1 tuning=0 bypass=0 relayC1=0 relayL=0 relayC2=0")
                 .arg(seq));
    }
    // The tuner refusing the newest `status`.
    void refuseStatus() { send(QStringLiteral("R%1|50000015|").arg(m_statusSeqs.last())); }
    void send(const QString& line)
    {
        m_peer->write(line.toUtf8() + '\n');
        m_peer->flush();
    }
    void hangUp() { m_peer->close(); }

private:
    QTcpServer m_server;
    QPointer<QTcpSocket> m_peer;
    QByteArray m_buffer;
    QList<quint32> m_statusSeqs;
};

// The 1 s poll timer's tick, without the wait.
void poll(NereusSDR::TgxlConnection& conn)
{
    QVERIFY(QMetaObject::invokeMethod(&conn, "pollStatus", Qt::DirectConnection));
}

// Long enough for a command written on loopback to reach the fake tuner.
constexpr int kArriveMs = 80;

// Longer than any case runs. With it, only the thing a case names can let
// the next `status` go: not the wait running out, and not the link's own
// 1 s poll timer, which is live throughout.
constexpr int kNeverMs = 60000;

} // namespace

class TgxlConnectionStatusPacingTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void thePollWaitsForTheAnswerToTheOneBefore();
    void aRefusalAlsoLetsTheNextPollGo();
    void anAnswerToSomethingElseDoesNotLetItGo();
    void aStatusLineWithAnotherNumberDoesNotLetItGo();
    void aLostAnswerDoesNotStopThePoll();
    void aNewConnectionAsksAtOnce();
};

void TgxlConnectionStatusPacingTest::init()
{
    NereusSDR::AppSettings::instance().setValue(QStringLiteral("TGXL_AutoReconnect"),
                                                QStringLiteral("True"));
}

void TgxlConnectionStatusPacingTest::thePollWaitsForTheAnswerToTheOneBefore()
{
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    QSignalSpy answers(&conn, &NereusSDR::TgxlConnection::statusUpdated);
    conn.testSetStatusReplyWaitMs(kNeverMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);   // the handshake's

    // A slow tuner: the poll does not ask again on top of it.
    poll(conn);
    poll(conn);
    // Nor does the keepalive.
    QVERIFY(QMetaObject::invokeMethod(&conn, "onKeepaliveTimeout", Qt::DirectConnection));
    QTest::qWait(kArriveMs);
    QCOMPARE(tuner.statusCount(), 1);

    // Answered: the next poll asks, once.
    tuner.answerStatus();
    QTRY_COMPARE(answers.count(), 1);
    poll(conn);
    poll(conn);
    QTest::qWait(kArriveMs);
    QCOMPARE(tuner.statusCount(), 2);

    // And the meters keep moving once it answers.
    tuner.answerStatus();
    QTRY_COMPARE(answers.count(), 2);
    poll(conn);
    QTRY_COMPARE(tuner.statusCount(), 3);
}

void TgxlConnectionStatusPacingTest::aRefusalAlsoLetsTheNextPollGo()
{
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    QSignalSpy replies(&conn, &NereusSDR::TgxlConnection::replyReceived);
    conn.testSetStatusReplyWaitMs(kNeverMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);

    tuner.refuseStatus();
    QTRY_COMPARE(replies.count(), 1);
    poll(conn);
    QTRY_COMPARE(tuner.statusCount(), 2);
}

void TgxlConnectionStatusPacingTest::anAnswerToSomethingElseDoesNotLetItGo()
{
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    QSignalSpy replies(&conn, &NereusSDR::TgxlConnection::replyReceived);
    QSignalSpy states(&conn, &NereusSDR::TgxlConnection::stateUpdated);
    conn.testSetStatusReplyWaitMs(kNeverMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);   // sequence 2; `info` was 1

    // The answer to `info`, and a state push the tuner sends unasked.
    tuner.send(QStringLiteral("R1|0|info serial=1 version=1.2.17 nickname=Tuner"));
    tuner.send(QStringLiteral("S0|state bypassA=0 state=1 tuning=0 relayC1=16 relayL=16 relayC2=0"));
    QTRY_COMPARE(replies.count(), 1);
    QTRY_COMPARE(states.count(), 1);
    poll(conn);
    QTest::qWait(kArriveMs);
    QCOMPARE(tuner.statusCount(), 1);
}

void TgxlConnectionStatusPacingTest::aStatusLineWithAnotherNumberDoesNotLetItGo()
{
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    QSignalSpy answers(&conn, &NereusSDR::TgxlConnection::statusUpdated);
    conn.testSetStatusReplyWaitMs(kNeverMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);

    // A status line that is not the answer to the one in flight: read for
    // its values, but the poll still waits for its own.
    tuner.sendStatusLine(tuner.newestStatusSeq() + 40);
    QTRY_COMPARE(answers.count(), 1);
    poll(conn);
    QTest::qWait(kArriveMs);
    QCOMPARE(tuner.statusCount(), 1);

    tuner.answerStatus();
    QTRY_COMPARE(answers.count(), 2);
    poll(conn);
    QTRY_COMPARE(tuner.statusCount(), 2);
}

void TgxlConnectionStatusPacingTest::aLostAnswerDoesNotStopThePoll()
{
    constexpr int kWaitMs = 300;
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    conn.testSetStatusReplyWaitMs(kWaitMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);

    // Never answered: held back inside the wait, sent after it.
    poll(conn);
    QTest::qWait(kArriveMs);
    QCOMPARE(tuner.statusCount(), 1);
    QTest::qWait(kWaitMs);
    poll(conn);
    QTRY_COMPARE(tuner.statusCount(), 2);
}

void TgxlConnectionStatusPacingTest::aNewConnectionAsksAtOnce()
{
    FakeTuner tuner;
    NereusSDR::TgxlConnection conn;
    QSignalSpy answers(&conn, &NereusSDR::TgxlConnection::statusUpdated);
    conn.testSetReconnectBackoffUnitMs(20);
    conn.testSetStatusReplyWaitMs(kNeverMs);
    conn.connectToTgxl(QStringLiteral("127.0.0.1"), tuner.port());
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 1);
    // The tuner drops the link with that `status` unanswered.
    tuner.hangUp();
    QVERIFY(tuner.greet());
    QTRY_COMPARE(tuner.statusCount(), 2);   // the new handshake's
    tuner.answerStatus();
    QTRY_COMPARE(answers.count(), 1);
    poll(conn);
    QTRY_COMPARE(tuner.statusCount(), 3);
}

QTEST_GUILESS_MAIN(TgxlConnectionStatusPacingTest)
#include "tst_tgxl_connection_status_pacing.moc"
