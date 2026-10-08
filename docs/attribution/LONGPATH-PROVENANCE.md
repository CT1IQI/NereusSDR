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

NereusSDR-original files that use the palette but port nothing
(QssTranslator, WorkspacePreview and the call-site branches) carry a
`no-port-check:` marker and are not listed.
