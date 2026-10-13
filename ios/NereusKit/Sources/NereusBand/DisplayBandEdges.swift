// Ported from src/models/Band.cpp:123–149 and its cited Thetis sources.
// Modification history (NereusSDR):
// 2026-10-12 — Swift identity-range consumer by J.J. Boyd (KG4VCF),
// AI-assisted transformation via OpenAI Codex. No regulatory band plan.
// =================================================================
// src/models/Band.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/clsBandStackManager.cs (the 2 m band edges),
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-28 - 2 m as its own band (Band2m, 144 to 148 MHz, "2m"),
//                 R-IOS-26 / R-R3-49. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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


import NereusMedia

/// The existing native union of amateur identity boundaries, in hertz.
/// No station protocol field or regional permission is inferred from it.
enum DisplayBandEdges {
    // Band edges in Hz. IARU Region 2 covers the widest allocations; we use
    // the union so European/Australian frequencies also round-trip correctly.
    // Source: ARRL band plan / IARU Region 2, cross-checked against Thetis
    // BandStackManager HF definitions.
    // From src/models/Band.cpp:131–149; constants unchanged.
    static let ranges: [ClosedRange<Double>] = [
        1800000.0...2000000.0,
        3500000.0...4000000.0,
        5330000.0...5410000.0,  // US channelized block
        7000000.0...7300000.0,
        10100000.0...10150000.0,
        14000000.0...14350000.0,
        18068000.0...18168000.0,
        21000000.0...21450000.0,
        24890000.0...24990000.0,
        28000000.0...29700000.0,
        50000000.0...54000000.0,
        // From Thetis clsBandStackManager.cs:1330 [v2.10.3.15]:
        //   frequencyData.Add(new BandFrequencyData(144.0, 148.0, Band.B2M, BandType.VHF, false, region));
        // The same 144.0 to 148.0 MHz row is in every region's table
        // (clsBandStackManager.cs:1346-1729), and the lookup is inclusive at
        // both ends (clsBandStackManager.cs:1240, freq >= low && freq <= high).
        144000000.0...148000000.0,
    ]

    static func frequencies(in coverage: BandCoverage) -> [Double] {
        guard coverage.centerHz.isFinite, coverage.spanHz.isFinite, coverage.spanHz > 0 else { return [] }
        return ranges.flatMap { [$0.lowerBound, $0.upperBound] }
            .filter { $0 >= coverage.lowHz && $0 <= coverage.highHz }
    }
}
