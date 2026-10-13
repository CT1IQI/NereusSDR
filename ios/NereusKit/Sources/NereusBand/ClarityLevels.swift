// =================================================================
// src/core/spectrum/ClarityLevels.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR, GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5
//   requirements.
//
// Ported from AetherSDR src/gui/SpectrumWidget.cpp [@1e0718ad]: the
// Black Level step (0.5 dB, 50 = no offset, :11928) and the Color Gain
// span (80 dB less 0.7 dB a step, :11593). Upstream has no per-file
// copyright header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-10: Created for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 Display pill plan Task 3: the waterfall level model
//                 used while Clarity drives the levels, and the one-time
//                 conversion of Black Level and Color Gain values stored
//                 under the model before it.
// =================================================================
//
// The waterfall's Black Level and Color Gain while Clarity drives the
// levels. Clarity measures the noise floor N; the two sliders place the
// colour range against it:
//
//   black point = N + (Black - 50) * 0.5 dB     Black up = blacker
//   colour span = 80 - Gain * 0.7 dB            Gain up = more contrast
//   top         = black point + colour span
//
// NereusSDR, based on AetherSDR. AetherSDR's own comment says "higher
// black_level = more of the noise is black"
// (src/gui/SpectrumWidget.cpp:11588 [@1e0718ad]) but its formula moves
// the floor by (125 - black) * 0.4, so higher shows more noise (:11592).
// NereusSDR copied that formula and the mismatch with it. Here the
// slider does what it says, and the values stored under the old formula
// are converted once (convertLegacyBlackLevel / convertLegacyColorGain)
// so the picture does not jump on upgrade. Thetis has neither control;
// with Clarity off its Low and High levels apply unchanged.
//
// Header-only and Qt-free, so the renderer, the settings conversion and
// the tests share one copy of the arithmetic.

// Swift translation of src/core/spectrum/ClarityLevels.h:71-178.
// Original source notices and every source comment are retained above/below.
// Modification history (NereusSDR Swift):
//   2026-10-12: Translated by J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

public enum ClarityLevels {

    // ---- Black Level ----
    public static let kBlackLevelMin: Int = 0
    public static let kBlackLevelMax: Int = 100
    /// The slider position that puts the black point on the noise floor.
    public static let kBlackLevelAtNoise: Int = 50
    /// Reproduces the look before this model: N + 3.5 dB (was N + 3.4 dB).
    public static let kBlackLevelDefault: Int = 57
    // From AetherSDR src/gui/SpectrumWidget.cpp:11928 [@1e0718ad]
    //   blackThresh = m_autoBlackThresh + (50 - m_wfAutoBlackOffset) * 0.5f;
    // NereusSDR takes the step and the centre; the sign is ours (up = blacker).
    public static let kBlackLevelDbPerStep: Float = 0.5

    // ---- Color Gain ----
    public static let kColorGainMin: Int = 0
    public static let kColorGainMax: Int = 100
    /// Reproduces the look before this model: a 38 dB span (was 38.1 dB).
    public static let kColorGainDefault: Int = 60
    // From AetherSDR src/gui/SpectrumWidget.cpp:11593 [@1e0718ad]
    //   const float visRange = 80.0f - m_wfColorGain * 0.7f;  // 80 dB down to 10 dB
    public static let kColorSpanAtZeroGainDb: Float = 80.0
    public static let kColorSpanDbPerGainStep: Float = 0.7

    /// Where black starts, in dBm, for a noise floor and a Black Level.
    public static func blackPointDbm(_ noiseFloorDbm: Float, _ blackLevel: Int) -> Float {
        noiseFloorDbm + Float(blackLevel - kBlackLevelAtNoise) * kBlackLevelDbPerStep
    }

    /// How many dB the colours cover above the black point.
    public static func colorSpanDb(_ colorGain: Int) -> Float {
        kColorSpanAtZeroGainDb - Float(colorGain) * kColorSpanDbPerGainStep
    }

    // ---- The model these values were stored under ----
    //
    // Before this model the renderer applied both sliders to Clarity's pair
    // (low = N - 5 dB, high = N + 55 dB, ClarityController's margins):
    //
    //   black point = N - 5 + 0.4 * (125 - oldBlack)
    //   top         = N + 55 - 0.3 * oldGain
    //
    // so the span depended on both sliders.
    public static let kLegacyBlackLevelMax: Int = 125
    public static let kLegacyBlackLevelDefault: Int = 104
    public static let kLegacyColorGainMax: Int = 100
    public static let kLegacyColorGainDefault: Int = 45
    // From AetherSDR src/gui/SpectrumWidget.cpp:11592 [@1e0718ad]
    //   const float floorShift = (125 - m_wfBlackLevel) * 0.4f;  // inverted: 0=max shift, 125=no shift
    public static let kLegacyBlackDbPerStep: Double = 0.4
    /// NereusSDR-original: dB the old Color Gain took off the top, per step.
    public static let kLegacyGainDbPerStep: Double = 0.3
    /// ClarityController's low and high margins around the noise floor.
    public static let kLegacyClarityLowMarginDb: Double = -5.0
    public static let kLegacyClarityHighMarginDb: Double = 55.0

    /// dB above the noise floor where an old Black Level put the black point.
    public static func legacyBlackOffsetDb(_ oldBlack: Int) -> Double {
        let black = min(max(oldBlack, 0), kLegacyBlackLevelMax)
        return kLegacyClarityLowMarginDb + kLegacyBlackDbPerStep * Double(kLegacyBlackLevelMax - black)
    }

    /// dB between the black point and the top for an old Gain and Black.
    public static func legacyColorSpanDb(_ oldGain: Int, _ oldBlack: Int) -> Double {
        let gain = min(max(oldGain, 0), kLegacyColorGainMax)
        return kLegacyClarityHighMarginDb - kLegacyGainDbPerStep * Double(gain) - legacyBlackOffsetDb(oldBlack)
    }

    /// The Black Level that keeps an old Black Level's black point: the
    /// nearest slider position, clamped to the slider's ends.
    public static func convertLegacyBlackLevel(_ oldBlack: Int) -> Int {
        let steps = legacyBlackOffsetDb(oldBlack) / Double(kBlackLevelDbPerStep)
        let level = Int((Double(kBlackLevelAtNoise) + steps).rounded(.toNearestOrAwayFromZero))
        return min(max(level, kBlackLevelMin), kBlackLevelMax)
    }

    /// The Color Gain that keeps the old span. The old span depended on the
    /// Black Level too, so the pan's own old Black Level is needed.
    public static func convertLegacyColorGain(_ oldGain: Int, _ oldBlack: Int) -> Int {
        let steps = (Double(kColorSpanAtZeroGainDb) - legacyColorSpanDb(oldGain, oldBlack))
            / Double(kColorSpanDbPerGainStep)
        let gain = Int(steps.rounded(.toNearestOrAwayFromZero))
        return min(max(gain, kColorGainMin), kColorGainMax)
    }

}
