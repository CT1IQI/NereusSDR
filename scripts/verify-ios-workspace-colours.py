#!/usr/bin/env python3
"""The phone's chrome stays in the workspace look (R-IOS-49, W1, W2, W5).

docs/architecture/2026-10-20-phone-workspace-look-design.md, sections 6
point 4 and 9. The app's chrome colors are written only in
ios/Shared/WorkspacePalette.swift and the role files, which point at the
desktop's workspace tokens. This check scans the .swift files under
ios/NereusApp, ios/Shared and ios/NereusActivity (never Tests, UITests or
NereusKit) for hex colors written as

    rgb(0xRR, 0xGG, 0xBB)    0xRRGGBB    "#RRGGBB"    "RRGGBB"

and as SwiftUI's Color(red: 0xRR / 255, green: 0xGG / 255, blue: 0xBB / 255)
or UIKit's UIColor(red:green:blue:alpha:) (each part in hex or decimal, over
255 or 255.0, a bare 0 or 1, or a fraction such as 0.12), and fails,
printing one "FAIL <file>:<line>: <hex>" line per finding, on any value in
NAVY (the old navy chrome) unless KEPT lists that file and value with a
reason.

It also refuses what draws the system blue in a workspace screen (W6):
Color.accentColor, and the stock bordered button styles
(.buttonStyle(.bordered), .buttonStyle(.borderedProminent) and their
BorderedButtonStyle() and BorderedProminentButtonStyle() forms), printing
"FAIL <file>:<line>: <what>" for each. Data colors (band plan, slice colors, spectrum and
waterfall, meter zones, the cyan frequency readout and the rest of spec W2)
are kept; the ones that happen to share a value with the old chrome are
listed in KEPT.

It also refuses a corner radius written as a number (W11: buttons, chips
and fields round at WorkspacePalette.chipRadius, 7; panels at panelRadius,
10): cornerRadius: 4, .cornerRadius(4), cornerRadius = 4, and a numeric
radius: given to WorkspaceFace, WorkspaceButtonStyle, workspacePanel or
workspaceInset, printing "FAIL <file>:<line>: cornerRadius <n>". The
instruments that keep their own corners (meter bars and gauges, Canvas
meter drawings, slice color swatches, the app icon clip, the Live
Activity's 24 point card, the VFO flag, the spectrum's spot labels) are
listed in KEPT_RADII by file and value, with how many times the value
may appear there and the reason.

Usage:
  python3 scripts/verify-ios-workspace-colours.py [--root REPO_ROOT]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple

REPO = Path(__file__).resolve().parent.parent

SCANNED = ("ios/NereusApp", "ios/Shared", "ios/NereusActivity")
SKIPPED_PARTS = {"Tests", "UITests", "NereusKit"}

# The old chrome: every key of MAP in
# docs/architecture/2026-10-20-phone-workspace-look/make_board.py:51-89,
# copied here (not imported), then #0a0a14, #0f0f1a, #00b4d8, #0070c0,
# #ff4444 and #cc2222 named again for clarity.
NAVY = frozenset({
    # surfaces
    "#0f0f1a", "#0b0b13", "#0a0a18", "#0a0a14", "#1a1a2a", "#07070d", "#10101c",
    "#0d1b28", "#0f1a26", "#0f1420", "#131326", "#1b1d26", "#1c1a24", "#2c303c",
    "#1c1c1e",
    # text
    "#c8d8e8", "#e4eef8", "#8090a0", "#708090", "#607080", "#405060", "#8899aa",
    "#6888a0", "#8aa8c0", "#556070", "#a8b8c8", "#a0b4c4", "#404858",
    # buttons and borders
    "#1a2a3a", "#204060", "#205070", "#203040", "#304050", "#1e2e3e", "#2a3040",
    "#2a4a60", "#305070", "#2a3a4a", "#2a3a4c", "#233244", "#1a3a5a",
    # title gradient
    "#3a4a5a", "#1a2a38", "#0a1a28",
    # chrome accent and selection
    "#00b4d8", "#0070c0", "#0090e0", "#0088d8", "#0080d0", "#1a5ca8", "#0a84ff",
    "#5fa8ff", "#3a7cc8",
    # confirmed and live (green)
    "#006040", "#00ff88", "#00a060", "#1a6030", "#20a040", "#80ff80", "#1a5030",
    "#006030", "#008040", "#20603a", "#00e060", "#00d860", "#39c167", "#5fff8a",
    "#3fcf6a", "#a0d8a0",
    # measured and warning (amber)
    "#604000", "#906000", "#ffb800", "#ffd700", "#ddbb00", "#ffc040", "#3a3010",
    "#a07000", "#8a6000", "#d39c2a", "#ff9f0a",
    # transmit and danger (red)
    "#ff4444", "#cc2222", "#ff8080", "#6a3030", "#4a2020", "#c8a8a8", "#c14848",
    "#ff6b6b", "#ff5050", "#ff8a8a", "#ff6060", "#3a1c20", "#2a1014", "#a07078",
    "#6a4048", "#ffd0d0", "#ffb0b0", "#d09098", "#b07880", "#ff3b30",
    # named again for clarity
    "#0a0a14", "#0f0f1a", "#00b4d8", "#0070c0", "#ff4444", "#cc2222",
})

# Data colors kept on purpose (spec W2) that share a value with the old
# chrome: (path under the repository root, hex) -> reason.
_GRAPH = "connection graph series color (spec W2)"
KEPT: Dict[Tuple[str, str], str] = {
    # The rotor dial's instrument: heading amber, target cyan, arrival green, end stops.
    ("ios/NereusApp/Accessories/RotorDial.swift", "#ffb800"): "rotor instrument: the heading amber",
    ("ios/NereusApp/Accessories/RotorDial.swift", "#00b4d8"): "rotor instrument: the target cyan",
    ("ios/NereusApp/Accessories/RotorDial.swift", "#5fff8a"): "rotor instrument: arrived on the target",
    ("ios/NereusApp/Accessories/RotorDial.swift", "#c14848"): "rotor instrument: the end-stop marks",
    # The modulation monitor's scope trace.
    ("ios/NereusApp/Main/ModMonitorView.swift", "#00ff88"): "modulation scope: the trace",
    # Spot source colors.
    ("ios/NereusApp/Spots/SpotColours.swift", "#5fa8ff"): "spot source color: the Spot List's source tag",
    ("ios/NereusApp/Spots/SpotsModel.swift", "#00b4d8"): "spot source color: a spot nothing else colors",
    # The TX EQ plot's curve series.
    ("ios/NereusApp/Tools/TxEqCurveParts.swift", "#00b4d8"): "TX EQ plot series: the curve and its chosen mark",
    # The Classic S-meter face's scale colors (the desktop's).
    ("ios/NereusApp/iPad/ClassicMeterFace.swift", "#ff4444"): "meter scale color: the scale's red zone",
    ("ios/NereusApp/iPad/ClassicMeterFace.swift", "#0080d0"): "meter scale color: the TX scale's blue",
    ("ios/NereusApp/iPad/ClassicMeterFace.swift", "#00b4d8"): "meter scale color: the reading",
    # A slice's color when the Core's cannot be read (BandSlice.colourUnknown).
    ("ios/Shared/Activity/ActivityColours.swift", "#8090a0"): "slice color: the unknown slice's grey",
    # The connection graphs' series (ConnectionPerformanceCatalog.charts).
    ("ios/NereusApp/Tools/ConnectionPerformanceCatalog.swift", "#00b4d8"): _GRAPH,
    ("ios/NereusApp/Tools/ConnectionPerformanceCatalog.swift", "#5fff8a"): _GRAPH,
    ("ios/NereusApp/Tools/ConnectionPerformanceCatalog.swift", "#ffd700"): _GRAPH,
    ("ios/NereusApp/Tools/ConnectionPerformanceCatalog.swift", "#5fa8ff"): _GRAPH,
    ("ios/NereusApp/Tools/ConnectionPerformanceCatalog.swift", "#ff6060"): _GRAPH,
}

_METER = "meter bar or gauge (W11: instruments keep their own corners)"
_CANVAS = "Canvas meter drawing (W11)"
_SWATCH = "slice color swatch (W11)"
_ICON = "the app icon clip (W11)"
_FLAG = "the VFO flag keeps its own shape (W11)"
# Corners kept on purpose: (path under the repository root, the number as
# written) -> (how many times it may appear in that file, reason).
KEPT_RADII: Dict[Tuple[str, str], Tuple[int, str]] = {
    ("ios/NereusApp/Accessories/AccessoryChrome.swift", "2"): (3, _METER),
    ("ios/NereusApp/Main/KeyedMeterGauge.swift", "2"): (2, _METER),
    ("ios/NereusApp/Main/LinearGauge.swift", "2"): (2, _METER),
    ("ios/NereusApp/Main/TxStageMeters.swift", "2"): (2, _METER),
    ("ios/NereusApp/Modes/TransmitSection.swift", "3"): (2, "the CFC bars' greyed meter place (W11)"),
    ("ios/NereusApp/Tools/PureSignalPage.swift", "3"): (2, _METER),
    ("ios/NereusApp/Tools/VaxAudioPage.swift", "3"): (3, _METER),
    ("ios/NereusApp/Main/ModMonitorView.swift", "2"): (3, _CANVAS),
    ("ios/NereusApp/Main/ModMonitorView.swift", "6"): (1, _CANVAS),
    ("ios/NereusApp/Main/PanelGlyph.swift", "2"): (1, "a glyph drawn in a Canvas (W11)"),
    ("ios/NereusApp/Dial/Knob.swift", "2"): (1, "the knob's index mark, an instrument (W11)"),
    ("ios/NereusApp/Dial/Thumbwheel.swift", "6"): (3, "the drawn thumbwheel, an instrument like the knob (W11)"),
    ("ios/NereusApp/Tools/TxEqCurveParts.swift", "4"): (1, "a label drawn inside the TX EQ plot (W11)"),
    ("ios/NereusApp/Spots/SpotLayer.swift", "3"): (2, "spot labels on the spectrum (W11: spectrum drawing)"),
    ("ios/NereusApp/Band/SliceLetterBadge.swift", "3"): (1, _SWATCH),
    ("ios/NereusApp/Main/SliceListSheet.swift", "2"): (1, _SWATCH),
    ("ios/NereusApp/Setup/DevicesPage.swift", "2"): (1, _SWATCH),
    ("ios/Shared/Activity/ActivityBadge.swift", "3"): (1, _SWATCH),
    ("ios/NereusApp/Connect/WelcomeScreen.swift", "15"): (1, _ICON),
    ("ios/NereusApp/Setup/AboutAppPage.swift", "10"): (1, _ICON),
    ("ios/Shared/Activity/ActivityAppIcon.swift", "6"): (1, _ICON),
    ("ios/NereusApp/Connect/WelcomeScreen.swift", "3"): (2, "the station picture, an illustration (W11)"),
    ("ios/NereusApp/Connect/WelcomeScreen.swift", "5"): (1, "the station picture, an illustration (W11)"),
    ("ios/Shared/Activity/ActivityCard.swift", "24"): (2, "the Live Activity card's 24 point corner is Apple's (spec 8)"),
    ("ios/NereusApp/Band/VfoFlagView.swift", "4"): (4, _FLAG),
    ("ios/NereusApp/Band/VfoFlagView.swift", "3"): (2, _FLAG),
    ("ios/NereusApp/Band/VfoFlagView.swift", "6"): (1, _FLAG),
    ("ios/NereusApp/Band/FoldedTagView.swift", "4"): (2, _FLAG),
    ("ios/NereusApp/Band/JumpViews.swift", "4"): (4, "the edge markers, drawn as the folded flag (W11)"),
}

# A corner radius written as a number.
RADIUS_RE = re.compile(
    r"\bcornerRadius\s*(?::|\(|=)\s*(\d+(?:\.\d+)?)\b"
    r"|\b(?:WorkspaceFace|WorkspaceButtonStyle|workspacePanel|workspaceInset)\([^)]*?\bradius:\s*(\d+(?:\.\d+)?)\b"
)

HEX_RE = re.compile(
    r"rgb\(\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*\)"
    r"|\b0x([0-9A-Fa-f]{6})\b"
    r'|"#?([0-9A-Fa-f]{6})"'
)

_PART = r"(0x[0-9A-Fa-f]{1,2}|\d{1,3}(?:\.\d+)?|\.\d+)(\s*/\s*255(?:\.0)?)?"
COLOR_RE = re.compile(
    r"\b(?:UI)?Color\(\s*(?:\.sRGB\s*,\s*)?red:\s*" + _PART
    + r"\s*,\s*green:\s*" + _PART + r"\s*,\s*blue:\s*" + _PART + r"\s*[,)]"
)

# What draws the system blue (W6): the system accent and the stock bordered buttons.
SYSTEM_RE = re.compile(
    r"\bColor\.accentColor\b"
    r"|\.buttonStyle\(\s*\.(?:bordered|borderedProminent)\s*\)"
    r"|\bBordered(?:Prominent)?ButtonStyle\(\s*\)"
)


def _part(number: str, over: str) -> int:
    """One Color(red:green:blue:) part as 0 to 255, or -1 when it is not one.

    Over 255 the number is the channel; alone it is a fraction of 1 (0, 1,
    0.12), rounded to the nearest channel as the color would draw it."""
    if number.lower().startswith("0x"):
        value = int(number, 16)
        if not over:
            value = {0: 0, 1: 255}.get(value, -1)
    elif over:
        value = int(float(number)) if float(number).is_integer() else -1
    else:
        fraction = float(number)
        value = int(fraction * 255 + 0.5) if 0 <= fraction <= 1 else -1
    return value if 0 <= value <= 255 else -1


def colours(line: str) -> List[str]:
    """The hex colors written on a line, as lowercase "#rrggbb"."""
    found: List[str] = []
    for match in HEX_RE.finditer(line):
        if match.group(1):
            value = match.group(1) + match.group(2) + match.group(3)
        else:
            value = match.group(4) or match.group(5)
        found.append("#" + value.lower())
    for match in COLOR_RE.finditer(line):
        parts = [_part(match.group(index), match.group(index + 1)) for index in (1, 3, 5)]
        if min(parts) >= 0:
            found.append("#" + "".join("%02x" % part for part in parts))
    return found


def system_blue(line: str) -> List[str]:
    """What on a line draws the system blue: Color.accentColor or a bordered button style."""
    return [re.sub(r"\s+", "", match.group(0)) for match in SYSTEM_RE.finditer(line)]


def scanned_files(root: Path) -> List[Path]:
    files: List[Path] = []
    for folder in SCANNED:
        base = root / folder
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*.swift")):
            parts = set(path.relative_to(root).parts)
            if parts & SKIPPED_PARTS:
                continue
            if any(part.startswith(".") for part in path.relative_to(root).parts):
                continue
            files.append(path)
    return files


def radii(line: str) -> List[str]:
    """The corner radii written as numbers on a line, as written ("4", "2.5")."""
    return [match.group(1) or match.group(2) for match in RADIUS_RE.finditer(line)]


def check(root: Path, kept: Dict[Tuple[str, str], str] = KEPT,
          kept_radii: Dict[Tuple[str, str], Tuple[int, str]] = KEPT_RADII) -> List[str]:
    """One "FAIL <file>:<line>: <hex>" line per navy chrome color, one
    "FAIL <file>:<line>: <what>" line per use of the system blue, and one
    "FAIL <file>:<line>: cornerRadius <n>" line per corner written as a
    number that KEPT_RADII does not cover. When a file holds more of a kept
    value than its entry allows, every one of them is reported."""
    findings: List[str] = []
    for path in scanned_files(root):
        shown = path.relative_to(root).as_posix()
        lines = path.read_text(encoding="utf-8").splitlines()
        corners: Dict[str, List[int]] = {}
        for number, line in enumerate(lines, 1):
            for value in colours(line):
                if value in NAVY and (shown, value) not in kept:
                    findings.append("FAIL " + shown + ":" + str(number) + ": " + value)
            for what in system_blue(line):
                findings.append("FAIL " + shown + ":" + str(number) + ": " + what)
            for value in radii(line):
                corners.setdefault(value, []).append(number)
        for value, numbers in corners.items():
            allowed = kept_radii.get((shown, value), (0, ""))[0]
            if len(numbers) > allowed:
                for number in numbers:
                    findings.append("FAIL " + shown + ":" + str(number) + ": cornerRadius " + value)
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=REPO,
                        help="repository root (default: this script's repository)")
    args = parser.parse_args()
    findings = check(args.root.resolve())
    for finding in findings:
        print(finding)
    if findings:
        print("verify-ios-workspace-colours: " + str(len(findings))
              + " finding(s); move a navy color to a role or token, or list a data"
              " color in KEPT with its reason; draw a button in a workspace style and"
              " a mark in a WorkspacePalette token, never the system blue; round a"
              " control at WorkspacePalette.chipRadius and a panel at panelRadius, or"
              " list an instrument's corner in KEPT_RADII with its reason")
        return 1
    print("ios/ chrome is in the workspace look")
    return 0


if __name__ == "__main__":
    sys.exit(main())
