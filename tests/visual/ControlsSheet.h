// no-port-check: NereusSDR-original visual preview tool; controls comparison sheet.
// =================================================================
// tests/visual/ControlsSheet.h  (NereusSDR)
// =================================================================
//
// The controls-sheet scene: the same real Qt controls, at the same sizes,
// in three looks side by side. Today is the shipped look (switch off).
// Preview now is the workspace preview as it ships (QssTranslator and
// Workspace::Components over today's stylesheets). Longpath is Longpath's
// own control stylesheets (LongpathControlStyles.h). Real NereusSDR panel
// slider rows are cropped from the main window and the TX EQ dialog in
// each look. Writes controls-sheet and controls-sheet--zoom.
//
// Modification history (NereusSDR):
//   2026-10-08 - Written for the workspace visual review. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

namespace NereusSDR::Preview {

struct SceneContext;

// Runs after the two-band window pose (runWindowScene). Leaves the app in
// today's look with the workspace preview switch off.
void sceneControlsSheet(SceneContext& ctx);

} // namespace NereusSDR::Preview
