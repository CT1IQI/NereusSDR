// no-port-check: NereusSDR-original test helper.
//
// UnusedLoopbackPort: a port for a test to hand to something that listens
// on it later (a station server, a CAT or TCI listener, a Core started as a
// process), or to dial expecting nothing to answer.
//
// The pattern it replaces, listen on port 0, read the port and close, takes
// the port from the kernel's ephemeral range (Linux 32768 to 60999, macOS
// and Windows 49152 to 65535). Once closed, the kernel hands that same
// range out to every other process's port-0 listener and outgoing
// connection, so on a machine running tests side by side another test can
// hold the port by the time this one listens: "The bound address is
// already in use" on 127.0.0.1:37915 in tst_desktop_station_window on the
// Linux self-hosted CI (PR 611). This helper picks from 20000 to 31999,
// below every ephemeral range, which the kernel never hands out for port 0
// or an outgoing connection, and no product default uses (CAT 13013,
// rigctld 4532, the station 47910 and 47911, TCI 50001). It checks that
// nothing listens there now on TCP or UDP, choosing at random so that tests
// picking at the same moment seldom choose alike.
//
// 2026-10-22, J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once

#include <QHostAddress>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QUdpSocket>

namespace NereusSDR {

inline constexpr quint16 kUnusedLoopbackPortFirst = 20000;
inline constexpr quint16 kUnusedLoopbackPortEnd = 32000;  // exclusive
inline constexpr int kUnusedLoopbackPortAttempts = 200;

// A port nothing listens on at `address` now, other than `except`; 0 when
// none was found.
inline quint16 unusedLoopbackPort(const QHostAddress& address = QHostAddress::LocalHost,
                                  quint16 except = 0)
{
    for (int attempt = 0; attempt < kUnusedLoopbackPortAttempts; ++attempt) {
        const auto port = static_cast<quint16>(QRandomGenerator::global()->bounded(
            int(kUnusedLoopbackPortFirst), int(kUnusedLoopbackPortEnd)));
        if (port == except) {
            continue;
        }
        QTcpServer tcp;
        if (!tcp.listen(address, port)) {
            continue;
        }
        QUdpSocket udp;
        if (!udp.bind(address, port, QAbstractSocket::DontShareAddress)) {
            continue;
        }
        return port;
    }
    return 0;
}

} // namespace NereusSDR
