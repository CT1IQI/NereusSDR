#version 440

// =================================================================
// resources/shaders/waterfall.frag  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (ten9876/AetherSDR, GPLv3) shader source:
//   resources/shaders/texturedquad.frag — ring-buffer UV-offset
//   sampling pattern (line 19 below). AetherSDR has no per-file
//   shader headers; project-level citation per
//   docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in GLSL 440 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. The texture sampler binding, std140
//                 uniform layout, and rowOffset uniform are NereusSDR
//                 additions on top of AetherSDR's UV-fract pattern.
//   2026-10-11 - The sample row is held between the centers of the newest
//                 and the oldest row, so neither the sub-row glide nor the
//                 linear filter reaches across the ring's seam; rowOffset
//                 became newestRow, glideRows and ringRows. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D tex;

layout(std140, binding = 0) uniform Uniforms {
    float newestRow;  // ring row that holds the newest waterfall row
    float glideRows;  // rows the picture has still to glide down, 1 to 0
    float ringRows;   // rows in the ring texture
    float padding3;
};

void main()
{
    // Apply ring buffer offset per-pixel (not per-vertex, to avoid fract() interpolation issue)
    // From AetherSDR texturedquad.frag
    //
    // The row to sample, counted down from the top of the newest row. The
    // glide wants one row more below the oldest than the ring holds, and the
    // linear filter half a row more at each end where a row is taller than
    // a screen pixel. Left to wrap, both took the row from the other end of
    // the ring: the bottom line of the waterfall flashed each new row as it
    // arrived, and where a row is two screen pixels tall the top line
    // carried a quarter of the oldest row. Holding the row between the
    // centers of the newest and the oldest keeps every sample inside the
    // picture; the edge lines repeat their own row instead.
    //
    // AetherSDR met the same fault once it glided too, and likewise clamps
    // its taps to the rows it holds before mapping them into the ring
    // (texturedquad.frag [@e5cd51cd]).
    float rows = max(ringRows, 1.0);
    float row = clamp(v_uv.y * rows + glideRows, 0.5, rows - 0.5);
    vec2 uv = vec2(v_uv.x, mod(newestRow + row, rows) / rows);
    fragColor = texture(tex, uv);
}
