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
// =================================================================

#pragma once

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

// Unknown colour literals met by themeSubtree() since the last reset,
// lower case, with their counts.
QHash<QString, int> unknownLiteralCounts();
void resetUnknownLiteralCounts();

} // namespace NereusSDR::WorkspacePreview
