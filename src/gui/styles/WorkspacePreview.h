// =================================================================
// src/gui/styles/WorkspacePreview.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The workspace preview switch and
// the scoped stylesheet pass used to preview the Longpath look.
//
// The switch is on when the environment variable
// NEREUS_WORKSPACE_PREVIEW is "1" at the first call to enabled(), or
// after setEnabledForTest(true). There is no settings key and no UI.
// Every visual change of the workspace look branches on enabled(); with
// the switch off the application is unchanged.
//
// themeSubtree() is preview scaffolding, not the final migration: it
// rewrites the retained stylesheets of a widget subtree through
// QssTranslator. The final design migrates call sites one by one (LT
// "Theme interfaces and migration").
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - passInputThrough(): the preview's overlays let clicks
//                 through when they are native views, by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - installLiveTheming(): one application-wide pass that
//                 themes every widget when it is shown, so no window,
//                 page, menu or popup needs a call site. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - roleColour() and roleColourName(): the one helper for
//                 chrome colours that never pass through a stylesheet.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QColor>
#include <QHash>
#include <QString>

class QApplication;
class QWidget;

namespace NereusSDR::WorkspacePreview {

bool enabled();
void setEnabledForTest(bool on);

// Dynamic property names used by themeSubtree().
inline constexpr auto kOriginalQssProperty = "_nereusWorkspaceOriginalQss";
inline constexpr auto kThemedQssProperty = "_nereusWorkspaceThemedQss";
// Set to true on a widget to keep the scoped pass out of its subtree.
inline constexpr auto kSkipProperty = "nereusWorkspaceSkip";

// Rewrites every stylesheet under `root` (root included) from its
// retained original. The first time a widget is seen, or when its
// stylesheet changed since the last pass, the current stylesheet is
// kept as the original. Skips the slice flag (VfoWidget and its helper
// widgets), SpectrumWidget's subtree, colour pickers that show a user
// colour, and anything carrying kSkipProperty. Does nothing when the
// switch is off.
void themeSubtree(QWidget* root);

// Installs a filter that runs themeSubtree(dialog) each time the dialog
// is shown. Does nothing when the switch is off.
void themeOnShow(QWidget* dialog);

// Installs the live pass on the application: every widget gets the
// scoped pass of themeSubtree() (translation and component treatment,
// the same skip list and kSkipProperty) when it is shown, and again,
// once, when a shown widget sets a new stylesheet. A window (a dialog,
// menu, popup, tooltip or toast) is its own surface: a skipped widget
// keeps the pass out of its own window only, not out of a menu or popup
// it opens. A widget whose stylesheet has not changed since its last
// pass is not translated again. Installs once; does nothing when the
// switch is off. applyWorkspacePalette() calls it.
void installLiveTheming(QApplication& app);

// The workspace look of one stylesheet: QssTranslator (with the
// context rule for #203040) followed by the component treatment
// (raised, pressed, selected, sunken). `owner` is the widget the sheet
// is for, or null. Returns `original` unchanged when the switch is off,
// so a shared style builder can call it unconditionally.
QString lookQss(const QString& original, const QWidget* owner = nullptr);

// The colour of a chrome site that never passes through a stylesheet:
// QPainter colours, Qt global colours, per-widget palettes. With the
// switch off it returns `legacy` unchanged and looks nothing up. With it
// on it returns the workspace role `role` carrying the alpha of `legacy`
// (a translucent legacy fill stays translucent), or `legacy` itself when
// the palette has no such role or `legacy` is invalid. Cheap enough to
// call on every paint: one flag test off, one hash lookup on.
QColor roleColour(const QColor& legacy, const char* role);

// roleColour() for rich text, where the colour is written into HTML.
// With the switch off it returns `legacy` exactly as written, so the
// HTML is byte for byte what it was; with it on, roleColour()'s colour
// as "#rrggbb" ("#aarrggbb" when translucent). Call it where the HTML
// is built, so a later setText() writes the role colour again.
QString roleColourName(const char* legacy, const char* role);

// Unknown colour literals met by themeSubtree() since the last reset,
// lower case, with their counts.
QHash<QString, int> unknownLiteralCounts();
void resetUnknownLiteralCounts();

// Makes a drawn-over overlay (an outline, a shadow, drop bands) let every
// click through to what is under it, also when it is a native view. A
// Metal leaf (SpectrumWidget, MeterWidget) promotes its siblings to native
// views and they promote their ancestors, so on macOS the preview's
// overlays are native child views. AppKit hit-tests native views and
// passes a click through one only when its window carries
// Qt::WindowTransparentForInput; WA_TransparentForMouseEvents alone acts
// only inside Qt's own routing. Sets both, on the widget (carried into any
// native window created or re-created later) and on an existing window.
void passInputThrough(QWidget* widget);

} // namespace NereusSDR::WorkspacePreview
