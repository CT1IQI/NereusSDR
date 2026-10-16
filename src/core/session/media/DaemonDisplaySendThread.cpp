// =================================================================
// src/core/session/media/DaemonDisplaySendThread.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See DaemonDisplaySendThread.h.
//
// Modification history (NereusSDR):
//   2026-10-15: created.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/media/DaemonDisplaySendThread.h"

#include "core/session/media/DisplayBudget.h"

#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <pthread.h>
#endif

namespace NereusSDR {

namespace {

void nameThisThread()
{
#if defined(Q_OS_MAC)
    pthread_setname_np("NereusDisplayTx");
#elif defined(Q_OS_LINUX)
    pthread_setname_np(pthread_self(), "NereusDisplayTx");
#endif
}

} // namespace

// ── DaemonDisplayLock ────────────────────────────────────────────────────

void DaemonDisplayLock::lock()
{
    m_mutex.lock();
    if (m_depth++ == 0) {
        m_owner.store(std::this_thread::get_id(), std::memory_order_release);
    }
}

bool DaemonDisplayLock::tryLockFor(std::chrono::microseconds timeout)
{
    if (!m_mutex.try_lock_for(timeout)) {
        return false;
    }
    if (m_depth++ == 0) {
        m_owner.store(std::this_thread::get_id(), std::memory_order_release);
    }
    return true;
}

void DaemonDisplayLock::unlock()
{
    // The outermost hold of a thread other than the exempt one runs the
    // hook before it lets go; a hold the hook itself takes does not.
    if (m_depth == 1 && m_hook && !m_inHook
        && std::this_thread::get_id() != m_hookExempt.load(std::memory_order_acquire)) {
        m_inHook = true;
        m_hook();
        m_inHook = false;
    }
    if (--m_depth == 0) {
        m_owner.store(std::thread::id(), std::memory_order_release);
    }
    m_mutex.unlock();
}

bool DaemonDisplayLock::heldByCurrentThread() const
{
    return m_owner.load(std::memory_order_acquire) == std::this_thread::get_id();
}

void DaemonDisplayLock::setReleaseHook(ReleaseHook hook)
{
    m_hook = std::move(hook);
}

void DaemonDisplayLock::setHookExemptThread(std::thread::id hookExempt)
{
    m_hookExempt.store(hookExempt, std::memory_order_release);
}

// ── DaemonDisplaySendThread ──────────────────────────────────────────────

DaemonDisplaySendThread::DaemonDisplaySendThread() = default;

DaemonDisplaySendThread::~DaemonDisplaySendThread()
{
    stop();
}

void DaemonDisplaySendThread::start(Pass pass)
{
    if (m_thread.joinable()) {
        return;
    }
    m_pass = std::move(pass);
    m_stopRequested.store(false, std::memory_order_release);
    m_thread = std::thread([this] { run(); });
    // Known on this thread from here; run() stores the same id before its
    // first pass.
    m_threadId.store(m_thread.get_id(), std::memory_order_release);
}

void DaemonDisplaySendThread::setActive(bool active)
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_active = active;
    }
    m_wake.notify_all();
}

void DaemonDisplaySendThread::stop()
{
    if (!m_thread.joinable()) {
        return;
    }
    m_stopRequested.store(true, std::memory_order_release);
    {
        // Under the mutex, so a wait that checked the flag just before it
        // was set has begun waiting and is woken.
        const std::lock_guard<std::mutex> lock(m_mutex);
    }
    m_wake.notify_all();
    m_thread.join();
    m_threadId.store(std::thread::id(), std::memory_order_release);
}

void DaemonDisplaySendThread::wakeFrames()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_framesPending = true;
    }
    m_wake.notify_all();
}

void DaemonDisplaySendThread::wakeTurn()
{
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_turnPending = true;
    }
    m_wake.notify_all();
}

void DaemonDisplaySendThread::run()
{
    nameThisThread();
    m_threadId.store(std::this_thread::get_id(), std::memory_order_release);
    using Clock = std::chrono::steady_clock;
    constexpr auto kInterval = std::chrono::milliseconds(kDisplaySenderIntervalMs);
    Clock::time_point nextTurn = Clock::now() + kInterval;
    while (!m_stopRequested.load(std::memory_order_acquire)) {
        bool askedTurn = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            const auto woken = [this] {
                return m_stopRequested.load(std::memory_order_acquire) || m_framesPending
                    || m_turnPending;
            };
            if (m_active) {
                m_wake.wait_until(lock, nextTurn, woken);
            } else {
                m_wake.wait(lock, [this, &woken] { return woken() || m_active; });
                // Turns start again one interval after the thread is used.
                nextTurn = Clock::now() + kInterval;
            }
            if (m_stopRequested.load(std::memory_order_acquire)) {
                break;
            }
            m_framesPending = false;
            askedTurn = std::exchange(m_turnPending, false);
        }
        const Clock::time_point now = Clock::now();
        const bool timedTurn = now >= nextTurn;
        if (timedTurn) {
            nextTurn += kInterval;
            if (nextTurn <= now) {
                nextTurn = now + kInterval;
            }
        }
        m_pass(timedTurn, askedTurn);
        m_passes.fetch_add(1, std::memory_order_acq_rel);
    }
}

} // namespace NereusSDR
