// =================================================================
// src/gui/SpectrumWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Share the nine Appearance colors and RX Custom definition,
//                 retaining per-pan picture colors and independent TX.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Share the computer TX scale without stale inactive saves;
//                 preserve non-Manual waterfall modes in Smooth Defaults.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Apply approved effective trims, runtime level refresh and
//                 stored-step Track floor bounds without endpoint writes.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Query closed-pan ship defaults through the shared inventory.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Persist each pan's requested Clarity choice; expose pure
//                 current values and page-scoped picture reset.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12  J.J. Boyd (KG4VCF). Display rebuild: normalization,
//                 markers and Track floor choices use sparse per-pan
//                 persistence and live setting announcements. Existing
//                 renderer defaults/ranges retained. AI-assisted via
//                 OpenAI Codex.
//   2026-10-11  J.J. Boyd (KG4VCF). flagsRaised() also follows a flag that
//                 raises its floating buttons outside a placement pass
//                 (VfoWidget::floatingButtonsRaised). AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd (KG4VCF). flagsRaised(), emitted after every
//                 raise of the flags, so an open overlay flyout stays above
//                 them (the flag covered the open Display pill and took its
//                 clicks). AI-assisted implementation via Anthropic Claude
//                 Code.
//   2026-10-11  J.J. Boyd (KG4VCF). Follow-up wave F6: a frame computed
//                 before the FFT input purge does not end the waterfall's
//                 hold after a key edge. AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Follow-up wave F3: setDisplayKeyed()
//                 says displayKeyedChanged() on each edge. AI-assisted
//                 transformation via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Follow-up wave F1: NB Blank's
//                 threshold is in dB (1 to 20, default 6), a NereusSDR
//                 design, under the new key WaterfallBlankingThresholdDb.
//                 AetherSDR's factor of raw tile intensity is not carried
//                 over and its key is no longer read. The ring, the
//                 replacement row and the transmit freeze stay as ported
//                 from AetherSDR. AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 19: the
//                 right-click menu is trimmed to the dB range, CTUN, Add
//                 notch here and a line that opens the Display pill
//                 (displayPillRequested); its Ref Level and Dyn Range ask
//                 for a range (bandRangeRequested) that the band store
//                 takes. AI-assisted transformation via Anthropic Claude
//                 Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 15: a pan a Core
//                 feeds takes its hold length from the Core's FFT shape (the
//                 granted size, the context's source rate and
//                 setRemoteDecimation()). AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 13: the window's
//                 key edges on every pan. setDisplayKeyed() saves the
//                 waterfall AGC run level for the band and freezes the
//                 follower on a key, and on the un-key restores the band's
//                 level (Thetis's restart floor with none,
//                 display.cs:2810-2820 [v2.10.3.15]), starts the hold and
//                 the noise-floor fast attack with the hold as its minimum.
//                 setLocalFftShape() and moxHoldLengthMs() give the hold
//                 its length. AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 12: Clarity's
//                 drawn noise floor moves toward its target on every
//                 waterfall row by Thetis's 0.6 / 0.4 row blend
//                 (display.cs:7662-7666 [v2.10.3.15]);
//                 setClarityFloorTargetDbm() sets the target and
//                 setClarityFloorDbm() stays the immediate form.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 8: Cal Offset
//                 as its own term. The station's receive offset
//                 (setStationCalOffsetDb, pushed with every frame, never
//                 saved) and the operator's Cal Offset (setDbmCalOffset,
//                 DisplayCalOffset) were one member, so the per-frame push
//                 erased the operator's value and the save stored the
//                 station's. displayCalOffsetDb() adds the two.
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 5: two
//                 AetherSDR ports, each per pan. The trace line takes a
//                 colour of its own on the QPainter and GPU paths
//                 (AetherSDR src/gui/SpectrumWidget.cpp:4135-4141,
//                 14404-14407 [@1e0718ad]) and until one is chosen
//                 follows the fill colour; the GPU fill's lower edge is
//                 the fill colour darkened (:14379) instead of a fixed
//                 blue. A background behind the 2D spectrum: a fill
//                 colour and a picture over it at a set opacity
//                 (:11099-11125, 14790-14804), on the GPU as a layer of
//                 its own drawn before the trace (:12465-12477,
//                 13621-13647, 14452-14465). Attribution block below.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 17: the
//                 waterfall NF-AGC offset is kept in 0.1 dB steps
//                 (setWaterfallAGCOffsetTenthsDb); the whole-dB setter
//                 and getter stay for Setup and the Core. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 4: three
//                 AetherSDR ports, each per pan. NB Blank replaces a
//                 waterfall row whose mean power jumps over a rolling
//                 baseline with the last good row (AetherSDR
//                 src/gui/SpectrumWidget.cpp:8426-8468, 4672-4691
//                 [@1e0718ad]; the row mean is taken on linear power
//                 because rows here are dBm). A fixed grid spacing for
//                 the grid and the scale (:3030-3036, 15054-15072,
//                 17021-17039) with Auto left as it was. The frequency
//                 scale's text size, with a strip that grows to fit
//                 (:3037-3044, 16989-16996). Attribution block below.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd (KG4VCF). Display pill plan Task 3: waterfall
//                 levels under Clarity. Black Level and Color Gain place
//                 the colour range against Clarity's noise floor (up =
//                 blacker; the 0.5 dB Black step and the 80 - 0.7 x Gain
//                 span from AetherSDR src/gui/SpectrumWidget.cpp:11593,
//                 11928 [@1e0718ad], through core/spectrum/ClarityLevels.h)
//                 and do nothing while Clarity is off. One function,
//                 effectiveWaterfallLevels(), serves dbmToRgb and
//                 traceWaterfallState. AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF. A row arriving while a resize is still
//                 settling no longer rescales the whole waterfall history;
//                 it is resampled into the history's current width and the
//                 debounce rescales once (jerky dock edge drag). The same
//                 when paused in scrollback: the viewport rebuild reads the
//                 history at its own width (historyResizeSettling()).
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF. R-DISP-RETUNE (third wave): the Core's
//                 word (coreCarriedOn) decides whether a context continues
//                 the picture shown; the pan's own moved-picture rule and
//                 the span asked are gone. AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-11  J.J. Boyd / KG4VCF. R-DISP-RETUNE: setRemoteSpectrumContext
//                 tells a picture moved in frequency (a retune, a pan drag)
//                 from a new picture. AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF. The GPU waterfall blends four rows as it
//                 glides (waterfall.frag, ported from AetherSDR) where it
//                 mixed two, so its grain no longer pulses once per row;
//                 the glide can be held at a set point for a native test.
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF. The GPU waterfall no longer draws across
//                 its ring's seam (the bottom line flashed each new row),
//                 its glide rests while the waterfall is paused, and a
//                 ring's worth of rows arriving with no frame drawn sends
//                 the whole texture. AI-assisted via Anthropic Claude Code.
//   2026-10-10  J.J. Boyd / KG4VCF. R-DISP-HOLD: setRemoteSpectrumContext
//                 keeps the painted frame, peak hold, blobs and waterfall
//                 rows when the context is for the picture already shown.
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-11  J.J. Boyd / KG4VCF. setDbGridStep(): the horizontal dB
//                 grid lines are Setup's dB Step apart (drawGrid chose 10
//                 or 5 dB itself). AI-assisted via Anthropic Claude Code.
//   2026-10-09  J.J. Boyd / KG4VCF. The GPU dynamic overlay clears and
//                 uploads every row an earlier frame may have left (a
//                 created texture, a taller spectrum band) with the band,
//                 so stale noise-floor lines no longer show over the
//                 frequency scale and waterfall after the spectrum gets
//                 shorter. AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF. Receiver flags that fold are placed
//                 again when they fold or open, and the pan reports where
//                 its flags show (receiverFlagRegion) so its corner badges
//                 stay under them. AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF. Rotor control plan Task 8: "Turn beam
//                 to CALL (330°)" under "Tune to CALL" in the spot menu
//                 (greyed with the reason with no rotor or no bearing), the
//                 menu moved into buildSpotContextMenu, and spotTuned from
//                 every spot tune for auto-turn. AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-05  J.J. Boyd / KG4VCF. Refresh slice flags and floating
//                 controls when the RF view changes, including accepted
//                 remote crops on GPU. AI-assisted via OpenAI Codex.
//   2026-10-04  J.J. Boyd / KG4VCF. Opt-in numeric actual-row diagnostics
//                 with applied extras provenance, bounded to one log per
//                 widget per 500 ms. AI-assisted via OpenAI Codex.
//   2026-10-04  J.J. Boyd / KG4VCF. Keep 2D TX history aligned on the
//                 restored RX frequency axis using each row's capture
//                 window. Original history pixels stay intact.
//                 AI-assisted via OpenAI Codex.
//   2026-10-01  J.J. Boyd / KG4VCF. Opt-in numeric RX history diagnostics.
//                 AI-assisted via OpenAI Codex.
//   2026-10-02 J.J. Boyd / KG4VCF : issues #141/#147: CPU paint, GPU
//                 paint and interaction use the existing GPU panel layout;
//                 divider drag reserves the same chrome height.
//                 AI-assisted via OpenAI Codex.
//   2026-09-30 J.J. Boyd / KG4VCF : GUI memory leak: renderGpuFrame writes
//                 the 2D trace's line, fill and peak vertex buffers only in
//                 a frame that binds them. A 3D pan (and a 2D pan with pan
//                 fill off, for the fill) wrote them every frame unbound,
//                 and Qt's Metal backend kept every write pending: 23 GB in
//                 2.5 hours. AI-assisted via Anthropic Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : desktop listening fix 3: the edge
//                 arrow is drawn by drawOffScreenArrow, for the pan's VFO
//                 and, one row each, for a slice shown here only as its
//                 flag (setEdgeMarkedSlice). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : desktop listening lane review: a
//                 marker's click says who controls the slice and offers
//                 Take control, in the listener words; nobody's slice says
//                 so. AI-assisted via Anthropic Claude Code.
//   2026-09-30 J.J. Boyd / KG4VCF : Level Cal fix wave: while the level
//                 calibration holds the grid's noise floor follow off, the
//                 saved value stays the user's (setGridFollowSaveHold).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-29 J.J. Boyd / KG4VCF : a remote window's waterfall AGC and
//                 NF-AGC colour against the Core's levels (display extras)
//                 when the Core offers them. AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : the grid's noise-floor tracking follows
//                 the pan's own display noise floor every 500 ms, as Thetis
//                 tmrAutoAGC does, so it works with Clarity off and in a
//                 remote window; a pan Clarity feeds keeps Clarity's grid.
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : settings stored once for every pan
//                 (overlays, normalize, peak value, grid noise-floor
//                 tracking, band plan size) reach every pan when one
//                 changes (shareWithOtherPans). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : setAverageMode routes to the split
//                 spectrum averaging the renderer reads (Reset to Smooth
//                 Defaults). AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : the 1 Hz normalise applies only
//                 with the Average, Sample and RMS spectrum detectors
//                 (normalizeActive). AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF : persist the local trace/fill colour
//                 per pan and let per-pan appearance colours inherit pan 0
//                 until overridden. AI-assisted via OpenAI Codex.
//   2026-09-27 J.J. Boyd / KG4VCF : local receive waterfall rows use
//                 RX1Offset before colour and level tracking, matching
//                 Core-calibrated remote rows. AI-assisted via OpenAI Codex.
//   2026-09-26 J.J. Boyd / KG4VCF : other devices' slices drawn as markers
//                 (drawForeignMarkers), their label's click says whose
//                 they are (iPhone app plan Task 78, R-IOS-02, R-IOS-30).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 J.J. Boyd / KG4VCF : the band-plan strip's lowest-licence-
//                 class rule moved unchanged to lowestLicenceClass() in
//                 models/BandPlan.h, which the station catalogue also
//                 calls (R-IOS-27, R-IOS-11). AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : processNoiseFloor, the fast-attack
//                 setter, the waterfall level composition, the normalise
//                 shift, the averaging constant, the calibration range and
//                 the peak-blob passband moved unchanged to
//                 core/spectrum/DisplayFollowers, and PeakBlobDetector and
//                 ActivePeakHoldTrace to core/spectrum (iPhone app Task 20,
//                 R-IOS-27). AI-assisted via Anthropic Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : the waterfall palettes' gradient stops
//                 and wfSchemeStops() moved unchanged to
//                 core/spectrum/WaterfallPalettes.cpp (iPhone app Task 19,
//                 R-IOS-06). AI-assisted via Anthropic Claude Code.
//   2026-09-24 J.J. Boyd / KG4VCF : setPeakHoldEnabled emits
//                 peakHoldEnabledChanged (R-R3-49, R-R3-21). AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-22 J.J. Boyd / KG4VCF — adapt AetherSDR prepareForShutdown()
//                 [@0dea0dd7]; native child teardown only, preserving the
//                 standalone QRhi owner's lifetime (OpenAI Codex).
//   2026-09-22 J.J. Boyd / KG4VCF — share wing reference with Core (OpenAI Codex).
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-01 / R-R3-08 / R-R3-12 (parity
//                 Task 17): a remote pan's bin width, normalise shift and
//                 Hz/bin readout follow the Core's granted FFT size, and its
//                 peak hold, blob and noise floor decay follow the frame
//                 rate the Core sends. AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-49 / R-R3-12 (parity Task 29): the
//                 GPU path draws no stale receive trace while the MOX overlay
//                 is on without transmit pixels (drawsSpectrumTrace).
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-49 (parity Tasks 27-29 fix wave):
//                 a remote pan's rise and fall retire the other axis's trace,
//                 so the GPU path cannot redraw it under the new axis before
//                 a frame for it lands. AI-assisted via Anthropic Claude Code.
//   2026-09-26 J.J. Boyd / KG4VCF - R-R3-21 / R-R3-08: a remote pan's
//                 waterfall rows queue for the ticker (one a tick, two while
//                 more than two wait) with the RF window they were captured
//                 at, instead of one pending row. AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-27 J.J. Boyd / KG4VCF - A11 / R-R3-49 (parity Task 31): display
//                 duplex (DUP), off by default as Thetis. Keyed with it on,
//                 the pan keeps the receive span and bins under the transmit
//                 grid and waterfall levels, the TX filter sits against the
//                 receive span without XIT, the receive trace takes the
//                 transmit calibration (RX1Offset), and a change while keyed
//                 swaps the view and resets the blob maxima and active peaks.
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 J.J. Boyd / KG4VCF - parity ruling C13: the performance
//                 overlay's lines come from perfOverlayLines(), which in a
//                 remote window adds the Core's drops after this computer's
//                 counters, each group headed. AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-10-01: Completed attribution for per-slice marker colors
//                 and selected-last paint order
//                 by J.J. Boyd (KG4VCF), with AI assistance via
//                 OpenAI Codex. Port introduced 2026-09-23.
//                 Source: AetherSDR src/gui/SpectrumWidget.cpp [@9f81dc00].
//                 Upstream has no per-file copyright header.
//                 Copyright (C) 2024-2026 Jeremy (KK7GWY) and
//                 AetherSDR contributors. GPLv3; project source:
//                 https://github.com/ten9876/AetherSDR
//   2026-10-08 J.J. Boyd / KG4VCF - workspace preview data colours:
//                 the spectrum, waterfall and DSS default backgrounds
//                 read WorkspacePreview::dataBackground() (navy until a
//                 choice is made). AI-assisted via Anthropic Claude Code.
//   2026-10-09 - Spot list and spot display changes mark the GPU path's
//                 static overlay dirty, as AetherSDR [@1e0718ad] does, so
//                 the spot labels follow them. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

// =================================================================
// Source attribution (AetherSDR, GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors,
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file carries ports and structural derivatives of AetherSDR
//   source. AetherSDR is licensed under the GNU General Public License
//   v3. NereusSDR is also GPLv3. Attribution follows GPLv3 §5
//   requirements. AetherSDR has no per-file license header; its project
//   LICENSE applies.
//
//   Ported at 1e0718ad from src/gui/SpectrumWidget.cpp (with
//   src/gui/SpectrumOverlayMenu.cpp for ranges): the NB waterfall
//   blanker, the frequency grid spacing and the frequency scale text
//   size (Modification history above, 2026-10-10, Task 4); the separate
//   trace colour and the spectrum background (2026-10-10, Task 5). Each
//   ported block carries its own "From AetherSDR" cite. The 3D
//   stacked-trace port has its own block below.
// =================================================================

// =================================================================
// Source attribution for 3D stacked-trace spectrum (AetherSDR, GPLv3):
//   Project lead: Jeremy (KK7GWY) / AetherSDR contributors
//   https://github.com/ten9876/AetherSDR
//   Upstream files at 1872028c: src/gui/SpectrumWidget.cpp,
//   src/gui/DssRenderer.cpp, resources/shaders/dss_mesh.vert, and
//   resources/shaders/dss_mesh.frag. AetherSDR has no per-file license
//   header; its project LICENSE is GPLv3.
//
// Modification history (3D stacked-trace port, NereusSDR):
//   2026-10-02 - Integrated the display bindings and speed fold with
//                landed remote capture, RX/TX and marker paths for
//                NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                integration via OpenAI Codex.
//   2026-08-08 - Adapted the AetherSDR 3D renderer and GPU upload path
//                for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code. The 3D Speed
//                row divider and peak-hold fold are NereusSDR additions.
//   2026-10-01 - Corrected frame-fold and skipped-paint GPU uploads for
//                NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via OpenAI Codex.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

// Swift display-only translation of SpectrumWidget::blankWaterfallRow.
// Modification history (NereusSDR Swift):
//   2026-10-12: J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// Original mixed-source license/header notices above are retained verbatim.

import Foundation

/// The existing rolling mean-power blanker; its ring freezes during TX.
public struct WaterfallBlanker: Equatable, Sendable {
    // From AetherSDR src/gui/SpectrumWidget.h:1771-1779 [@1e0718ad]
    // NereusSDR, based on AetherSDR: the ring holds each row's mean LINEAR
    // power (rows here are dBm, AetherSDR's are intensity), the last good
    // row is kept in dBm, and the unused fill / interpolate mode is left
    // out.
    // NB Waterfall Blanker (#277)
    private static let ringSize = 32
    // From AetherSDR src/gui/SpectrumWidget.cpp:8449 [@1e0718ad]: rows of
    // history needed before a row can be called a burst.
    private static let minimumRows = 8
    // From AetherSDR src/gui/SpectrumWidget.cpp:8460 [@1e0718ad]: what a burst
    // row may add to the baseline, as a factor of the baseline.
    private static let burstRingCap: Float = 1.05
    private var ring = [Float](repeating: 0, count: WaterfallBlanker.ringSize)
    private var next = 0
    private var count = 0
    private var lastGood: [Float] = []

    public init() {}
    public mutating func reset() { self = WaterfallBlanker() }

    public mutating func filtered(_ row: [Float], enabled: Bool, transmitting: Bool, thresholdDb: Int) -> [Float] {
    // NB Waterfall Blanker (#277) — suppress impulse rows.
    // Skip entirely during TX: with show-tx-in-waterfall enabled, TX-era tiles
    // flow through and would otherwise poison the rolling baseline.  Post-TX,
    // real RX rows would then read as huge "impulses" against the suppressed
    // TX-era baseline, causing the blanker to substitute m_wfLastGoodLevels (a
    // TX-era row) for ~10–18 s while baseline slowly re-converges —
    // visible as a frozen, striped waterfall.  Freezing the ring across TX
    // means post-TX rows are compared against the pre-TX baseline instead.
        guard enabled && !transmitting else { return row }
        let sum = row.reduce(0.0) { $0 + pow(10.0, Double($1) / 10.0) }
        let mean: Float = row.isEmpty ? 0 : Float(sum / Double(row.count))

        // Compute rolling baseline
        var baseline: Float = 0
        for index in 0..<count { baseline += ring[index] }
        if count > 0 { baseline /= Float(count) }

        // Detect impulse (need ≥8 rows of history)
        // NereusSDR: the row's level and the baseline in dB, and the
        // threshold in dB between them (AetherSDR: rowMean > baseline *
        // m_wfBlankerThreshold, :8449).
        var burst = false
        if count >= Self.minimumRows && baseline > 0 && mean > 0 {
            let rowDb: Float = 10 * log10(mean)
            let baselineDb: Float = 10 * log10(baseline)
            burst = rowDb - baselineDb > Float(thresholdDb)
        }
        var result = row
        if burst {
            // Impulse detected — replace with last good row (interpolate)
            if lastGood.count == row.count { result = lastGood }
            else {
                // No previous good row yet — fill with noise floor color
                result = [Float](repeating: 10 * log10(baseline), count: row.count)
            }
            ring[next] = min(mean, baseline * Self.burstRingCap)
        } else {
            lastGood = row
            ring[next] = mean
        }
        next = (next + 1) % Self.ringSize
        if count < Self.ringSize { count += 1 }
        return result
    }
}
