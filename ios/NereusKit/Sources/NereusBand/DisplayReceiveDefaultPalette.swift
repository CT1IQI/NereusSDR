// Ported from src/core/spectrum/WaterfallPalettes.cpp:75–85 kDefaultStops.
// AetherSDR https://github.com/ten9876/AetherSDR, Jeremy (KK7GWY) / contributors;
// its source has no per-file header (HOW-TO-PORT rule6).
// Modification history (NereusSDR):
// 2026-10-12 — Swift receive Custom seed by J.J. Boyd (KG4VCF),
// AI-assisted transformation via OpenAI Codex. Existing TX seed is retained.
// =================================================================
// src/core/spectrum/WaterfallPalettes.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// The waterfall palettes' gradient stops, moved unchanged from
// gui/SpectrumWidget.cpp (iPhone app plan Task 19, R-IOS-06) so the Core's
// catalogue can send them to an app; SpectrumWidget draws from them here.
// The Default stops come from AetherSDR (ten9876/AetherSDR, GPLv3, like
// NereusSDR); Enhanced, Spectran and BlackWhite follow Thetis's display.cs
// colour schemes; LinLog, LinRad, the Custom fallback and Clarity Blue are
// NereusSDR's. Each table keeps the comment it had in SpectrumWidget.cpp.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): moved
//                                    from gui/SpectrumWidget.cpp unchanged,
//                                    with wfSchemeName(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//


extension PaletteStop {
    // ---- Default waterfall gradient stops (AetherSDR style) ----
    // From AetherSDR SpectrumWidget.cpp:43-51
    // From src/core/spectrum/WaterfallPalettes.cpp:75–85; Float positions unchanged.
    static let receiveDefault = [
        PaletteStop(at: Double(Float(0.00)), colour: "#000000"),    // black
        PaletteStop(at: Double(Float(0.15)), colour: "#000080"),    // dark blue
        PaletteStop(at: Double(Float(0.30)), colour: "#0040FF"),    // blue
        PaletteStop(at: Double(Float(0.45)), colour: "#00C8FF"),    // cyan
        PaletteStop(at: Double(Float(0.60)), colour: "#00DC00"),    // green
        PaletteStop(at: Double(Float(0.80)), colour: "#FFFF00"),    // yellow
        PaletteStop(at: Double(Float(1.00)), colour: "#FF0000"),    // red
    ]
}
