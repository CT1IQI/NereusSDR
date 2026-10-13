// Ported from src/gui/SpectrumWidget.cpp:9566–9632 (NereusSDR speed fold).
// Modification history (NereusSDR):
// 2026-10-12 — Swift committed-frame exact/wide fold by J.J. Boyd (KG4VCF),
// AI-assisted transformation via OpenAI Codex. Phone captures use their existing
// endpoint/context axes; they are already calibrated by the Core.
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


import NereusMedia

/// One incomplete3D capture, kept independently from painted stack history.
struct StackTraceFold: Equatable, Sendable {
    private var count = 0
    private var latest: DisplayFrame?
    private var exact: [Float] = []
    private var wide: [Float] = []
    private var coverage: BandCoverage?
    private var wideCoverage: BandCoverage?
    private var transmitting = false

    static func == (lhs: Self, rhs: Self) -> Bool {
        lhs.count == rhs.count && lhs.latest == rhs.latest
            && lhs.exact.map(\.bitPattern) == rhs.exact.map(\.bitPattern)
            && lhs.wide.map(\.bitPattern) == rhs.wide.map(\.bitPattern)
            && lhs.coverage == rhs.coverage && lhs.wideCoverage == rhs.wideCoverage
            && lhs.transmitting == rhs.transmitting
    }

    mutating func clear() { self = Self() }

    // From src/gui/SpectrumWidget.cpp:9566–9632; existing capture lifecycle
    // replaces native DDC/source identity, with no new transport metadata.
    mutating func append(_ frame: DisplayFrame, coverage: BandCoverage?, wide wideCoverage: BandCoverage?,
                         transmitting: Bool, divider: Int) -> DisplayFrame? {
        guard let coverage, coverage.spanHz > 0, !frame.waterfallDbm.isEmpty else {
            clear()
            return nil
        }
        // Fold the planes that this tick actually supplied, after calibration.
        // A remote wide plane already carries Core calibration and its capture axes.
        let offeredWide = transmitting ? [] : frame.wideDbm
        let offeredWideCoverage = transmitting ? nil : wideCoverage
        let incompatible = latest?.endpointId != frame.endpointId
            || latest?.contextGeneration != frame.contextGeneration
            || exact.count != frame.waterfallDbm.count || wide.count != offeredWide.count
            || self.coverage != coverage || self.wideCoverage != offeredWideCoverage
            || self.transmitting != transmitting
        if incompatible {
            // Keep painted history, but do not smooth a new source against it.
            clear()
        }
        if count == 0 {
            exact = frame.waterfallDbm
            wide = offeredWide
            count = 1
        } else {
            for index in exact.indices { exact[index] = Self.peak(exact[index], frame.waterfallDbm[index]) }
            for index in wide.indices { wide[index] = Self.peak(wide[index], offeredWide[index]) }
            count += 1
        }
        latest = frame
        self.coverage = coverage
        self.wideCoverage = offeredWideCoverage
        self.transmitting = transmitting
        // A divider lowered mid-fold (down to at or below the current count)
        // pushes right here, on this very tick, rather than waiting out
        // whatever the count target was when the fold started.
        guard count >= max(1, divider) else { return nil }
        let folded = DisplayFrame(endpointId: frame.endpointId, contextGeneration: frame.contextGeneration,
            encoderSequence: frame.encoderSequence, producerTimestamp: frame.producerTimestamp,
            isKeyframe: frame.isKeyframe, waterfallAdvance: true, minDbm: frame.minDbm, maxDbm: frame.maxDbm,
            traceDbm: frame.traceDbm, waterfallDbm: exact, wideDbm: wide)
        clear()
        return folded
    }

    private static func peak(_ previous: Float, _ offered: Float) -> Float {
        // Nonfinite samples are unavailable, rather than a permanent poisoned
        // bin: the actual sample-plane history already uses the same rule.
        if !previous.isFinite { return offered }
        return offered.isFinite ? max(previous, offered) : previous
    }
}
