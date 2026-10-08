// src/gui/styles/WorkspacePalette.cpp

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
// src/gui/styles/WorkspacePalette.cpp  (NereusSDR)
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
//                 Differences from Longpath's table: no #203040 row,
//                 no inset row for #0a0a18 (unreachable upstream too),
//                 no identity rows, no instrument-face or TX filter
//                 overlay rows (meter artwork and filter overlays keep
//                 their colours in NereusSDR's preview).
// =================================================================

#include "WorkspacePalette.h"

namespace NereusSDR::Workspace {

WorkspacePalette::WorkspacePalette(QVector<Entry> entries)
    : m_entries(std::move(entries))
{
    for (const Entry& e : m_entries) {
        m_byRole.insert(QString::fromLatin1(e.role), QString::fromLatin1(e.hex));
    }
}

const WorkspacePalette& WorkspacePalette::glassAndDepth()
{
    static const WorkspacePalette p({
        // Surfaces
        { "app-bg",            kAppBg },
        { "panel",             kPanelBg },
        { "panel-top",         kGlassPanelTop },
        { "panel-bottom",      kGlassPanelBot },
        { "tile-active-bg",    kTileActiveBg },
        { "panadapter-bg",     kPanadapterBg },
        { "statusbar-bg",      kStatusBarBg },
        { "statusbar-border",  kStatusBarBorder },
        { "status-sep",        kStatusSep },
        { "disabled-bg",       kDisabledBg },

        // Text
        { "text",              kTextPrimary },
        { "text-secondary",    kTextSecondary },
        { "text-tertiary",     kTextTertiary },
        { "text-scale",        kTextScale },
        { "text-inactive",     kTextInactive },
        { "text-group-head",   kTextGroupHead },
        { "label-mid",         kLabelMid },
        { "title-text",        kTitleText },
        { "disabled-text",     kDisabledText },

        // Controls and borders
        { "button",            kButtonBg },
        { "button-hover",      kButtonHover },
        { "button-alt-hover",  kButtonAltHover },
        { "raised-top",        kGlassBtnTop },
        { "raised-bottom",     kGlassBtnBot },
        { "border",            kBorder },
        { "border-subtle",     kBorderSubtle },
        { "border-muted",      kBorderMuted },
        { "edge-light",        kGlassEdgeLight },
        { "edge-dark",         kGlassEdgeDark },
        { "disabled-border",   kDisabledBorder },
        { "overlay-border",    kOverlayBorder },

        // Recessed fields
        { "inset",             kInsetBg },
        { "inset-bg",          kInsetBg },
        { "inset-top",         kInsetBgTop },
        { "inset-border",      kInsetBorder },
        { "groove",            kGroove },

        // Title bar
        { "titlebar-top",      kTitleGradTop },
        { "titlebar-mid",      kTitleGradMid },
        { "titlebar-bottom",   kTitleGradBot },
        { "titlebar-border",   kTitleBorder },

        // Meaning: each role one job
        { "accent",            kAccent },
        { "ok-bg",             kGreenBg },
        { "ok",                kGreenText },
        { "ok-border",         kGreenBorder },
        { "live",              kLiveGreen },
        { "sel-bg",            kBlueBg },
        { "sel-text",          kBlueText },
        { "sel-border",        kBlueBorder },
        { "sel-hover",         kBlueHover },
        { "selected-top",      kGlassSelTop },
        { "selected-bottom",   kGlassSelBot },
        { "selected-border",   kGlassSelBorder },
        { "selected-text",     kGlassSelText },
        { "measured-bg",       kAmberBg },
        { "measured",          kAmberText },
        { "measured-dim",      kAmberDim },
        { "measured-border",   kAmberBorder },
        { "warn",              kAmberWarn },
        { "tx",                kTxRed },
        { "danger-bg",         kRedBg },
        { "danger-text",       kRedText },
        { "danger",            kRedBorder },
        { "gauge-danger",      kGaugeDanger },

        // Badges
        { "badge-info-bg",     kBadgeInfoBg },
        { "badge-ok-bg",       kBadgeOkBg },
        { "badge-off-bg",      kBadgeOffBg },
        { "badge-warn-bg",     kBadgeWarnBg },
        { "badge-tx-bg",       kBadgeTxBg },

        // DSP toggles
        { "dsp-on-bg",         kDspToggleBg },
        { "dsp-on-border",     kDspToggleBorder },
        { "dsp-on",            kDspToggleText },

        // Spectrum chrome (not plot data)
        { "spectrum-grid",     kSpectrumGrid },
    });
    return p;
}

// From Longpath src/gui/styles/ThemeQss.cpp:36-153 [@551576e3]
//
// ── Die Tabelle ──────────────────────────────────────────────────────
//
// Links die Rolle, in der Mitte was der Quelltext hinschreibt,
// rechts was das Programm heute daraus malt. Eine Theme-Datei kann jede Rolle
// überschreiben; wo sie schweigt, gilt die rechte Spalte.
//
// Eine Konstante ohne Zeile ist eine Farbe, die kein Theme je erreicht.
// tst_theme_qss läuft über StyleConstants und schlägt fehl, wenn eine
// fehlt — das ist die einzige Absicherung dagegen.
// [original inline comment from ThemeQss.cpp:26-34; NereusSDR keeps
//  legacy hex and role here, the value comes from WorkspacePalette]
//
// The left column is what NereusSDR's retained stylesheets write; the
// right column names the role. First row wins for a duplicated key, as
// upstream (ThemeQss.cpp:331-339).
const QVector<WorkspacePalette::LegacyRow>& WorkspacePalette::legacyRows()
{
    static const QVector<LegacyRow> rows = {
        // Flächen
        { "#0f0f1a", "app-bg" },
        { "#0a0a18", "panel" },
        { "#0a0a14", "statusbar-bg" },
        { "#1a1a2a", "disabled-bg" },

        // Text
        { "#c8d8e8", "text" },
        { "#8090a0", "text-secondary" },
        { "#708090", "text-tertiary" },
        { "#607080", "text-scale" },
        { "#405060", "text-inactive" },
        { "#8899aa", "label-mid" },
        { "#8aa8c0", "title-text" },
        { "#556070", "disabled-text" },

        // Rahmen und Flächen zweiter Ordnung
        { "#1a2a3a", "button" },
        // ── #203040 war vier Konstanten auf einmal ──────────────────
        //
        // kButtonHover, kGroove, kBorderSubtle und kStatusBarBorder
        // trugen alle denselben Wert. Beim Entblauen sind sie
        // auseinandergegangen, und eine Zeile kann nur ein Ziel haben.
        //
        // Sie zeigt auf den Rahmen. Einen Rahmen sieht man immer, einen
        // Hover-Zustand nur, solange die Maus stillsteht; wenn eine der
        // Bedeutungen falsch getroffen wird, soll es die seltene sein.
        // [original inline comment from ThemeQss.cpp:57-65; NereusSDR
        //  drops the row and names the role at the call site, LT:90]
        { "#204060", "button-alt-hover" },
        { "#205070", "border" },
        { "#1e2e3e", "inset-border" },
        { "#304050", "status-sep" },
        { "#2a3040", "disabled-border" },

        // Titelleiste
        { "#3a4a5a", "titlebar-top" },
        { "#2a3a4a", "titlebar-mid" },
        { "#1a2a38", "titlebar-bottom" },
        { "#0a1a28", "titlebar-border" },

        // Bedeutung — jede Rolle genau ein Job
        { "#00b4d8", "accent" },
        { "#006040", "ok-bg" },
        { "#00ff88", "ok" },
        { "#00a060", "ok-border" },
        { "#0070c0", "sel-bg" },
        { "#0090e0", "sel-border" },
        { "#0088d8", "sel-hover" },
        { "#604000", "measured-bg" },
        // Die Grundflaeche des Panadapters. Eigene Rolle, weil ein
        // Thema sie unabhaengig vom Panel setzen koennen muss: manche
        // helle Palette will ein helles Spektrum, manche behaelt das
        // dunkle wegen des Kontrasts der Kurve.
        { "#0a0a12", "panadapter-bg" },
        { "#ffb800", "measured" },
        // Zweiter Vorgaenger derselben Rolle.
        //
        // Der Quelltext schreibt an 67 Stellen #c2924f hin — das war
        // der Wert von kAmberText, bis die Textleiter am 2026-08-20
        // auf #d8a55f angehoben wurde. In der Tabelle stand als
        // „legacy" aber nur der noch aeltere Wert #ffb800, und damit
        // blieben alle 67 Stellen im alten, dunkleren Ton stehen,
        // waehrend der Rest heller wurde.
        //
        // Eine Rolle darf mehrere Vorgaenger haben: `seen` schluesselt
        // auf den legacy-Wert, nicht auf die Rolle.
        { "#c2924f", "measured" },
        { "#906000", "measured-border" },
        { "#ddbb00", "warn" },
        { "#cc2222", "danger-bg" },
        { "#ff4444", "danger" },

        // DSP-Umschalter
        { "#1a6030", "dsp-on-bg" },
        { "#20a040", "dsp-on-border" },
        { "#80ff80", "dsp-on" },
    };
    return rows;
}

bool WorkspacePalette::contains(const QString& role) const
{
    return m_byRole.contains(role);
}

QColor WorkspacePalette::color(const QString& role) const
{
    const auto it = m_byRole.constFind(role);
    if (it == m_byRole.constEnd()) {
        return {};
    }
    return QColor(it.value());
}

QColor WorkspacePalette::color(const QString& role, const QColor& fallback) const
{
    const QColor c = color(role);
    return c.isValid() ? c : fallback;
}

QString WorkspacePalette::qss(const QString& role) const
{
    return m_byRole.value(role);
}

QString WorkspacePalette::qss(const QString& role, const QString& fallback) const
{
    const auto it = m_byRole.constFind(role);
    return it == m_byRole.constEnd() ? fallback : it.value();
}

} // namespace NereusSDR::Workspace
