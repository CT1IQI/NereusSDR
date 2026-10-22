#!/usr/bin/env python3
"""The phone's workspace tokens against the desktop's (R-IOS-49, R-IOS-50).

ios/Shared/WorkspacePalette.swift carries the desktop's workspace tokens by
the desktop's names, one per line in the form

    static let <name> = rgb(0xRRGGBB) // k<Name>, WorkspacePalette.h:<line>; From Longpath ...

(docs/architecture/2026-10-20-phone-workspace-look-design.md, sections 4 and
11). This check reads src/gui/styles/WorkspacePalette.h
(constexpr auto k<Name> = "#rrggbb";) and fails when a Swift token's hex
differs from the header's value for the k<Name> it names, when its
Longpath cite differs from the header's (on the token's line or the line
above it), or when one of the desktop tokens the phone
carries is missing.
"""

from __future__ import annotations

import re
import shutil
import tempfile
import unittest
from pathlib import Path
from typing import Dict, List, Tuple

PROJECT = Path(__file__).resolve().parents[2]
HEADER = PROJECT / "src" / "gui" / "styles" / "WorkspacePalette.h"
SWIFT = PROJECT / "ios" / "Shared" / "WorkspacePalette.swift"

# The desktop tokens the phone carries, by the phone's names.
TOKENS = (
    "appBg", "panelBg", "statusBarBg", "statusBarBorder",
    "textPrimary", "textSecondary", "textTertiary", "textScale", "textInactive",
    "titleText", "border", "borderSubtle", "borderMuted", "insetBg", "insetBorder",
    "greenBg", "liveGreen", "amberBg", "amberText", "amberBorder", "dspToggleBorder",
    "badgeTxBg", "txRed", "redBg", "redText", "redBorder", "gaugeDanger",
    "glassPanelTop", "glassPanelBot", "glassBtnTop", "glassBtnBot", "glassSelTop",
    "glassSelBot", "glassSelBorder", "glassSelText",
)

HEADER_RE = re.compile(r'^constexpr auto (k[A-Za-z0-9]+)\s*=\s*"#([0-9a-fA-F]{6})";')
CITE_RE = re.compile(r"From Longpath src/gui/StyleConstants\.h:\d+ \[@551576e3\]")
SWIFT_RE = re.compile(
    r"^\s*static let ([a-z][A-Za-z0-9]*) = rgb\(0x([0-9A-Fa-f]{6})\) "
    r"// (k[A-Za-z0-9]+), WorkspacePalette\.h:(\d+); (.+)$"
)


def desktop_name(name: str) -> str:
    return "k" + name[0].upper() + name[1:]


def read_header(path: Path) -> Dict[str, Tuple[str, int, str]]:
    """k<Name> -> (hex, line, Longpath cite on the line or the line above)."""
    lines = path.read_text(encoding="utf-8").splitlines()
    found: Dict[str, Tuple[str, int, str]] = {}
    for index, line in enumerate(lines):
        match = HEADER_RE.match(line)
        if not match:
            continue
        cite = CITE_RE.search(line)
        if cite is None and index > 0:
            cite = CITE_RE.search(lines[index - 1])
        found[match.group(1)] = (match.group(2).lower(), index + 1,
                                 cite.group(0) if cite else "")
    return found


def check(header: Path, swift: Path) -> List[str]:
    """One line per drift between the phone's token file and the desktop's."""
    desktop = read_header(header)
    findings: List[str] = []
    seen = set()
    for number, line in enumerate(swift.read_text(encoding="utf-8").splitlines(), 1):
        match = SWIFT_RE.match(line)
        if not match:
            continue
        name, value, k_name, _, cite = match.groups()
        shown = swift.name + ":" + str(number)
        seen.add(name)
        if k_name != desktop_name(name):
            findings.append(shown + ": " + name + " names " + k_name
                            + ", expected " + desktop_name(name))
            continue
        if k_name not in desktop:
            findings.append(shown + ": " + k_name + " is not in WorkspacePalette.h")
            continue
        d_value, _, d_cite = desktop[k_name]
        if value.lower() != d_value:
            findings.append(shown + ": " + name + " is #" + value.lower()
                            + ", the desktop's " + k_name + " is #" + d_value)
        if cite.strip() != d_cite:
            findings.append(shown + ": " + name + " cites '" + cite.strip()
                            + "', the desktop's " + k_name + " cites '" + d_cite + "'")
    for name in TOKENS:
        if name not in seen:
            findings.append(swift.name + ": " + name + " (" + desktop_name(name)
                            + ") is missing")
    return findings


class WorkspacePaletteDriftTest(unittest.TestCase):
    def test_every_desktop_token_is_carried(self) -> None:
        self.assertEqual(len(TOKENS), 35)
        desktop = read_header(HEADER)
        for name in TOKENS:
            with self.subTest(name=name):
                self.assertIn(desktop_name(name), desktop)

    def test_phone_tokens_equal_the_desktops(self) -> None:
        self.assertEqual(check(HEADER, SWIFT), [])

    def test_changed_hex_fails(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            copy = Path(folder) / SWIFT.name
            shutil.copyfile(SWIFT, copy)
            text = copy.read_text(encoding="utf-8")
            line = "static let txRed = rgb(0xC25A5C)"
            self.assertIn(line, text)
            copy.write_text(text.replace(line, "static let txRed = rgb(0xC25A5D)"),
                            encoding="utf-8")
            findings = check(HEADER, copy)
            self.assertEqual(len(findings), 1, findings)
            self.assertIn("txRed is #c25a5d, the desktop's kTxRed is #c25a5c", findings[0])

    def test_missing_token_fails(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            copy = Path(folder) / SWIFT.name
            kept = [line for line in SWIFT.read_text(encoding="utf-8").splitlines(True)
                    if "static let insetBorder = " not in line]
            copy.write_text("".join(kept), encoding="utf-8")
            findings = check(HEADER, copy)
            self.assertEqual(findings, [SWIFT.name + ": insetBorder (kInsetBorder) is missing"])

    def test_changed_cite_fails(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            copy = Path(folder) / SWIFT.name
            text = SWIFT.read_text(encoding="utf-8")
            line = "// kAppBg, WorkspacePalette.h:105; From Longpath src/gui/StyleConstants.h:90 [@551576e3]"
            self.assertIn(line, text)
            copy.write_text(text.replace(line, line.replace(":90 ", ":91 ")), encoding="utf-8")
            findings = check(HEADER, copy)
            self.assertEqual(len(findings), 1, findings)
            self.assertIn("appBg cites", findings[0])


if __name__ == "__main__":
    unittest.main()
