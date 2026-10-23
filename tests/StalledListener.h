#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/StalledListener.h  (NereusSDR)
// =================================================================
//
// A silent path on this computer, for the Happy Eyeballs tests: an address
// whose connection attempts get no answer at all, so the connecting side
// waits in SYN_SENT, as it does towards an IPv6 address whose path is dead
// (2026-10-22: a Mac whose only global IPv6 address was deprecated).
// Loopback only; nothing leaves this computer. Two ways, the first that
// stalls is used:
//   - Linux: a listener on ::1 whose accept queue is full; the kernel drops
//     a new connection's SYN.
//   - macOS: 127.0.0.2. Only 127.0.0.1 is configured on lo0, but all of
//     127/8 routes to lo0, so a SYN to 127.0.0.2 is looped back and dropped
//     unanswered. (macOS answers a full accept queue with a reset or a
//     connection, never silence; Linux owns all of 127/8 and resets.)
// start() confirms the stall (a connection still opening after 300 ms) and
// returns the silent address; on a system with neither it returns a null
// address and says why, and the test skips.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QHostAddress>
#include <QNetworkProxy>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <memory>
#include <vector>

namespace NereusSDR::Test {

class StalledListener {
public:
    StalledListener() = default;
    StalledListener(const StalledListener&) = delete;
    StalledListener& operator=(const StalledListener&) = delete;

    /// An address that leaves a connection to `port` unanswered, or a null
    /// address (see whyNot).
    QHostAddress start(quint16 port)
    {
        const QHostAddress v6(QStringLiteral("::1"));
        if (fillQueue(v6, port)) {
            return v6;
        }
        const QString queueWhyNot = whyNot;
        const QHostAddress unconfigured(QStringLiteral("127.0.0.2"));
        if (stalls(unconfigured, port)) {
            whyNot.clear();
            return unconfigured;
        }
        whyNot = QStringLiteral("no silent loopback path: %1; and 127.0.0.2 answers").arg(queueWhyNot);
        return {};
    }

    QString whyNot;

private:
    static constexpr int kStallProbeMs = 300;

    bool stalls(const QHostAddress& address, quint16 port)
    {
        auto probe = std::make_unique<QTcpSocket>();
        probe->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        probe->connectToHost(address, port);
        QTest::qWait(kStallProbeMs);
        const bool stalled = probe->state() == QAbstractSocket::ConnectingState;
        m_fillers.push_back(std::move(probe));
        return stalled;
    }

    bool fillQueue(const QHostAddress& address, quint16 port)
    {
        m_server.setListenBacklogSize(1);
        if (!m_server.listen(address, port)) {
            whyNot = QStringLiteral("cannot listen on %1 port %2: %3")
                         .arg(address.toString())
                         .arg(port)
                         .arg(m_server.errorString());
            return false;
        }
        m_server.pauseAccepting();
        constexpr int kMaxFillers = 64;
        for (int i = 0; i < kMaxFillers; ++i) {
            auto filler = std::make_unique<QTcpSocket>();
            filler->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
            filler->connectToHost(address, port);
            QTest::qWait(kStallProbeMs);
            const QAbstractSocket::SocketState state = filler->state();
            m_fillers.push_back(std::move(filler));
            if (state == QAbstractSocket::ConnectingState) {
                return true;
            }
            if (state != QAbstractSocket::ConnectedState) {
                whyNot = QStringLiteral("this system refuses a connection to a full accept "
                                        "queue instead of leaving it unanswered");
                m_fillers.clear();
                m_server.close();
                return false;
            }
        }
        whyNot = QStringLiteral("the accept queue never filled");
        m_fillers.clear();
        m_server.close();
        return false;
    }

    QTcpServer m_server;
    std::vector<std::unique_ptr<QTcpSocket>> m_fillers;
};

} // namespace NereusSDR::Test
