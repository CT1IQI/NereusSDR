// src/gui/styles/WorkspaceComponents.h

// --- From src/gui/StyleConstants.h ---
// =================================================================
// src/gui/StyleConstants.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Theme palette imported from AetherSDR `src/gui/ComboStyle.h` /
//                 `HGauge.h` / `SliceColors.h` and inline QColor calls
//                 in `MainWindow.cpp` / `VfoWidget.cpp`.
// =================================================================

// =================================================================
// src/gui/styles/WorkspaceComponents.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/StyleConstants.h (the raised, pressed, selected and sunken
//   fills and edges of buttonBaseStyle, blueCheckedStyle, sunkenFill,
//   sliderHStyle, glassFieldStyle, insetValueStyle and kComboStyle) at
//   revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banner is copied above.
//
//   The upstream file carries the AetherSDR copyright and GPLv3
//   notice reproduced above. Longpath's project LICENSE is GPLv3.
//
//   Only the colour values and fills are ported. Longpath's padding,
//   font sizes and handle sizes are not: the workspace preview must not
//   move or resize any widget. The radius rule (4 px up to 24 px high,
//   6 px above) is the workspace design's, not Longpath's 7 px.
//
//   componentize() is NereusSDR-original: it rewrites the declarations
//   of a stylesheet in place, by control class and state, so the
//   result keeps the original selectors and their specificity.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The licence note corrected.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - tableRules(): the grid and header colours of Longpath's
//                 tableStyle(). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - generatedColours(): the colours componentize() writes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QString>
#include <QStringList>

class QColor;
class QWidget;

namespace NereusSDR::Workspace::Components {

// Raised: light top, dark bottom (buttons, selectors, tabs).
QString raisedFill();
QString raisedHoverFill();
// Pressed: the raised fill flipped.
QString pressedFill();
// Selected: the glass gradient.
QString glassFill();
// Sunken: dark top, lighter bottom (fields, value chips, grooves).
QString sunkenFill(const char* baseHex, int deepen, int lift);
QString fieldFill();

// The radius for a control of the given height in device-independent
// pixels: 4 up to 24 px, 6 above.
int radiusFor(int heightPx);

// Application-wide rules for tables and header sections in the
// workspace look: grid lines, the line under the titles and the edges
// between sections, from the workspace roles. Colours only, plus the
// header's horizontal padding.
QString tableRules();

// Rewrites the background, border colours, text colour and radius of
// every control block in `qss` (already translated to palette values)
// into the raised, pressed, selected, sunken or disabled treatment.
// Selectors, widths, paddings, fonts and sizes are kept, so geometry
// does not change. Semantic colours (green, amber, red, slice colours)
// and translucent or transparent backgrounds are kept. `owner` is the
// widget the sheet is set on (may be null); it resolves selector-less
// and id-only blocks. Applying it to its own output changes nothing.
QString componentize(const QString& qss, const QWidget* owner);

// Every colour componentize() writes that is not a palette value (the
// shifted gradient stops, the raised hover pair, the glass edges), as
// QssTranslator reports a literal. A sheet the program copies from an
// already themed widget brings them back to the translator; the preview
// leaves them out of its unknown-literal count.
QStringList generatedColours();

} // namespace NereusSDR::Workspace::Components
