#!/usr/bin/env python3
"""Repository rules that are worth enforcing mechanically.

The prose rules live in AGENTS.md. Anything a program can check is checked
here so a violation fails CI instead of relying on review.

Run locally:
    python3 scripts/check_repo_rules.py           # check
    python3 scripts/check_repo_rules.py --fix     # repair BOM/line endings
    python3 scripts/check_repo_rules.py --staged  # only staged files (pre-commit)
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOM = b"\xef\xbb\xbf"

CPP_SUFFIXES = {".h", ".hpp", ".hxx", ".hh", ".c", ".cc", ".cpp", ".cxx"}

SKIP_PARTS = {
    "build",
    ".git",
    "_deps",
    "third_party",
    "vcpkg_installed",
    "out",
    "x64",
    "Win32",
}
SKIP_PREFIXES = ("build-", "build_", "cmake-build")

COLUMN_LIMIT = 140
TAB_WIDTH = 4

# UTF-16LE files that are intentionally outside the UTF-8 policy.
UTF16_FILES = {"src/resources/resource.h"}
# Resource scripts are compiled by rc.exe and are not C++ text files.
RC_FILES = {"src/resources/ShellExtContextMenuHandler.rc", "src/resources/Version.rc"}

TEXT_SUFFIXES = {
    ".md",
    ".yml",
    ".yaml",
    ".py",
    ".ps1",
    ".sh",
    ".cmd",
    ".bat",
    ".cmake",
    ".in",
    ".txt",
    ".json",
    ".def",
    ".mdc",
    ".editorconfig",
    ".gitignore",
    ".gitattributes",
    ".clang-format",
}
TEXT_NAMES = {"LICENSE", "CMakeLists.txt", "pre-commit"}

EOL_BY_SUFFIX = {
    ".md": "crlf",
    ".yml": "lf",
    ".yaml": "lf",
    ".py": "lf",
    ".ps1": "lf",
    ".sh": "lf",
    ".cmd": "crlf",
    ".bat": "crlf",
    ".cmake": "lf",
    ".in": "lf",
    ".txt": "lf",
    ".json": "lf",
    ".def": "lf",
    ".mdc": "crlf",
    ".editorconfig": "lf",
    ".gitignore": "lf",
    ".gitattributes": "lf",
    ".clang-format": "crlf",
}
EOL_BY_NAME = {
    "CMakeLists.txt": "lf",
    "LICENSE": "lf",
    "pre-commit": "lf",
}

LANGUAGE_PAIRS = (
    ("README.md", "README.en.md"),
    ("docs/ARCHITECTURE.md", "docs/ARCHITECTURE.en.md"),
    ("CONTRIBUTING.md", "CONTRIBUTING.en.md"),
    ("SECURITY.md", "SECURITY.en.md"),
)

REQUIRED_FILES = (
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "CHANGELOG.md",
    "README.md",
    "README.en.md",
    "docs/ARCHITECTURE.md",
    "docs/ARCHITECTURE.en.md",
    "config/menu.json",
    "config/registration.json",
    "scripts/check_repo_rules.py",
    "AGENTS.md",
)

VERSION_HEADER = "include/shell_ext/common.h"


class Report:
    def __init__(self) -> None:
        self.problems: list[str] = []
        self.fixed: list[str] = []

    def bad(self, where: str, rule: str, message: str) -> None:
        self.problems.append(f"{where}: [{rule}] {message}")

    def fix(self, where: str, rule: str, message: str) -> None:
        self.fixed.append(f"{where}: [{rule}] {message}")


def tracked_files() -> list[Path]:
    try:
        out = subprocess.run(
            ["git", "ls-files", "-z"],
            cwd=ROOT,
            capture_output=True,
            check=True,
        ).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return [p for p in ROOT.rglob("*") if p.is_file() and not skipped(p)]
    files = [Path(p) for p in out.decode("utf-8").split("\0") if p]
    return [ROOT / f for f in files if not skipped(f)]


def staged_files() -> list[Path]:
    out = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"],
        cwd=ROOT,
        capture_output=True,
        check=True,
    ).stdout
    files = [Path(p) for p in out.decode("utf-8").split("\0") if p]
    return [ROOT / f for f in files if not skipped(f)]


def skipped(path: Path) -> bool:
    parts = path.parts[:-1]
    if any(part in SKIP_PARTS for part in parts):
        return True
    return any(part.startswith(SKIP_PREFIXES) for part in parts)


def rel(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def cpp_only(files: list[Path]) -> list[Path]:
    return [
        f
        for f in files
        if f.suffix.lower() in CPP_SUFFIXES and rel(f) not in UTF16_FILES and rel(f) not in RC_FILES
    ]


def eol_style(raw: bytes) -> str:
    crlf = raw.count(b"\r\n")
    bare_lf = raw.count(b"\n") - crlf
    if crlf and bare_lf:
        return "mixed"
    return "crlf" if crlf else "lf"


def first_significant_line(lines: list[str]) -> str:
    """First line that is neither blank nor part of a comment."""
    in_block = False
    for line in lines:
        text = line.strip()
        if in_block:
            if "*/" in text:
                in_block = False
                text = text.split("*/", 1)[1].strip()
                if text:
                    return text
            continue
        if not text:
            continue
        if text.startswith("//"):
            continue
        if text.startswith("/*"):
            if "*/" not in text:
                in_block = True
                continue
            text = text.split("*/", 1)[1].strip()
            if not text:
                continue
        return text
    return ""


# R1: hand-written C/C++ is UTF-8 with BOM and CRLF.
def check_encoding(paths: list[Path], report: Report, do_fix: bool) -> None:
    for path in paths:
        raw = path.read_bytes()
        if not raw:
            continue
        has_bom = raw.startswith(BOM)
        bare_lf = raw.count(b"\n") - raw.count(b"\r\n")
        if has_bom and bare_lf == 0:
            continue

        if do_fix:
            text = raw.decode("utf-8-sig", errors="replace")
            body = text.replace("\r\n", "\n").replace("\n", "\r\n")
            path.write_bytes(BOM + body.encode("utf-8"))
            details = []
            if not has_bom:
                details.append("added BOM")
            if bare_lf:
                details.append(f"normalised {bare_lf} bare LF to CRLF")
            report.fix(rel(path), "encoding", ", ".join(details))
        else:
            if not has_bom:
                report.bad(rel(path), "encoding", "missing UTF-8 BOM")
            if bare_lf:
                report.bad(rel(path), "encoding", f"{bare_lf} LF line ending(s) instead of CRLF")


# R2: no line wider than .clang-format's ColumnLimit.
def check_width(paths: list[Path], report: Report) -> None:
    for path in paths:
        for number, line in enumerate(path.read_bytes().decode("utf-8-sig", "replace").splitlines(), 1):
            width = len(line.replace("\t", " " * TAB_WIDTH).rstrip())
            if width > COLUMN_LIMIT:
                report.bad(f"{rel(path)}:{number}", "width", f"{width} columns > {COLUMN_LIMIT}")


# R3: Windows libraries are linked through CMake, not #pragma comment(lib).
PRAGMA_LINK = re.compile(r"#\s*pragma\s+comment\s*\(\s*(lib|linker)", re.IGNORECASE)


def check_pragma_comment(paths: list[Path], report: Report) -> None:
    for path in paths:
        for number, line in enumerate(path.read_bytes().decode("utf-8-sig", "replace").splitlines(), 1):
            if PRAGMA_LINK.search(line):
                report.bad(
                    f"{rel(path)}:{number}",
                    "cmake-libs",
                    "#pragma comment(lib/linker, ...) - link through target_link_libraries instead",
                )


# R4: headers are self-contained and use #pragma once.
def check_headers(paths: list[Path], report: Report) -> None:
    for path in paths:
        if path.suffix.lower() not in {".h", ".hpp", ".hxx", ".hh"}:
            continue
        if rel(path) in UTF16_FILES:
            continue
        lines = path.read_bytes().decode("utf-8-sig", "replace").splitlines()
        first = first_significant_line(lines)
        if first != "#pragma once":
            report.bad(rel(path), "header", f"first significant line is {first!r}, expected '#pragma once'")
        for number, line in enumerate(lines, 1):
            stripped = line.strip()
            if stripped.startswith("using namespace") and not line[:1] in ("\t", " "):
                report.bad(f"{rel(path)}:{number}", "header", "`using namespace` at namespace scope")


# R5: version, CMake and Version.rc agree with common.h.
def parse_cmake_version(text: str) -> tuple[int, int, int] | None:
    match = re.search(r"project\s*\([^)]*?VERSION\s+([0-9]+)\.([0-9]+)\.([0-9]+)", text, re.DOTALL)
    if not match:
        return None
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def parse_header_version(text: str) -> tuple[int, int, int] | None:
    values = {}
    for name in ("MAJOR", "MINOR", "PATCH"):
        match = re.search(rf"#define\s+SHELLEXT_VERSION_{name}\s+(\d+)", text)
        if not match:
            return None
        values[name] = int(match.group(1))
    return (values["MAJOR"], values["MINOR"], values["PATCH"])


def parse_rc_version(text: str) -> tuple[int, int, int] | None:
    match = re.search(r"FILEVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)", text)
    if not match:
        return None
    return tuple(int(part) for part in match.groups()[:3])  # type: ignore[return-value]


def check_version(report: Report) -> None:
    cmake_path = ROOT / "CMakeLists.txt"
    header_path = ROOT / VERSION_HEADER
    rc_path = ROOT / "src/resources/Version.rc"
    try:
        cmake = parse_cmake_version(cmake_path.read_text(encoding="utf-8"))
        header = parse_header_version(header_path.read_text(encoding="utf-8-sig"))
        rc = parse_rc_version(rc_path.read_text(encoding="utf-8"))
    except OSError as exc:
        report.bad("version", "version", f"cannot read version source: {exc}")
        return

    if cmake is None:
        report.bad("CMakeLists.txt", "version", "project(... VERSION x.y.z) not found")
    if header is None:
        report.bad(VERSION_HEADER, "version", "SHELLEXT_VERSION_* macros not found")
    if rc is None:
        report.bad("src/resources/Version.rc", "version", "FILEVERSION not found")
    if cmake and header and cmake != header:
        report.bad("CMakeLists.txt vs common.h", "version", f"{cmake} != {header}")
    if cmake and rc and cmake != rc:
        report.bad("CMakeLists.txt vs Version.rc", "version", f"{cmake} != {rc}")


# R6: changelog has an Unreleased section and every heading has its link.
CHANGELOG_HEADING = re.compile(r"^##\s+\[([^\]]+)\]")
CHANGELOG_DEFINITION = re.compile(r"^\[([^\]]+)\]:\s*(\S+)")


def missing_changelog_definitions(text: str) -> list[str]:
    headings = [match.group(1) for match in map(CHANGELOG_HEADING.match, text.splitlines()) if match]
    definitions = {match.group(1) for match in map(CHANGELOG_DEFINITION.match, text.splitlines()) if match}
    return [heading for heading in headings if heading not in definitions]


def check_changelog(report: Report) -> None:
    path = ROOT / "CHANGELOG.md"
    if not path.exists():
        report.bad("CHANGELOG.md", "changelog", "file missing")
        return
    text = path.read_text(encoding="utf-8-sig")
    if "## [Unreleased]" not in text:
        report.bad("CHANGELOG.md", "changelog", "no `## [Unreleased]` section")
    for heading in missing_changelog_definitions(text):
        report.bad(
            "CHANGELOG.md",
            "changelog",
            f"`## [{heading}]` has no `[{heading}]: <url>` definition",
        )


# R7: working-tree line endings match the policy table and .gitattributes.
def check_line_endings(files: list[Path], report: Report, do_fix: bool) -> None:
    for path in files:
        rel_path = rel(path)
        if path.suffix.lower() in CPP_SUFFIXES or rel_path in UTF16_FILES or rel_path in RC_FILES:
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in TEXT_NAMES:
            continue
        raw = path.read_bytes()
        if not raw:
            continue
        try:
            raw.decode("utf-8")
        except UnicodeDecodeError:
            continue

        expected = EOL_BY_SUFFIX.get(path.suffix.lower()) or EOL_BY_NAME.get(path.name)
        if expected is None:
            continue
        actual = eol_style(raw)
        if actual == expected:
            continue

        if do_fix and expected in ("lf", "crlf"):
            text = raw.decode("utf-8").replace("\r\n", "\n")
            if expected == "crlf":
                text = text.replace("\n", "\r\n")
            path.write_bytes(text.encode("utf-8"))
            report.fix(rel_path, "eol", f"{actual} -> {expected}")
            continue

        report.bad(
            rel_path,
            "eol",
            f"line endings are {actual}, expected {expected}; run --fix or update .gitattributes",
        )


def check_gitattributes(report: Report) -> None:
    path = ROOT / ".gitattributes"
    if not path.exists():
        report.bad(".gitattributes", "eol", "missing - the line-ending policy must be declared there")
        return
    text = path.read_text(encoding="utf-8")
    required = (
        r"\* text=auto",
        r"\*\.cpp\s+-text -eol",
        r"\*\.h\s+-text -eol",
        r"\*\.md\s+-text -eol",
        r"\*\.yml\s+text eol=lf",
        r"src/resources/resource\.h\s+-text -eol",
    )
    for pattern in required:
        if not re.search(pattern, text):
            report.bad(".gitattributes", "eol", f"missing policy rule: {pattern}")


# R8: relative Markdown links resolve.
LINK_RE = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")
LINK_DEF_RE = re.compile(r"^\s*\[[^\]]+\]:\s*(\S+)")


def markdown_links(text: str) -> list[tuple[int, str, str]]:
    links: list[tuple[int, str, str]] = []
    in_fence = False
    for number, line in enumerate(text.splitlines(), 1):
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for match in LINK_RE.finditer(line):
            links.append((number, match.group(1), match.group(2).strip()))
        match = LINK_DEF_RE.match(line)
        if match:
            links.append((number, "", match.group(1).strip()))
    return links


def check_markdown_links(files: list[Path], report: Report) -> None:
    for path in files:
        if path.suffix.lower() != ".md":
            continue
        text = path.read_bytes().decode("utf-8-sig", "replace")
        for number, _, target in markdown_links(text):
            target = target.strip()
            if target.startswith(("http://", "https://", "#", "mailto:")):
                continue
            clean = target.split("#", 1)[0].split('"', 1)[0].strip().strip("<>")
            if not clean:
                continue
            resolved = (path.parent / clean).resolve()
            if not resolved.exists():
                report.bad(f"{rel(path)}:{number}", "links", f"`{target}` does not resolve")


# R9: the bilingual pairs link to each other.
def check_bilingual_links(report: Report) -> None:
    for left_name, right_name in LANGUAGE_PAIRS:
        left = ROOT / left_name
        right = ROOT / right_name
        if not left.exists() or not right.exists():
            report.bad("bilingual", "translation", f"missing pair: {left_name} / {right_name}")
            continue
        left_text = left.read_text(encoding="utf-8-sig", errors="replace")
        right_text = right.read_text(encoding="utf-8-sig", errors="replace")
        if Path(right_name).name not in left_text:
            report.bad(left_name, "translation", f"does not link to {Path(right_name).name}")
        if Path(left_name).name not in right_text:
            report.bad(right_name, "translation", f"does not link to {Path(left_name).name}")


def check_required_files(report: Report) -> None:
    for name in REQUIRED_FILES:
        if not (ROOT / name).is_file():
            report.bad(name, "required", "required repository file is missing")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="repair BOM/line-ending violations")
    parser.add_argument("--staged", action="store_true", help="only check git-staged files (pre-commit)")
    args = parser.parse_args()

    report = Report()
    files = staged_files() if args.staged else tracked_files()
    paths = cpp_only(files)
    if not files:
        print("nothing to check")
        return 0

    check_encoding(paths, report, args.fix)
    check_width(paths, report)
    check_pragma_comment(paths, report)
    check_headers(paths, report)
    check_line_endings(files, report, args.fix)
    check_gitattributes(report)
    check_markdown_links(files, report)
    check_bilingual_links(report)
    check_required_files(report)
    check_version(report)
    check_changelog(report)

    for line in report.fixed:
        print(f"fixed  {line}")
    for line in report.problems:
        print(f"rule   {line}")

    if report.problems:
        print(f"\n{len(report.problems)} violation(s). See AGENTS.md for the rule text.")
        return 1
    scope = "staged files" if args.staged else f"{len(paths)} C/C++ of {len(files)} tracked files"
    print(f"repository rules: clean ({scope})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
