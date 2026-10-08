// src/gui/styles/WorkspacePalette.h

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
// src/gui/styles/WorkspacePalette.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/StyleConstants.h (palette values) and
//   src/gui/styles/ThemeQss.cpp (role names and the legacy-colour table)
//   at revision 551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banners are copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   The built-in "Glass and Depth" palette: named presentation roles
//   with the Longpath values, looked up as QColor or as a stylesheet
//   string. It lives in NereusSDR::Workspace so that the symbols whose
//   names match NereusSDR::Style (kBorder, kAccent, ...) but whose values
//   differ never collide. NereusSDR's StyleConstants.h is unchanged and
//   remains the facade for existing callers.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//                 Used only behind the workspace preview switch.
// =================================================================

#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>

namespace NereusSDR::Workspace {

// ── Surfaces ─────────────────────────────────────────────────────────
constexpr auto kAppBg           = "#08080a";   // From Longpath src/gui/StyleConstants.h:90 [@551576e3]
constexpr auto kPanelBg         = "#0c0c0e";   // From Longpath src/gui/StyleConstants.h:91 [@551576e3]
constexpr auto kTileActiveBg    = "#16161c";   // From Longpath src/gui/StyleConstants.h:94 [@551576e3]
constexpr auto kPanadapterBg    = "#141e27";   // From Longpath src/gui/StyleConstants.h:219 [@551576e3]
constexpr auto kStatusBarBg     = "#0a0a0c";   // From Longpath src/gui/StyleConstants.h:640 [@551576e3]
constexpr auto kStatusBarBorder = "#1f1f23";   // From Longpath src/gui/StyleConstants.h:641 [@551576e3]
constexpr auto kStatusSep       = "#2c2c31";   // From Longpath src/gui/StyleConstants.h:642 [@551576e3]

// ── Text ─────────────────────────────────────────────────────────────
constexpr auto kTextPrimary     = "#dcdce1";   // From Longpath src/gui/StyleConstants.h:117 [@551576e3]
constexpr auto kTextSecondary   = "#a6a6ac";   // From Longpath src/gui/StyleConstants.h:118 [@551576e3]
constexpr auto kTextTertiary    = "#909096";   // From Longpath src/gui/StyleConstants.h:119 [@551576e3]
constexpr auto kTextScale       = "#7e7e85";   // From Longpath src/gui/StyleConstants.h:182 [@551576e3]
constexpr auto kTextInactive    = "#58585e";   // From Longpath src/gui/StyleConstants.h:183 [@551576e3]
constexpr auto kLabelMid        = "#828288";   // From Longpath src/gui/StyleConstants.h:186 [@551576e3]
constexpr auto kTextGroupHead   = "#5c5c60";   // From Longpath src/gui/StyleConstants.h:190 [@551576e3]
constexpr auto kAccent          = "#4a7ba8";   // From Longpath src/gui/StyleConstants.h:191 [@551576e3]
constexpr auto kTitleText       = "#c4c4ca";   // From Longpath src/gui/StyleConstants.h:192 [@551576e3]

// ── Buttons and borders ──────────────────────────────────────────────
constexpr auto kButtonBg        = "#1a1a1e";   // From Longpath src/gui/StyleConstants.h:200 [@551576e3]
constexpr auto kButtonHover     = "#26262b";   // From Longpath src/gui/StyleConstants.h:201 [@551576e3]
constexpr auto kButtonAltHover  = "#2f3138";   // From Longpath src/gui/StyleConstants.h:202 [@551576e3]
constexpr auto kBorder          = "#2c2c31";   // From Longpath src/gui/StyleConstants.h:203 [@551576e3]
constexpr auto kBorderSubtle    = "#1f1f23";   // From Longpath src/gui/StyleConstants.h:204 [@551576e3]
constexpr auto kBorderMuted     = "#3d3d41";   // From Longpath src/gui/StyleConstants.h:208 [@551576e3]

// ── Recessed fields and grooves ──────────────────────────────────────
constexpr auto kInsetBg         = "#050507";   // From Longpath src/gui/StyleConstants.h:245 [@551576e3]
constexpr auto kInsetBgTop      = "#101014";   // From Longpath src/gui/StyleConstants.h:248 [@551576e3]
constexpr auto kInsetBorder     = "#232329";   // From Longpath src/gui/StyleConstants.h:249 [@551576e3]
constexpr auto kGroove          = "#1f1f23";   // From Longpath src/gui/StyleConstants.h:250 [@551576e3]

// ── Title bar gradient ───────────────────────────────────────────────
constexpr auto kTitleGradTop    = "#26262b";   // From Longpath src/gui/StyleConstants.h:253 [@551576e3]
constexpr auto kTitleGradMid    = "#1c1c20";   // From Longpath src/gui/StyleConstants.h:254 [@551576e3]
constexpr auto kTitleGradBot    = "#141417";   // From Longpath src/gui/StyleConstants.h:255 [@551576e3]
constexpr auto kTitleBorder     = "#0d0d0f";   // From Longpath src/gui/StyleConstants.h:256 [@551576e3]

// ── Confirmed state (green) ──────────────────────────────────────────
constexpr auto kGreenBg         = "#1c3a2a";   // From Longpath src/gui/StyleConstants.h:259 [@551576e3]
constexpr auto kGreenText       = "#6fa384";   // From Longpath src/gui/StyleConstants.h:260 [@551576e3]
constexpr auto kGreenBorder     = "#2c5c44";   // From Longpath src/gui/StyleConstants.h:261 [@551576e3]
constexpr auto kLiveGreen       = "#4caf6a";   // From Longpath src/gui/StyleConstants.h:265 [@551576e3]

// ── Selection (blue) ─────────────────────────────────────────────────
constexpr auto kBlueBg          = "#3576e0";   // From Longpath src/gui/StyleConstants.h:293 [@551576e3]
constexpr auto kBlueText        = "#ffffff";   // From Longpath src/gui/StyleConstants.h:294 [@551576e3]
constexpr auto kBlueBorder      = "#2a5fbe";   // From Longpath src/gui/StyleConstants.h:295 [@551576e3]
constexpr auto kBlueHover       = "#2d5885";   // From Longpath src/gui/StyleConstants.h:296 [@551576e3]

// ── Reading and warning (amber) ──────────────────────────────────────
constexpr auto kAmberBg         = "#33280f";   // From Longpath src/gui/StyleConstants.h:297 [@551576e3]
constexpr auto kAmberText       = "#d8a55f";   // From Longpath src/gui/StyleConstants.h:298 [@551576e3]
constexpr auto kAmberDim        = "#6b5630";   // From Longpath src/gui/StyleConstants.h:304 [@551576e3]
constexpr auto kAmberBorder     = "#6b5426";   // From Longpath src/gui/StyleConstants.h:305 [@551576e3]
constexpr auto kAmberWarn       = "#a8853f";   // From Longpath src/gui/StyleConstants.h:306 [@551576e3]

// ── Status badges ────────────────────────────────────────────────────
constexpr auto kBadgeInfoBg     = "#161e27";   // From Longpath src/gui/StyleConstants.h:350 [@551576e3]
constexpr auto kBadgeOkBg       = "#212b27";   // From Longpath src/gui/StyleConstants.h:351 [@551576e3]
constexpr auto kBadgeOffBg      = "#18181a";   // From Longpath src/gui/StyleConstants.h:352 [@551576e3]
constexpr auto kBadgeWarnBg     = "#372c1d";   // From Longpath src/gui/StyleConstants.h:353 [@551576e3]
constexpr auto kBadgeTxBg       = "#3f2224";   // From Longpath src/gui/StyleConstants.h:354 [@551576e3]

// ── TX and danger (red) ──────────────────────────────────────────────
constexpr auto kTxRed           = "#c25a5c";   // From Longpath src/gui/StyleConstants.h:359 [@551576e3]
constexpr auto kRedBg           = "#7a2c2e";   // From Longpath src/gui/StyleConstants.h:361 [@551576e3]
constexpr auto kRedText         = "#f0dcdc";   // From Longpath src/gui/StyleConstants.h:362 [@551576e3]
constexpr auto kRedBorder       = "#a86b6d";   // From Longpath src/gui/StyleConstants.h:363 [@551576e3]
constexpr auto kGaugeDanger     = "#a86b6d";   // From Longpath src/gui/StyleConstants.h:506 [@551576e3]

// ── Spectrum chrome ──────────────────────────────────────────────────
constexpr auto kSpectrumGrid    = "#8a8f96";   // From Longpath src/gui/StyleConstants.h:461 [@551576e3]

// ── Glass and Depth ──────────────────────────────────────────────────
constexpr auto kGlassPanelTop   = "#17171b";   // From Longpath src/gui/StyleConstants.h:521 [@551576e3]
constexpr auto kGlassPanelBot   = "#0e0e10";   // From Longpath src/gui/StyleConstants.h:522 [@551576e3]
constexpr auto kGlassBtnTop     = "#222227";   // From Longpath src/gui/StyleConstants.h:523 [@551576e3]
constexpr auto kGlassBtnBot     = "#141417";   // From Longpath src/gui/StyleConstants.h:524 [@551576e3]
constexpr auto kGlassSelTop     = "#2f5f92";   // From Longpath src/gui/StyleConstants.h:525 [@551576e3]
constexpr auto kGlassSelBot     = "#1e3d5f";   // From Longpath src/gui/StyleConstants.h:526 [@551576e3]
constexpr auto kGlassSelBorder  = "#3d78b0";   // From Longpath src/gui/StyleConstants.h:527 [@551576e3]
constexpr auto kGlassSelText    = "#dbe9f8";   // From Longpath src/gui/StyleConstants.h:528 [@551576e3]
constexpr int  kGlassLightAlpha  = 18;         // From Longpath src/gui/StyleConstants.h:529 [@551576e3]
constexpr int  kGlassShadeAlpha  = 110;        // From Longpath src/gui/StyleConstants.h:530 [@551576e3]
constexpr auto kGlassLight       = "rgba(255, 255, 255, 18)";   // From Longpath src/gui/StyleConstants.h:531 [@551576e3]
constexpr auto kGlassShade       = "rgba(0, 0, 0, 160)";        // From Longpath src/gui/StyleConstants.h:532 [@551576e3]
constexpr int  kGlassChipRadius  = 7;          // From Longpath src/gui/StyleConstants.h:533 [@551576e3]
constexpr int  kGlassPanelRadius = 10;         // From Longpath src/gui/StyleConstants.h:534 [@551576e3]
constexpr auto kGlassEdgeLight  = "#38383e";   // From Longpath src/gui/StyleConstants.h:1109 [@551576e3]
constexpr auto kGlassEdgeDark   = "#1f1f23";   // From Longpath src/gui/StyleConstants.h:1110 [@551576e3]

// ── Disabled ─────────────────────────────────────────────────────────
constexpr auto kDisabledBg      = "#141417";   // From Longpath src/gui/StyleConstants.h:618 [@551576e3]
constexpr auto kDisabledText    = "#4e4e53";   // From Longpath src/gui/StyleConstants.h:619 [@551576e3]
constexpr auto kDisabledBorder  = "#232327";   // From Longpath src/gui/StyleConstants.h:620 [@551576e3]

// ── Overlay and DSP toggle ───────────────────────────────────────────
constexpr auto kOverlayBorder   = "#2c2c31";   // From Longpath src/gui/StyleConstants.h:626 [@551576e3]
constexpr auto kDspToggleBg     = "#1b3527";   // From Longpath src/gui/StyleConstants.h:886 [@551576e3]
constexpr auto kDspToggleBorder = "#33684c";   // From Longpath src/gui/StyleConstants.h:887 [@551576e3]
constexpr auto kDspToggleText   = "#7fae91";   // From Longpath src/gui/StyleConstants.h:888 [@551576e3]

// A named-role palette. Role names follow Longpath's ThemeQss role
// vocabulary (kebab-case, for example "border-subtle"), plus the roles
// the later instrument ports look up by name (design-facts C6).
class WorkspacePalette
{
public:
    struct Entry {
        const char* role;
        const char* hex;   // "#rrggbb", lower case
    };

    // One row of the legacy-colour table: a hex value NereusSDR's
    // retained stylesheets write, and the role it stands for.
    struct LegacyRow {
        const char* legacyHex;   // "#rrggbb", lower case
        const char* role;
    };

    explicit WorkspacePalette(QVector<Entry> entries);

    // The built-in Glass and Depth palette.
    static const WorkspacePalette& glassAndDepth();

    // Legacy hex to role, built from Longpath ThemeQss.cpp's table and
    // design-facts B10. #203040 has no row on purpose: it stood for a
    // hover, a groove, a subtle border and the status-bar border, and a
    // single row cannot recover which. Callers name its role.
    static const QVector<LegacyRow>& legacyRows();

    bool contains(const QString& role) const;
    // Invalid QColor when the role is unknown.
    QColor color(const QString& role) const;
    QColor color(const QString& role, const QColor& fallback) const;
    // "#rrggbb" for a stylesheet; empty when the role is unknown.
    QString qss(const QString& role) const;
    QString qss(const QString& role, const QString& fallback) const;

    const QVector<Entry>& entries() const { return m_entries; }

private:
    QVector<Entry> m_entries;
    QHash<QString, QString> m_byRole;
};

} // namespace NereusSDR::Workspace
