#!/usr/bin/env python3
"""Tests for scripts/check_repo_rules.py.

The checker turns AGENTS.md's mechanical rules into a build failure, so the
repair path (BOM/line endings) and the version/changelog parsers deserve tests
of their own.

Run: python3 -m unittest discover -s scripts/tests -v
"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import check_repo_rules as rules

BOM = rules.BOM


class EncodingTest(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = Path(self._tmp.name)

    def write(self, name: str, data: bytes) -> Path:
        path = self.dir / name
        path.write_bytes(data)
        return path

    def test_fix_keeps_bom_and_normalises_line_endings(self) -> None:
        path = self.write("mixed.hpp", BOM + b"int a;\r\nint b;\n")
        report = rules.Report()
        rules.check_encoding([path], report, do_fix=True)
        data = path.read_bytes()
        self.assertTrue(data.startswith(BOM), "--fix stripped the BOM")
        self.assertEqual(data.count(b"\n"), data.count(b"\r\n"), "bare LF survived")

        after = rules.Report()
        rules.check_encoding([path], after, do_fix=False)
        self.assertEqual(after.problems, [], "one --fix run must leave the file clean")

    def test_fix_adds_bom_to_bomless_file(self) -> None:
        path = self.write("plain.hpp", b"int a;\r\n")
        rules.check_encoding([path], rules.Report(), do_fix=True)
        self.assertTrue(path.read_bytes().startswith(BOM))

    def test_check_reports_missing_bom_and_bare_lf(self) -> None:
        path = self.write("plain.hpp", b"int a;\nint b;\r\n")
        report = rules.Report()
        rules.check_encoding([path], report, do_fix=False)
        messages = " ".join(report.problems)
        self.assertIn("missing UTF-8 BOM", messages)
        self.assertIn("LF line ending", messages)


class LineEndingTest(unittest.TestCase):
    def test_style(self) -> None:
        self.assertEqual(rules.eol_style(b"a\r\nb\r\n"), "crlf")
        self.assertEqual(rules.eol_style(b"a\nb\n"), "lf")
        self.assertEqual(rules.eol_style(b"a\r\nb\n"), "mixed")

    def test_fix_markdown_to_crlf(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "note.md"
            path.write_bytes(b"# Title\n\ntext\n")
            rules.check_line_endings([path], rules.Report(), do_fix=True)
            self.assertNotIn(b"\n", path.read_bytes().replace(b"\r\n", b""))


class ChangelogTest(unittest.TestCase):
    def test_heading_without_definition_is_reported(self) -> None:
        text = "## [Unreleased]\n\n## [1.0.0] - 2026-01-01\n\n[Unreleased]: https://example.com\n"
        self.assertEqual(rules.missing_changelog_definitions(text), ["1.0.0"])

    def test_every_heading_defined(self) -> None:
        text = "## [Unreleased]\n\n[Unreleased]: https://example.com\n"
        self.assertEqual(rules.missing_changelog_definitions(text), [])


class VersionParserTest(unittest.TestCase):
    def test_cmake(self) -> None:
        self.assertEqual(
            rules.parse_cmake_version('project(ShellExtContextMenuHandler VERSION 1.2.3 LANGUAGES CXX)'),
            (1, 2, 3),
        )

    def test_header(self) -> None:
        text = "#define SHELLEXT_VERSION_MAJOR 1\n#define SHELLEXT_VERSION_MINOR 2\n#define SHELLEXT_VERSION_PATCH 3\n"
        self.assertEqual(rules.parse_header_version(text), (1, 2, 3))

    def test_rc(self) -> None:
        self.assertEqual(rules.parse_rc_version(" FILEVERSION 1,2,3,0\n"), (1, 2, 3))


class HeaderTest(unittest.TestCase):
    def test_first_significant_line_skips_comments(self) -> None:
        lines = ["/* header", " * comment", " */", "", "#pragma once", ""]
        self.assertEqual(rules.first_significant_line(lines), "#pragma once")


class MarkdownLinkTest(unittest.TestCase):
    def test_links_skip_fenced_code(self) -> None:
        text = "```\n[no](missing.md)\n```\n[yes](README.md)\n"
        links = rules.markdown_links(text)
        self.assertEqual(len(links), 1)
        self.assertEqual(links[0][2], "README.md")


class SkipTest(unittest.TestCase):
    def test_build_directories_are_skipped(self) -> None:
        self.assertTrue(rules.skipped(Path("build/src/a.cpp")))
        self.assertTrue(rules.skipped(Path("cmake-build-verify/src/a.cpp")))
        self.assertFalse(rules.skipped(Path("src/build_and_run.cpp")))


if __name__ == "__main__":
    unittest.main()
