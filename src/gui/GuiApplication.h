#pragma once
// no-port-check: NereusSDR-original application shutdown boundary.
// Modification history (NereusSDR):
//   2026-10-04 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-22 — J.J. Boyd (KG4VCF), AI-assisted via Claude Code: a
//                 main-thread stall trace (slow event deliveries and a
//                 lateness heartbeat) under NEREUS_TRACE_MAIN_STALLS=1;
//                 diagnostics only.

#include <QApplication>
#include <QElapsedTimer>

class QTimer;

namespace NereusSDR {

class GuiApplication : public QApplication
{
    Q_OBJECT

public:
    GuiApplication(int& argc, char** argv);
    static bool applicationQuitInProgress();

    // With NEREUS_TRACE_MAIN_STALLS=1 (read once at construction), times
    // each event delivery on the GUI thread and logs a MAIN_STALL line for
    // one of 12 ms or more. Off, it is a straight call to QApplication.
    bool notify(QObject* receiver, QEvent* event) override;

protected:
    bool event(QEvent* event) override;

private:
    bool notifyTraced(QObject* receiver, QEvent* event);
    void onStallHeartbeat();

    bool m_traceMainStalls = false;
    int m_stallDepth = 0;
    QTimer* m_stallHeartbeat = nullptr;
    QElapsedTimer m_stallHeartbeatClock;
    bool m_quitEventInProgress = false;
    bool m_terminalShutdown = false;
};

} // namespace NereusSDR
