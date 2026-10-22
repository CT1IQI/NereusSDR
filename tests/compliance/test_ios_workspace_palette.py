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

WorkspaceColoursCheckTest covers scripts/verify-ios-workspace-colours.py,
the navy check: it fails on an old navy chrome color written in an app
view unless its KEPT table lists that file and value as a data color.
"""

from __future__ import annotations

import importlib.util
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Dict, List, Tuple

PROJECT = Path(__file__).resolve().parents[2]
HEADER = PROJECT / "src" / "gui" / "styles" / "WorkspacePalette.h"
SWIFT = PROJECT / "ios" / "Shared" / "WorkspacePalette.swift"
COLOURS_SCRIPT = PROJECT / "scripts" / "verify-ios-workspace-colours.py"


def load_colours_check():
    """scripts/verify-ios-workspace-colours.py as a module (its name has hyphens)."""
    spec = importlib.util.spec_from_file_location("verify_ios_workspace_colours", COLOURS_SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module

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



class WorkspaceColoursCheckTest(unittest.TestCase):
    """The navy check (scripts/verify-ios-workspace-colours.py, R-IOS-49)."""

    VIEW = "ios/NereusApp/Band/SomeView.swift"

    @classmethod
    def setUpClass(cls) -> None:
        cls.module = load_colours_check()

    def tree(self, folder: str, files: Dict[str, str]) -> Path:
        root = Path(folder)
        for name, text in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        return root

    def test_real_tree_passes(self) -> None:
        self.assertEqual(self.module.check(PROJECT), [])
        result = subprocess.run([sys.executable, str(COLOURS_SCRIPT)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ios/ chrome is in the workspace look", result.stdout)

    def test_navy_in_a_view_fails_with_file_and_line(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = self.tree(folder, {self.VIEW: "import SwiftUI\n\nlet edge = rgb(0x1A, 0x2A, 0x3A)\n"})
            self.assertEqual(self.module.check(root, kept={}),
                             ["FAIL " + self.VIEW + ":3: #1a2a3a"])
            result = subprocess.run([sys.executable, str(COLOURS_SCRIPT), "--root", str(root)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            self.assertIn("FAIL " + self.VIEW + ":3: #1a2a3a", result.stdout)

    def test_every_written_form_is_read(self) -> None:
        text = ("let a = WorkspacePalette.rgb(0x0F0F1A)\n"
                "let b = \"#00B4D8\"\n"
                "let c = Color(red: 0x30 / 255, green: 0x40 / 255, blue: 0x50 / 255)\n"
                "let d = Color(red: 15 / 255.0, green: 15 / 255.0, blue: 26 / 255.0).opacity(0.9)\n"
                "let e = Color(red: 0, green: 1, blue: 0x88 / 255)\n"
                "let f = WorkspacePalette.rgb(0x0C0C0E)\n")
        with tempfile.TemporaryDirectory() as folder:
            root = self.tree(folder, {self.VIEW: text})
            self.assertEqual(self.module.check(root, kept={}), [
                "FAIL " + self.VIEW + ":1: #0f0f1a",
                "FAIL " + self.VIEW + ":2: #00b4d8",
                "FAIL " + self.VIEW + ":3: #304050",
                "FAIL " + self.VIEW + ":4: #0f0f1a",
                "FAIL " + self.VIEW + ":5: #00ff88",
            ])

    def test_kept_file_and_value_passes(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = self.tree(folder, {self.VIEW: "let zone = rgb(0x1A, 0x2A, 0x3A)\n",
                                      "ios/NereusApp/Band/Other.swift": "let edge = rgb(0x1A, 0x2A, 0x3A)\n"})
            kept = {(self.VIEW, "#1a2a3a"): "meter zone"}
            self.assertEqual(self.module.check(root, kept=kept),
                             ["FAIL ios/NereusApp/Band/Other.swift:1: #1a2a3a"])

    def test_tests_and_kit_are_not_scanned(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = self.tree(folder, {
                "ios/NereusApp/Tests/SomeTests.swift": "let edge = rgb(0x1A, 0x2A, 0x3A)\n",
                "ios/NereusApp/UITests/SomeUITests.swift": "let edge = rgb(0x1A, 0x2A, 0x3A)\n",
                "ios/NereusKit/Sources/Kit/Some.swift": "let edge = rgb(0x1A, 0x2A, 0x3A)\n",
            })
            self.assertEqual(self.module.check(root, kept={}), [])

    def test_kept_entries_name_scanned_files_with_reasons(self) -> None:
        for (path, value), reason in self.module.KEPT.items():
            with self.subTest(path=path, value=value):
                self.assertIn(value, self.module.NAVY)
                self.assertTrue(reason.strip())
                self.assertTrue((PROJECT / path).is_file(), path)
                text = (PROJECT / path).read_text(encoding="utf-8")
                found = {v for line in text.splitlines() for v in self.module.colours(line)}
                self.assertIn(value, found, "a KEPT entry that no longer matches")


if __name__ == "__main__":
    unittest.main()
