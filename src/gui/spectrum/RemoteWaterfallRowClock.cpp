// =================================================================
// src/gui/spectrum/RemoteWaterfallRowClock.cpp  (NereusSDR-native)
// =================================================================
// 2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// See RemoteWaterfallRowClock.h for design rationale.
// =================================================================
#include "RemoteWaterfallRowClock.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

void RemoteWaterfallRowClock::setPeriodNs(qint64 periodNs)
{
    periodNs = std::max<qint64>(0, periodNs);
    if (periodNs == m_periodNs) {
        return;
    }
    m_periodNs = periodNs;
    reset();
}

void RemoteWaterfallRowClock::reset()
{
    m_anchored = false;
    m_starved = false;
    m_slideStartNs = 0.0;
    m_slotNs = 0.0;
    m_progress = 1.0;
    m_rate = 1.0;
    m_errorNs = 0.0;
    m_starvedSlots = 0;
    m_reanchors = 0;
}

void RemoteWaterfallRowClock::anchorAt(double slotNs)
{
    m_slideStartNs = slotNs - double(m_periodNs) / m_rate;
    m_slotNs = slotNs;
    m_anchored = true;
    m_starved = false;
}

int RemoteWaterfallRowClock::advance(qint64 nowNs, int queuedRows, qint64 frontDueNs,
                                     qint64 frontQueuedNs)
{
    if (!running()) {
        return 0;
    }
    const double now = double(nowNs);
    const double period = double(m_periodNs);
    const double reanchorNs = kReanchorRows * period;
    int rows = 0;
    while (rows < kMaxRowsPerFrame && queuedRows - rows > 0) {
        const double step = period / m_rate;
        // The slot this row should have: its due time, but never sooner than
        // a row period after it was handed over, the shortest glide it can
        // have. Only the oldest row's times are known; a second row this
        // frame (playing out rows far behind) takes the grid's next slot.
        double target = m_slotNs + step;
        bool lateBound = false;
        if (rows == 0) {
            const double earliest = double(frontQueuedNs) + period;
            lateBound = earliest > double(frontDueNs);
            target = std::max(double(frontDueNs), earliest);
        }
        if (!m_anchored) {
            // The grid starts at this row: it slides in from one row period
            // before its due time.
            if (now < target - step) {
                break;
            }
            anchorAt(target);
            ++rows;
            continue;
        }
        if (m_starved && rows == 0) {
            // It was not in hand when its slide should have started, and the
            // glide rested. It goes in now, its slot a row period on, unless
            // its due time is later than that (the waterfall rests rather
            // than plays a row early) or a few rows earlier: then the grid
            // starts again at it.
            m_starved = false;
            const double lateSlot = now + step;
            if (target > lateSlot || lateSlot - target > reanchorNs) {
                ++m_reanchors;
                m_errorNs = 0.0;
                m_anchored = false;
                continue;
            }
            m_slideStartNs = now;
            m_slotNs = lateSlot;
            ++rows;
            continue;
        }
        // The next row's slide starts at the last row's slot.
        if (now < m_slotNs) {
            break;
        }
        const double scheduled = m_slotNs + step;
        double error = target - scheduled;
        if (rows == 0 && std::abs(error) > reanchorNs) {
            ++m_reanchors;
            m_errorNs = 0.0;
            m_anchored = false;
            continue;
        }
        // A row that came too late for its due time cannot be shown sooner
        // than a row period after it came: an earlier slot is no goal.
        if (lateBound && error < 0.0) {
            error = 0.0;
        }
        double correction = 0.0;
        if (rows == 0) {
            m_errorNs += kErrorSmoothing * (error - m_errorNs);
            const double maxStep = kMaxPhaseStepRows * step;
            correction = std::clamp(kPhaseGain * m_errorNs, -maxStep, maxStep);
            m_rate = std::clamp(m_rate - kRateGain * m_errorNs / period,
                                1.0 - kMaxRateTrim, 1.0 + kMaxRateTrim);
        }
        m_slideStartNs = m_slotNs;
        m_slotNs = scheduled + correction;
        ++rows;
    }
    if (rows == 0 && m_anchored && queuedRows <= 0 && now >= m_slotNs && !m_starved) {
        // At the slide start with no row in hand: the glide rests at the
        // full row until one comes.
        m_starved = true;
        ++m_starvedSlots;
    }
    if (m_anchored && m_slotNs > m_slideStartNs) {
        m_progress = std::clamp((now - m_slideStartNs) / (m_slotNs - m_slideStartNs), 0.0, 1.0);
    } else {
        m_progress = 1.0;
    }
    return rows;
}

} // namespace NereusSDR
