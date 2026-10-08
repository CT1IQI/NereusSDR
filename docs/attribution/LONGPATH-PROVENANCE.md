# Longpath Provenance: NereusSDR derived-file inventory

This document lists every NereusSDR source file derived from, translated
from, or materially based on Longpath. Per-file license headers live in
the source files themselves; this index is the grep-able summary.

NereusSDR is distributed under GPLv3 (root `LICENSE`). Longpath is GPLv3
at project level, so the two are compatible.

## Upstream

| Field | Value |
|---|---|
| Project | Longpath |
| Repository | https://github.com/oe5sos/Longpath |
| Revision | 551576e30c42731c6d51ce005c5f23ec38be391c |
| Primary author | Martin Fischer (OE5SOS) |
| License | GPLv3 at project level (the project's LICENSE file) |

Longpath grew out of NereusSDR. Some Longpath files carry
a Longpath banner with AetherSDR attribution (for example
`src/gui/StyleConstants.h`); NereusSDR copies those banners byte-for-byte.
Where an upstream file has no top-of-file copyright or GPL header, the
NereusSDR header says so and the project-level LICENSE applies.

## When entries get added

A row is added below, in the same commit that introduces the ported
logic, whenever a NereusSDR file ports, translates or materially
re-expresses logic or values from a Longpath source file. The procedure
follows `HOW-TO-PORT.md`: the Longpath banner of each source file is
copied byte-for-byte under `// --- From <file> ---`, followed by a
NereusSDR block and a modification history, and every ported value or
block carries an inline cite of the form
`// From Longpath <path>:<line> [@551576e3]`.

## Files

| NereusSDR file | Longpath source | Line ranges | Type | Notes |
|---|---|---|---|---|
| src/gui/styles/WorkspacePalette.h | src/gui/StyleConstants.h; src/gui/styles/ThemeQss.cpp | StyleConstants.h 90-1110; ThemeQss.cpp 36-153 | Port (values) | Glass and Depth palette values as named constants and roles; role names from ThemeQss.cpp. Workspace preview only. |
| src/gui/styles/WorkspacePalette.cpp | src/gui/styles/ThemeQss.cpp; src/gui/StyleConstants.h | ThemeQss.cpp 36-153 | Port (table) | Role table and the legacy-colour table. Differs from upstream: no blanket #203040 row, no inset row, no identity, instrument or txfilter rows, no current-value rows. |
| src/gui/styles/WorkspaceStyle.h | src/gui/StyleConstants.h; src/gui/styles/ThemeQss.cpp | StyleConstants.h 70-1401; ThemeQss.cpp 284-288 | Port (values) | Font ladder, caps tracking, mono families, scroll bar stylesheet. |
| src/gui/styles/WorkspaceStyle.cpp | src/gui/StyleConstants.h; src/gui/styles/ThemeQss.cpp | StyleConstants.h 544-1401; ThemeQss.cpp 284-288 | Port (helpers) | capsFont, monoFont, paintInsetTop, paintGlassChip, shiftL, glassPanelFill, titleBarStyle. |
| src/gui/styles/WorkspaceComponents.h | src/gui/StyleConstants.h | StyleConstants.h 723-1121 | Port (values) | Raised, pressed, glass-selected and sunken fills and edges for the component treatment. Workspace preview only. |
| src/gui/styles/WorkspaceComponents.cpp | src/gui/StyleConstants.h | StyleConstants.h 723-1121 | Port (fills) | Fill and edge values only; Longpath padding, font and handle sizes and its 7 px radius are not ported. componentize() itself is NereusSDR-original. |
| src/gui/containers/WorkspaceHeader.h | src/gui/applets/GridCellWidget.cpp | GridCellWidget.cpp 1-10 (banner) | Port (structure) | The group header's row (stripe, grab handle, title, buttons) follows GridCellWidget's title bar. The source chip, lock glyph, entry header and dot corner are NereusSDR-original. Workspace preview only. |
| src/gui/containers/WorkspaceHeader.cpp | src/gui/applets/GridCellWidget.cpp | GridCellWidget.cpp 59-155 | Port (values) | Header stylesheet and top radius, margins and spacing, 3 px stripe, ⋮⋮ grip at 11 px, 16x14 buttons and their stylesheet. Height 18 (Longpath 22), the entry header and the chip are the workspace review's proposal. |
| src/gui/containers/EdgeDockStrip.h | src/gui/applets/AppletGrid.cpp; src/gui/StyleConstants.h; src/gui/WindowPlacement.h | AppletGrid.cpp 24; StyleConstants.h 750; WindowPlacement.h 58 | Port (values) | 6 px strip padding and gap from kCellGap, 6 px handle from kSplitterHandlePx, 8 px alignment grid from kSnapGridPx. Longpath has no four-edge docking: the strips, the edge area and the drop preview are NereusSDR-original. Workspace preview only. |
| src/gui/containers/EdgeDockStrip.cpp | src/gui/WindowPlacement.cpp | WindowPlacement.cpp 140-147 | Port (helper) | workspaceSnappedTopLeft() follows snappedTopLeft(). Differs from upstream: Alt bypasses the grid (Longpath uses Ctrl; NereusSDR already uses Ctrl to round to 10), and the grid is off by default. |

| src/gui/instruments/FrequencyInstrument.h | src/gui/instruments/FrequencyInstrument.h | FrequencyInstrument.h 1-208 | Port | Namespace Longpath becomes NereusSDR. Frequency readout with per-digit wheel tuning and three forms. Workspace preview only. |
| src/gui/instruments/FrequencyInstrument.cpp | src/gui/instruments/FrequencyInstrument.h; src/gui/instruments/FrequencyInstrument.cpp | FrequencyInstrument.h 1-208; FrequencyInstrument.cpp 1-763 | Port | Namespace Longpath becomes NereusSDR. LP_MONO_QSS becomes Workspace::kMonoQssFamilies through .arg(). Style:: resolves through InstrumentStyle.h. |
| src/gui/instruments/NeedleInstrument.h | src/gui/instruments/NeedleInstrument.h | NeedleInstrument.h 1-181 | Port | Namespace Longpath becomes NereusSDR. Needle meter. |
| src/gui/instruments/NeedleInstrument.cpp | src/gui/instruments/NeedleInstrument.h; src/gui/instruments/NeedleInstrument.cpp | NeedleInstrument.h 1-181; NeedleInstrument.cpp 1-450 | Port | Namespace Longpath becomes NereusSDR. Three constants unused upstream carry [[maybe_unused]]. |
| src/gui/instruments/BarInstrument.h | src/gui/instruments/BarInstrument.h | BarInstrument.h 1-174 | Port | Namespace Longpath becomes NereusSDR. Bar meter. |
| src/gui/instruments/BarInstrument.cpp | src/gui/instruments/BarInstrument.h; src/gui/instruments/BarInstrument.cpp | BarInstrument.h 1-174; BarInstrument.cpp 1-381 | Port | Namespace Longpath becomes NereusSDR. |
| src/gui/instruments/InstrumentFooter.h | src/gui/instruments/InstrumentFooter.h | InstrumentFooter.h 1-65 | Port | Namespace Longpath becomes NereusSDR. Reading footer under the instruments. |
| src/gui/instruments/InstrumentFooter.cpp | src/gui/instruments/InstrumentFooter.h; src/gui/instruments/InstrumentFooter.cpp | InstrumentFooter.h 1-65; InstrumentFooter.cpp 1-97 | Port | Namespace Longpath becomes NereusSDR. |
| src/gui/instruments/InstrumentPainter.h | src/gui/instruments/InstrumentPainter.h | InstrumentPainter.h 1-172 | Port | Namespace Longpath becomes NereusSDR. Trough, fade, glow and needle painting shared by the instruments. |
| src/gui/instruments/InstrumentPainter.cpp | src/gui/instruments/InstrumentPainter.h; src/gui/instruments/InstrumentPainter.cpp | InstrumentPainter.h 1-172; InstrumentPainter.cpp 1-473 | Port | Namespace Longpath becomes NereusSDR. Style::role resolves to Workspace::role. |
| src/gui/instruments/InstrumentSpine.h | src/gui/instruments/InstrumentSpine.h | InstrumentSpine.h 1-239 | Port | Namespace Longpath becomes NereusSDR. Arc and linear geometry. |
| src/gui/instruments/InstrumentSpine.cpp | src/gui/instruments/InstrumentSpine.h; src/gui/instruments/InstrumentSpine.cpp | InstrumentSpine.h 1-239; InstrumentSpine.cpp 1-249 | Port | Namespace Longpath becomes NereusSDR. |
| src/gui/instruments/ReadingSource.h | src/gui/instruments/ReadingSource.h | ReadingSource.h 1-185 | Port | Namespace Longpath becomes NereusSDR. Reading descriptors and scales. Adds ReadingDescriptor::available (NereusSDR). |
| src/gui/instruments/ReadingSource.cpp | src/gui/instruments/ReadingSource.h; src/gui/instruments/ReadingSource.cpp | ReadingSource.h 1-185; ReadingSource.cpp 1-388 | Port | Namespace Longpath becomes NereusSDR. NoiseFloor (9) and RadeSnr (10) are local ids, marked unavailable: no NereusSDR producer. MeterBinding is unchanged. TX: ALC gains a scale from Thetis MeterManager.cs (NereusSDR addition, see THETIS-PROVENANCE). |
| src/gui/instruments/PeakHold.h | src/gui/instruments/PeakHold.h | PeakHold.h 1-103 | Port | Namespace Longpath becomes NereusSDR. Peak hold. |
| src/gui/widgets/BandwidthFilterPane.h | src/gui/widgets/BandwidthFilterPane.h | BandwidthFilterPane.h 1-172 | Port | Namespace Longpath becomes NereusSDR. Filter passband pane. |
| src/gui/widgets/BandwidthFilterPane.cpp | src/gui/widgets/BandwidthFilterPane.h; src/gui/widgets/BandwidthFilterPane.cpp | BandwidthFilterPane.h 1-172; BandwidthFilterPane.cpp 1-755 | Port | Namespace Longpath becomes NereusSDR. |
| src/core/antenna/AmateurBands.h | src/core/antenna/AmateurBands.h | AmateurBands.h 1-89 | Port | Namespace Longpath becomes NereusSDR. IARU band plan table (Qt only). Namespace Longpath::AmateurBands becomes NereusSDR::AmateurBands. |
| src/core/antenna/AmateurBands.cpp | src/core/antenna/AmateurBands.h; src/core/antenna/AmateurBands.cpp | AmateurBands.h 1-89; AmateurBands.cpp 1-163 | Port | Namespace Longpath becomes NereusSDR. |
| src/gui/instruments/LongpathContent.cpp | src/gui/applets/BandwidthFilterApplet.cpp | BandwidthFilterApplet.cpp 779-837 | Port (values) | Passband trace refresh interval (50 ms) and density (one point per 12 device pixels, 64 to 400). The registry factories, the slice and MeterPoller bindings and the trace accessor over SpectrumWidget::renderedPixels() are NereusSDR-original. Workspace preview only. |
| src/gui/widgets/PillStyle.h | src/gui/widgets/CommandBar.h | CommandBar.h 3-58 (banner), 167-183 | Port (values) | CommandBar::pillStyle(), kPillHeight (27) and kPillRadius (6) only, as namespace PillStyle. CommandBar itself is not ported. Workspace preview only. |
| src/gui/widgets/PillStyle.cpp | src/gui/widgets/CommandBar.h; src/gui/widgets/CommandBar.cpp | CommandBar.h 3-58 (banner); CommandBar.cpp 1-17 (banner), 59-98 | Port | pillStyle() verbatim. Style:: resolves through InstrumentStyle.h. |
| src/gui/widgets/ProfileRail.h | src/gui/widgets/ProfileRail.h | ProfileRail.h 3-44 (banner), 46-198 | Port | Namespace Longpath becomes NereusSDR. Reads WorkspaceProfiles (ids) instead of LayoutProfiles (names); adds manageRequested(), badgeButton(), plusButton() and setPlacementToggleOffered(). Workspace preview only. |
| src/gui/widgets/ProfileRail.cpp | src/gui/widgets/ProfileRail.h; src/gui/widgets/ProfileRail.cpp | ProfileRail.h 3-44 (banner); ProfileRail.cpp 1-421 | Port | User strings in English, comments verbatim. ScopedChildWidget replaced by a window-parented QPointer menu. Context menu gains Manage Profiles; the move-the-rail item can be left out. |

NereusSDR-original files that use the palette but port nothing
(QssTranslator, WorkspacePreview, InstrumentStyle.h, LongpathContent.h,
WorkspaceProfiles, WorkspaceProfilesDialog, ProfileRailDock and the
call-site branches) carry a
`no-port-check:` marker and are not listed.
