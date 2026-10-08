// src/gui/styles/WorkspaceStyle.cpp

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

// --- From src/gui/styles/ThemeQss.cpp ---
// =================================================================
// src/gui/styles/ThemeQss.cpp  (Longpath)
// =================================================================
//
// Longpath-original. See ThemeQss.h for why this exists and Theme.h
// for the layer it belongs to.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-08-15 — Rollen ergänzt; die Tabelle fragt jetzt Theme, damit
//                 die Palette aus einer Datei außerhalb des Repos
//                 kommen kann.
// =================================================================

// =================================================================
// src/gui/styles/WorkspaceStyle.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/StyleConstants.h and src/gui/styles/ThemeQss.cpp at
//   revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 hexRole(kX) becomes role("name", kX) against the
//                 built-in WorkspacePalette.
// =================================================================

#include "WorkspaceStyle.h"

#include "WorkspacePalette.h"

#include <QLatin1String>
#include <QPainter>
#include <QRect>
#include <QStringLiteral>

#include <algorithm>

namespace NereusSDR::Workspace {

// From Longpath src/gui/styles/ThemeQss.cpp:284-288 [@551576e3]
QString role(const char* roleName, const char* fallback)
{
    return WorkspacePalette::glassAndDepth().qss(QString::fromLatin1(roleName),
                                                 QString::fromLatin1(fallback));
}

// From Longpath src/gui/StyleConstants.h:1350-1358 [@551576e3]
QFont capsFont(const QFont& base, int px)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(QFont::DemiBold);
    f.setCapitalization(QFont::AllUppercase);
    f.setLetterSpacing(QFont::AbsoluteSpacing, px * kCapsTracking);
    return f;
}

// From Longpath src/gui/StyleConstants.h:1383-1401 [@551576e3]
QFont monoFont(const QFont& base, int px, QFont::Weight w)
{
    QFont f = base;
    f.setPixelSize(px);
    f.setWeight(w);
    // setFamilies statt setFamily: die ganze Rueckfallkette, dieselbe wie
    // LP_MONO_QSS. Mit setFamily() stand hier nur "Menlo" -- auf einem
    // Rechner ohne Menlo nahm Qt die naechstbeste Schrift, und das war
    // nicht zwingend eine dicktengleiche.
    f.setFamilies({QStringLiteral("Menlo"),
                   QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("monospace")});
    // Menlo gibt es nur auf dem Mac. Ohne diesen Hinweis nahm Qt unter
    // Windows/Linux eine proportionale Schrift, und die Stellen tanzten
    // doch (2026-09-27).
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);
    return f;
}

// From Longpath src/gui/StyleConstants.h:544-553 [@551576e3]
void paintInsetTop(QPainter& p, const QRect& r)
{
    static const int kShade[] = {kGlassShadeAlpha, 70, 40, 18};
    for (int i = 0; i < 4; ++i) {
        p.setPen(QColor(0, 0, 0, kShade[i]));
        p.drawLine(r.left(), r.top() + 1 + i, r.right(), r.top() + 1 + i);
    }
    p.setPen(QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(r.left(), r.top(), r.right(), r.top());
}

// From Longpath src/gui/StyleConstants.h:575-589 [@551576e3]
void paintGlassChip(QPainter& p, const QRect& box, int radius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(role("border", kBorder)));
    p.setBrush(QColor(0, 0, 0));
    p.drawRoundedRect(box, radius, radius);
    p.setClipRect(box.adjusted(1, 1, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(255, 255, 255, kGlassLightAlpha));
    p.drawLine(box.left() + 1, box.top() + 1, box.right() - 1, box.top() + 1);
    p.setPen(QColor(0, 0, 0, 90));
    p.drawLine(box.left() + 1, box.top() + 2, box.right() - 1, box.top() + 2);
    p.restore();
}

// From Longpath src/gui/StyleConstants.h:697-703 [@551576e3]
QString shiftL(const QColor& base, int deltaL)
{
    QColor c = base.toHsl();
    const int l = std::clamp(c.lightness() + deltaL, 0, 255);
    c.setHsl(c.hslHue(), c.hslSaturation(), l, c.alpha());
    return c.name();
}

// From Longpath src/gui/StyleConstants.h:949-954 [@551576e3]
QString glassPanelFill()
{
    return QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
        .arg(role("panel-top", kGlassPanelTop), role("panel-bottom", kGlassPanelBot));
}

// From Longpath src/gui/StyleConstants.h:1052-1066 [@551576e3]
QString titleBarStyle()
{
    // Die Kopfleiste soll AUFLIEGEN, nicht eingelassen sein: etwas
    // kraeftiger als die Platte darunter, und eine dunkle Unterkante,
    // die sie von ihr abhebt. Die Toene stehen weiter in der Palette
    // (kTitleGrad*) — hier ist nur der Schluss dunkler geworden.
    // Glas & Tiefe (2026-09-17): dazu eine Lichtkante am oberen Rand,
    // damit der Kopf eine Oberflaeche hat und nicht nur einen Verlauf.
    return QStringLiteral(
        "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        " stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4; border-top: 1px solid %5;"
    ).arg(QLatin1String(kTitleGradTop), QLatin1String(kTitleGradMid),
          QLatin1String(kTitleGradBot), QLatin1String(kTitleBorder),
          QLatin1String(kGlassLight));
}

} // namespace NereusSDR::Workspace
