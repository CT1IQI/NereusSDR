// =================================================================
// src/core/session/media/DaemonDisplaySendThread.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The thread that takes in a Core's
// spectrum frames and sends its displays, and the lock it shares with the
// Core's main thread; no Thetis logic.
//
// Why it exists (STRIX bench, 2026-10-15, the desktop app hosting the Core
// with Windows locked): the display sender was a 5 ms timer on the GUI main
// thread, which Direct3D held for up to half a second a frame.  The sender
// turned 2 to 4 times a second and 55 frames a second were replaced unsent,
// while nereusd beside it, with nothing drawing on its main thread, kept
// 6.7 to 9.8 ms.  This thread takes in and sends whatever the main thread
// does, as receive audio's own thread does (DaemonAudioSendThread).
// Plan: docs/architecture/2026-10-15-display-send-thread-plan.md.
//
// Modification history (NereusSDR):
//   2026-10-15: created.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include <QtGlobal>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace NereusSDR {

// The lock over every display controller of one DaemonSharedSpectrum (one
// per DaemonMediaHub, or one inside a stand-alone controller).  The main
// thread holds it whenever it runs controller code; the display thread holds
// it for each pass.  Recursive, since controller code reaches controller code
// again through signals.
//
// When the main thread lets go of its outermost hold, the release hook runs
// first, still under the lock: the controllers copy the main-thread values
// their display thread reads (DaemonMediaController::refreshDisplaySnapshot),
// so the display thread never runs on values older than the controller code
// that just ran.  The hook does not run for the display thread's own holds.
//
// No audio callback, DSP thread, FFT engine thread or transport library
// thread ever takes it.
class DaemonDisplayLock final {
public:
    using ReleaseHook = std::function<void()>;

    DaemonDisplayLock() = default;
    DaemonDisplayLock(const DaemonDisplayLock&) = delete;
    DaemonDisplayLock& operator=(const DaemonDisplayLock&) = delete;

    void lock();
    // The display thread's wait: false after `timeout` without the lock.
    bool tryLockFor(std::chrono::microseconds timeout);
    void unlock();
    bool heldByCurrentThread() const;

    // Under the lock.  `hookExempt` is the thread whose releases never run
    // the hook (the display thread); an empty id exempts none.
    void setReleaseHook(ReleaseHook hook);
    void setHookExemptThread(std::thread::id hookExempt);

private:
    std::recursive_timed_mutex m_mutex;
    std::atomic<std::thread::id> m_owner{};
    std::atomic<std::thread::id> m_hookExempt{};
    // Under m_mutex.
    int m_depth{0};
    bool m_inHook{false};
    ReleaseHook m_hook;
};

// Holds a DaemonDisplayLock for its scope, keeping the lock itself alive.
class DaemonDisplayLockGuard final {
public:
    explicit DaemonDisplayLockGuard(std::shared_ptr<DaemonDisplayLock> lock)
        : m_lock(std::move(lock))
    {
        if (m_lock) {
            m_lock->lock();
        }
    }
    ~DaemonDisplayLockGuard()
    {
        if (m_lock) {
            m_lock->unlock();
        }
    }
    DaemonDisplayLockGuard(const DaemonDisplayLockGuard&) = delete;
    DaemonDisplayLockGuard& operator=(const DaemonDisplayLockGuard&) = delete;

private:
    std::shared_ptr<DaemonDisplayLock> m_lock;
};

// One per DaemonSharedSpectrum, serving all of its controllers.
//
//   start()      starts the thread, named "NereusDisplayTx", at normal
//                priority, with the pass it runs.  Idempotent.
//   setActive()  whether any controller sends from it.  Inactive, the
//                thread waits for a wake without a timeout.
//   wakeFrames() an FFT engine published a frame; any thread.
//   wakeTurn()   a display channel took its held message: one turn now,
//                besides the timed ones; any thread.
//   stop()       wakes the thread and joins it.  The destructor calls it.
//                Never call it from inside a pass.
//
// The loop: wait until a frame is published, a turn is asked for, or the
// next timed turn (kDisplaySenderIntervalMs apart) is due, then run the pass
// with whether a timed turn is due and whether one was asked for.  Timed
// turns that fell behind are not made up: the next is one interval after
// the late one.  It holds no lock of its own while the pass runs.
class DaemonDisplaySendThread final {
public:
    // (timedTurn, askedTurn); frames are always taken in.
    using Pass = std::function<void(bool, bool)>;

    DaemonDisplaySendThread();
    ~DaemonDisplaySendThread();
    DaemonDisplaySendThread(const DaemonDisplaySendThread&) = delete;
    DaemonDisplaySendThread& operator=(const DaemonDisplaySendThread&) = delete;

    // ── Owner's thread ──────────────────────────────────────────────────
    void start(Pass pass);
    void setActive(bool active);
    void stop();
    bool isRunning() const { return m_thread.joinable(); }

    // ── Any thread ──────────────────────────────────────────────────────
    void wakeFrames();
    void wakeTurn();
    bool stopRequested() const { return m_stopRequested.load(std::memory_order_acquire); }
    // The running thread's id (empty when not running).
    std::thread::id threadId() const { return m_threadId.load(std::memory_order_acquire); }
    bool onThisThread() const { return std::this_thread::get_id() == threadId(); }
    // Passes completed (diagnostics and tests).
    quint64 passes() const { return m_passes.load(std::memory_order_acquire); }

private:
    void run();

    std::mutex m_mutex;
    std::condition_variable m_wake;
    // Under m_mutex.
    bool m_active{false};
    bool m_framesPending{false};
    bool m_turnPending{false};
    // Written only before the thread starts; the thread reads it.
    Pass m_pass;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<std::thread::id> m_threadId{};
    std::atomic<quint64> m_passes{0};
    std::thread m_thread;
};

} // namespace NereusSDR
