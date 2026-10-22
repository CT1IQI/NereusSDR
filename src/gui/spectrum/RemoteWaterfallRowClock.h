// =================================================================
// src/gui/spectrum/RemoteWaterfallRowClock.h  (NereusSDR-native)
// =================================================================
// 2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// no-port-check: NereusSDR-original. The Thetis and AetherSDR notes below
// describe how they pace rows; no code is taken from either.
//
// The clock a remote pan's waterfall scrolls by. It runs at the Core's
// row period on this machine's steady clock, is read on every frame the
// pan draws, and says when the next queued row goes in and how far the
// glide toward it has come.
//
// Bench symptom 2026-10-21 KG4VCF: with the ticker at the Core's row
// period, a screen recording still showed the remote waterfall stop for
// about 50 ms roughly once a second and then jump most of a row, while
// the trace kept drawing. The glide ran from the moment each row reached
// the main thread, so a row handed over late rested the glide at the
// full row, and the on-time row after it arrived early and jumped. On
// this Mac the ticker thread fired up to 5 ms off its slot and the queued
// hand-off to the main thread added up to 11 ms more on an idle test; the
// ticker and the Core's clock also drift apart (-500 to -5000 ppm on
// JJ's window).
//
// How the references pace rows:
//   * Thetis adds a waterfall row inside its render loop every Nth
//     rendered frame (display.cs:6541-6557 [v2.10.3.15];
//     WaterfallUpdatePeriod is "in frame intervals"), with no glide: the
//     row cadence is the frame clock.
//   * AetherSDR draws each row the moment it arrives, restarts its scroll
//     clock there and glides over a measured ms-per-row, resting at the
//     full row when the next is late (SpectrumWidget.cpp
//     startWaterfallScrollAnimation(), SpectrumPreviewLogic.h
//     waterfallScrollProgressRows() [@a9a83e66]).
// This clock keeps Thetis's property, rows paced by a steady clock read
// on every frame, and AetherSDR's glide, and adds what a remote stream
// needs: each row comes with the time it is due on screen (the Core's
// timestamp on the audio's playout clock), handed over about 200 ms early,
// and the rows go in on a grid locked to those due times. The first row
// starts the grid at its due time; each later slot is the one before plus
// the row period over the rate. A row goes in one row period before its
// slot and glides in by its slot, so the glide finishes as the trace of
// the same frame is drawn. For each row put in, the clock measures how far
// its due time is from its slot, smooths that, and moves the next slot by
// a fraction of it (phase) and the rate by a bounded step (2% at most), a
// critically damped loop: a 20 ms step of the audio delay is followed over
// a second or two, never as a jump, and the drift of the Core's clock
// against this one is taken up by the rate. A due time a few rows off the
// grid (a retune, a new context, the audio hold growing by hundreds of
// ms) starts the grid again at that row. A row not in hand when its slide
// should start rests the glide; when it comes it goes in at once, its slot
// one row period on, and the grid then locks back toward the due times. A
// row cannot be due sooner than one row period after it came (an older
// Core sends no due times, so its rows are due as they come): the grid
// then runs one row behind the rows' arrival, steadily.
//
// Pure logic, no Qt objects: the widget passes the time and its queue.
// =================================================================
#pragma once

#include <QtGlobal>

namespace NereusSDR {

class RemoteWaterfallRowClock {
public:
    // The phase correction: this fraction of the smoothed error moves the
    // next slot, at most kMaxPhaseStepRows of a row.
    static constexpr double kPhaseGain = 0.1;
    static constexpr double kMaxPhaseStepRows = 0.25;
    // The rate correction per row, per row period of smoothed error:
    // kPhaseGain^2 / 4 damps the loop critically, so a step of the due
    // times is followed without overshoot.
    static constexpr double kRateGain = 0.0025;
    // The rate never moves more than 2% either way. The Core's clock and
    // this machine's differ by well under 1%.
    static constexpr double kMaxRateTrim = 0.02;
    // The smoothing of the measured error, per row put in.
    static constexpr double kErrorSmoothing = 0.5;
    // A due time more than this many row periods from its slot starts the
    // grid again at that row.
    static constexpr double kReanchorRows = 3.0;
    // Rows one frame may put in: rows far behind (after a stall) play out
    // a few a frame, gliding, instead of all at once.
    static constexpr int kMaxRowsPerFrame = 2;

    // The Core's row period in ns; 0 stops the clock. A new period starts
    // the grid again.
    void setPeriodNs(qint64 periodNs);
    qint64 periodNs() const { return m_periodNs; }
    bool running() const { return m_periodNs > 0; }

    // Start again: the next row starts the grid at its due time.
    void reset();

    // Advance to nowNs. queuedRows is the queue's depth; frontDueNs the
    // time its oldest row is due on screen and frontQueuedNs the time that
    // row was handed over, on the same clock (a row with no due time is
    // due when it was handed over). Returns how many rows to put in now,
    // oldest first.
    int advance(qint64 nowNs, int queuedRows, qint64 frontDueNs, qint64 frontQueuedNs);

    // How far the glide toward the last row put in has come, 0 to 1, as of
    // the last advance(): 1 at that row's slot, and while the clock waits.
    float progress() const { return float(m_progress); }

    // The current rate, 1 +/- kMaxRateTrim.
    double rate() const { return m_rate; }
    // The slot of the last row put in, in ns; 0 before the first.
    qint64 lastSlotNs() const { return qint64(m_slotNs); }
    // The smoothed error (due time minus slot), in ns.
    double smoothedErrorNs() const { return m_errorNs; }
    bool anchored() const { return m_anchored; }
    // Slots the clock reached with no row in hand since the last reset.
    int starvedSlots() const { return m_starvedSlots; }
    // Times the grid started again at a row since the last reset.
    int reanchors() const { return m_reanchors; }

private:
    void anchorAt(double slotNs);

    qint64 m_periodNs{0};
    bool   m_anchored{false};  // the grid has a slot
    bool   m_starved{false};   // resting: no row in hand at its slide start
    double m_slideStartNs{0.0};
    double m_slotNs{0.0};
    double m_progress{1.0};
    double m_rate{1.0};
    double m_errorNs{0.0};
    int    m_starvedSlots{0};
    int    m_reanchors{0};
};

} // namespace NereusSDR
