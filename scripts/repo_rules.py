#!/usr/bin/env python3
"""repo-rules - mechanical repository hygiene checks.

The prose rules live in a project's AGENTS.md / CONTRIBUTING.md. This tool
enforces the subset that a program can check: encoding, line endings, column
width, forbidden patterns, header conventions, version sync, changelog shape,
required files, Markdown links and bilingual pairs.

Run directly:
    python repo_rules.py check --config repo-rules.json
    python repo_rules.py fix   --config repo-rules.json
    python repo_rules.py init  --preset cpp-windows
    python repo_rules.py list-presets

Or install the console script:
    pipx install .
    repo-rules check
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

__version__ = "0.3.0"
CONFIG_NAME = "repo-rules.json"
TOOL_ROOT = Path(__file__).resolve().parent
BOM = b"\xef\xbb\xbf"
# Embedded so `sync --hook` also works from a vendored copy, where templates/ is absent.
PRECOMMIT_HOOK = """#!/bin/sh
# Pre-commit hook installed by `repo-rules init`.
#
# Install once per clone:
#   git config core.hooksPath .githooks
#
# The same check runs in CI; this hook is a convenience.

set -e
root=$(git rev-parse --show-toplevel)
cd "$root"

for candidate in python3 python py; do
\tif command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c 'import sys' >/dev/null 2>&1; then
\t\texec "$candidate" scripts/repo_rules.py check --config repo-rules.json --staged
\tfi
done

echo "pre-commit: no Python interpreter found; skipping repository rules" >&2
exit 0
"""


class ConfigError(Exception):
    """A configuration file or a preset is invalid."""


@dataclass
class Report:
    problems: List[str] = field(default_factory=list)
    fixed: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def bad(self, where: str, rule: str, message: str) -> None:
        self.problems.append("%s: [%s] %s" % (where, rule, message))

    def warn(self, where: str, rule: str, message: str) -> None:
        self.warnings.append("%s: [%s] %s" % (where, rule, message))

    def fix(self, where: str, rule: str, message: str) -> None:
        self.fixed.append("%s: [%s] %s" % (where, rule, message))


def _as_bool(value: Any, name: str) -> bool:
    if not isinstance(value, bool):
        raise ConfigError("%s must be a boolean" % name)
    return value


def _as_int(value: Any, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ConfigError("%s must be an integer" % name)
    return value


def _as_list(value: Any, name: str) -> List[Any]:
    if not isinstance(value, list):
        raise ConfigError("%s must be an array" % name)
    return value


def _as_str(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value:
        raise ConfigError("%s must be a non-empty string" % name)
    return value


def _as_globs(value: Any, name: str) -> List[str]:
    items = _as_list(value, name)
    result = []
    for index, item in enumerate(items):
        result.append(_as_str(item, "%s[%d]" % (name, index)))
    return result


def match_glob(rel_path: str, pattern: str) -> bool:
    """Match a repository-relative POSIX path with shell-style globs.

    A pattern without a slash (for example ``*.py``) matches a basename at any
    depth; a pattern with a slash is rooted at the repository root.
    """
    return PurePosixPath(rel_path).match(pattern)


class RuleConfig:
    def __init__(self, root: Path, data: Dict[str, Any]) -> None:
        self.root = root
        if not isinstance(data, dict):
            raise ConfigError("the root of the configuration must be an object")
        self.version = _as_int(data.get("version", 1), "version")
        if self.version != 1:
            raise ConfigError("unsupported configuration version %r (expected 1)" % self.version)

        self.skip_dirs = [str(item) for item in _as_list(data.get("skip_dirs", [".git"]), "skip_dirs")]
        self.skip_prefixes = [str(item) for item in _as_list(data.get("skip_prefixes", []), "skip_prefixes")]
        self.skip_globs = _as_globs(data.get("skip_globs", []), "skip_globs")

        self.encoding: List[Dict[str, Any]] = []
        for index, rule in enumerate(_as_list(data.get("encoding", []), "encoding")):
            if not isinstance(rule, dict):
                raise ConfigError("encoding[%d] must be an object" % index)
            eol = rule.get("eol", "any")
            if eol not in ("any", "lf", "crlf"):
                raise ConfigError("encoding[%d].eol must be any, lf or crlf" % index)
            self.encoding.append(
                {
                    "glob": _as_str(rule.get("glob"), "encoding[%d].glob" % index),
                    "bom": _as_bool(rule.get("bom", False), "encoding[%d].bom" % index),
                    "eol": eol,
                }
            )

        width = data.get("width", {})
        if not isinstance(width, dict):
            raise ConfigError("width must be an object")
        self.width_globs = _as_globs(width.get("globs", []), "width.globs")
        self.width_limit = _as_int(width.get("limit", 140), "width.limit")
        self.tab_width = _as_int(width.get("tab_width", 4), "width.tab_width")

        self.headers = data.get("headers", {})
        if not isinstance(self.headers, dict):
            raise ConfigError("headers must be an object")
        self.header_globs = _as_globs(self.headers.get("globs", []), "headers.globs")
        self.header_first_line = _as_str(self.headers.get("first_line", "#pragma once"), "headers.first_line")
        self.header_forbid_using = _as_bool(
            self.headers.get("forbid_namespace_using", True), "headers.forbid_namespace_using"
        )

        self.forbidden: List[Dict[str, Any]] = []
        for index, rule in enumerate(_as_list(data.get("forbidden", []), "forbidden")):
            if not isinstance(rule, dict):
                raise ConfigError("forbidden[%d] must be an object" % index)
            try:
                compiled = re.compile(_as_str(rule.get("pattern"), "forbidden[%d].pattern" % index))
            except re.error as exc:
                raise ConfigError("forbidden[%d].pattern is not a valid regex: %s" % (index, exc))
            self.forbidden.append(
                {
                    "id": _as_str(rule.get("id", "forbidden-%d" % index), "forbidden[%d].id" % index),
                    "globs": _as_globs(rule.get("globs", []), "forbidden[%d].globs" % index),
                    "pattern": compiled,
                    "message": _as_str(rule.get("message", "forbidden pattern"), "forbidden[%d].message" % index),
                }
            )

        self.versions: List[Dict[str, Any]] = []
        for index, group in enumerate(_as_list(data.get("versions", []), "versions")):
            if not isinstance(group, dict):
                raise ConfigError("versions[%d] must be an object" % index)
            sources = []
            for source_index, source in enumerate(_as_list(group.get("sources"), "versions[%d].sources" % index)):
                if not isinstance(source, dict):
                    raise ConfigError("versions[%d].sources[%d] must be an object" % (index, source_index))
                try:
                    pattern = re.compile(_as_str(source.get("pattern"), "versions[%d].sources[%d].pattern" % (index, source_index)))
                except re.error as exc:
                    raise ConfigError("versions[%d].sources[%d].pattern is not a valid regex: %s" % (index, source_index, exc))
                sources.append(
                    {
                        "path": _as_str(source.get("path"), "versions[%d].sources[%d].path" % (index, source_index)),
                        "pattern": pattern,
                    }
                )
            if not sources:
                raise ConfigError("versions[%d].sources must not be empty" % index)
            self.versions.append({"id": _as_str(group.get("id", "version-%d" % index), "versions[%d].id" % index), "sources": sources})

        changelog = data.get("changelog")
        self.changelog: Optional[Dict[str, Any]] = None
        if changelog is not None:
            if not isinstance(changelog, dict):
                raise ConfigError("changelog must be an object or null")
            self.changelog = {
                "path": _as_str(changelog.get("path", "CHANGELOG.md"), "changelog.path"),
                "require": _as_str(changelog.get("require", "## [Unreleased]"), "changelog.require"),
                "link_definitions": _as_bool(changelog.get("link_definitions", True), "changelog.link_definitions"),
            }

        self.required_files = [str(item) for item in _as_list(data.get("required_files", []), "required_files")]

        markdown = data.get("markdown", {})
        if not isinstance(markdown, dict):
            raise ConfigError("markdown must be an object")
        self.markdown_globs = _as_globs(markdown.get("globs", []), "markdown.globs")
        self.markdown_links = _as_bool(markdown.get("check_links", True), "markdown.check_links")
        self.bilingual_pairs: List[Tuple[str, str]] = []
        for index, pair in enumerate(_as_list(markdown.get("bilingual_pairs", []), "markdown.bilingual_pairs")):
            if not isinstance(pair, list) or len(pair) != 2:
                raise ConfigError("markdown.bilingual_pairs[%d] must be a two-element array" % index)
            self.bilingual_pairs.append(
                (
                    _as_str(pair[0], "markdown.bilingual_pairs[%d][0]" % index),
                    _as_str(pair[1], "markdown.bilingual_pairs[%d][1]" % index),
                )
            )

        translations = markdown.get("translations")
        self.translation_pairs: List[Tuple[str, str]] = []
        self.translation_marker = "translated-from:"
        self.translation_suffix = ".zh-CN.md"
        self.translation_stale = "warning"
        self.translation_cjk_wrap = False
        self.translation_keep_language = False
        if translations is not None:
            if not isinstance(translations, dict):
                raise ConfigError("markdown.translations must be an object")
            pairs = _as_list(translations.get("pairs", []), "markdown.translations.pairs")
            for index, pair in enumerate(pairs):
                if not isinstance(pair, list) or len(pair) != 2:
                    raise ConfigError("markdown.translations.pairs[%d] must be a two-element array" % index)
                self.translation_pairs.append(
                    (
                        _as_str(pair[0], "markdown.translations.pairs[%d][0]" % index),
                        _as_str(pair[1], "markdown.translations.pairs[%d][1]" % index),
                    )
                )
            self.translation_marker = _as_str(
                translations.get("marker", "translated-from:"), "markdown.translations.marker"
            )
            stale = _as_str(translations.get("stale", "warning"), "markdown.translations.stale")
            if stale not in ("warning", "error", "off"):
                raise ConfigError("markdown.translations.stale must be warning, error or off")
            self.translation_stale = stale
            self.translation_suffix = _as_str(
                translations.get("suffix", ".zh-CN.md"), "markdown.translations.suffix"
            )
            self.translation_cjk_wrap = _as_bool(
                translations.get("cjk_wrap", False), "markdown.translations.cjk_wrap"
            )
            self.translation_keep_language = _as_bool(
                translations.get("keep_language", False), "markdown.translations.keep_language"
            )

        # markdown.languages: one document set with N ordered language variants.
        languages = markdown.get("languages")
        self.languages: Optional[Dict[str, Any]] = None
        if languages is not None:
            if not isinstance(languages, dict):
                raise ConfigError("markdown.languages must be an object")
            default_name = _as_str(languages.get("default", "README.md"), "markdown.languages.default")
            variants = [
                _as_str(item, "markdown.languages.variants[%d]" % index)
                for index, item in enumerate(_as_list(languages.get("variants", []), "markdown.languages.variants"))
            ]
            if not variants:
                raise ConfigError("markdown.languages.variants must not be empty")
            if default_name in variants:
                raise ConfigError("markdown.languages.default must not also be listed in variants")
            if len(set(variants)) != len(variants):
                raise ConfigError("markdown.languages.variants contains duplicates")
            self.languages = {
                "default": default_name,
                "variants": variants,
                "switch_line": _as_bool(languages.get("switch_line", True), "markdown.languages.switch_line"),
            }

        gitattributes = data.get("gitattributes", {})
        if not isinstance(gitattributes, dict):
            raise ConfigError("gitattributes must be an object")
        self.gitattributes_path = _as_str(gitattributes.get("path", ".gitattributes"), "gitattributes.path")
        self.gitattributes_patterns: List[re.Pattern[str]] = []
        for index, pattern in enumerate(_as_list(gitattributes.get("required_patterns", []), "gitattributes.required_patterns")):
            try:
                self.gitattributes_patterns.append(re.compile(_as_str(pattern, "gitattributes.required_patterns[%d]" % index)))
            except re.error as exc:
                raise ConfigError("gitattributes.required_patterns[%d] is not a valid regex: %s" % (index, exc))

    @classmethod
    def load(cls, root: Path, config_name: str = CONFIG_NAME) -> "RuleConfig":
        path = root / config_name
        if not path.is_file():
            raise ConfigError("configuration not found: %s (run `repo-rules init`?)" % path)
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as exc:
            raise ConfigError("cannot read %s: %s" % (path, exc))
        return cls(root, data)

    def is_skipped(self, rel_path: str) -> bool:
        parts = PurePosixPath(rel_path).parts
        directories = parts[:-1]
        if any(part in self.skip_dirs for part in directories):
            return True
        if any(part.startswith(tuple(self.skip_prefixes)) for part in directories if self.skip_prefixes):
            return True
        return any(match_glob(rel_path, pattern) for pattern in self.skip_globs)

    def encoding_for(self, rel_path: str) -> Optional[Dict[str, Any]]:
        for rule in self.encoding:
            if match_glob(rel_path, rule["glob"]):
                return rule
        return None


def git_files(root: Path, staged: bool) -> List[Path]:
    if staged:
        command = ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"]
    else:
        command = ["git", "ls-files", "-z"]
    try:
        output = subprocess.run(command, cwd=str(root), capture_output=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return sorted(path for path in root.rglob("*") if path.is_file())

    files = []
    for name in output.decode("utf-8").split("\0"):
        if not name:
            continue
        path = root / name
        if path.is_file():
            files.append(path)
    return sorted(files)


def rel_path(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def first_significant_line(lines: Sequence[str]) -> str:
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


LINK_RE = re.compile(r"\[([^\]]*)\]\(([^)]+)\)")
LINK_DEF_RE = re.compile(r"^\s*\[[^\]]+\]:\s*(\S+)")
CHANGELOG_HEADING = re.compile(r"^##\s+\[([^\]]+)\]")
CHANGELOG_DEFINITION = re.compile(r"^\[([^\]]+)\]:\s*(\S+)")


def markdown_links(text: str) -> List[Tuple[int, str, str]]:
    links: List[Tuple[int, str, str]] = []
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


def missing_changelog_definitions(text: str) -> List[str]:
    headings = [match.group(1) for match in map(CHANGELOG_HEADING.match, text.splitlines()) if match]
    definitions = {match.group(1) for match in map(CHANGELOG_DEFINITION.match, text.splitlines()) if match}
    return [heading for heading in headings if heading not in definitions]


HEADING_RE = re.compile(r"^#\s+\S")


def language_switch_line(text: str) -> Tuple[int, str]:
    """The language switch line: first non-empty line after the H1.

    Returns (1-based line number, line) or (0, "") when the file has no H1 or no
    line after it. Comment lines are ignored so a provenance marker above the
    title does not shift the result.
    """
    lines = text.splitlines()
    heading = None
    for index, line in enumerate(lines):
        stripped = line.strip()
        if not stripped or stripped.startswith("<!--"):
            continue
        if HEADING_RE.match(stripped):
            heading = index
        break
    if heading is None:
        return 0, ""
    for index in range(heading + 1, len(lines)):
        stripped = lines[index].strip()
        if not stripped or stripped.startswith("<!--"):
            continue
        # A section heading right after the title means there is no switch line.
        if stripped.startswith("#"):
            return 0, ""
        return index + 1, stripped
    return 0, ""


def switch_line_targets(line: str, known: Sequence[str]) -> List[str]:
    """Targets on a switch line that point at one of the known language files.

    A `*italic*` or plain label is not a link, so the current language (rendered
    in bold) never shows up here.
    """
    found: List[str] = []
    for match in LINK_RE.finditer(line):
        target = match.group(2).strip().strip("<>")
        target = target.split("#", 1)[0]
        if target in known:
            found.append(target)
    return found


CJK_END = re.compile(r"[\u4e00-\u9fff]$")
CJK_START = re.compile(r"^[\u4e00-\u9fff]")
CJK_PUNCT_END = re.compile(r"[，。；：！？、]$")


def cjk_wrap_lines(text: str) -> List[int]:
    """1-based lines that start with a CJK character right after a line ending with
    one: Markdown renders that newline as a space, so the sentence is broken."""
    lines = text.splitlines()
    wrapped: List[int] = []
    in_fence = False
    for index in range(len(lines) - 1):
        line = lines[index]
        nxt = lines[index + 1]
        if line.lstrip().startswith("```"):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        if line.startswith(("#", "|", "    ")) or nxt.startswith(("#", "|", "    ")):
            continue
        if CJK_END.search(line) and not CJK_PUNCT_END.search(line) and CJK_START.match(nxt):
            wrapped.append(index + 2)
    return wrapped


def recorded_translation_sha(text: str, marker: str) -> Optional[str]:
    recorded = None
    for line in text.splitlines():
        if marker in line:
            for token in line.replace("-->", " ").split():
                if token.startswith("sha256:"):
                    recorded = token[len("sha256:"):]
    return recorded


def missing_translation_links(text: str, base: Path, suffix: str) -> List[Tuple[int, str, str]]:
    """Relative `.md` links whose translated sibling (`<stem><suffix>`) exists."""
    out: List[Tuple[int, str, str]] = []
    for number, label, target in markdown_links(text):
        if label.strip().lower() == "english":
            continue
        if target.startswith(("http://", "https://", "mailto:", "#", "/")):
            continue
        path = target.split("#", 1)[0].split("?", 1)[0].strip("<>")
        if not path.endswith(".md") or path.endswith(suffix):
            continue
        candidate = path[:-3] + suffix
        if (base / candidate).exists():
            out.append((number, target, candidate))
    return out


class Checker:
    def __init__(self, config: RuleConfig) -> None:
        self.config = config
        self.report = Report()

    def run(self, files: Sequence[Path], fix: bool = False) -> Report:
        for path in files:
            rel = rel_path(self.config.root, path)
            if self.config.is_skipped(rel):
                continue
            self._check_encoding(path, rel, fix)
            self._check_width(path, rel)
            self._check_forbidden(path, rel)
            self._check_header(path, rel)

        self._check_versions()
        self._check_changelog()
        self._check_required()
        self._check_markdown(files)
        self._check_bilingual()
        self._check_languages()
        if self.config.translation_pairs:
            self._check_translations()
            if self.config.translation_cjk_wrap:
                self._check_cjk_wrap()
            if self.config.translation_keep_language:
                self._check_lang_links()
        self._check_gitattributes()
        return self.report

    def _check_encoding(self, path: Path, rel: str, fix: bool) -> None:
        rule = self.config.encoding_for(rel)
        if rule is None:
            return
        raw = path.read_bytes()
        if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
            self.report.bad(rel, "encoding", "UTF-16 file matched a UTF-8 encoding rule; add it to skip_globs")
            return
        if b"\0" in raw[:4096]:
            self.report.bad(rel, "encoding", "binary file matched a text encoding rule; add it to skip_globs")
            return

        has_bom = raw.startswith(BOM)
        eol = rule["eol"]
        crlf = raw.count(b"\r\n")
        bare_lf = raw.count(b"\n") - crlf
        needs_fix = False

        if rule["bom"] and not has_bom:
            needs_fix = True
        if not rule["bom"] and has_bom:
            needs_fix = True
        if eol == "crlf" and bare_lf:
            needs_fix = True
        if eol == "lf" and crlf:
            needs_fix = True

        if not needs_fix:
            return

        if fix:
            text = raw.decode("utf-8-sig", errors="replace")
            text = text.replace("\r\n", "\n").replace("\r", "\n")
            if eol == "crlf":
                text = text.replace("\n", "\r\n")
            data = text.encode("utf-8")
            if rule["bom"]:
                data = BOM + data
            path.write_bytes(data)
            details = []
            if rule["bom"] and not has_bom:
                details.append("added BOM")
            if not rule["bom"] and has_bom:
                details.append("removed BOM")
            if eol == "crlf" and bare_lf:
                details.append("normalised %d LF to CRLF" % bare_lf)
            if eol == "lf" and crlf:
                details.append("normalised %d CRLF to LF" % crlf)
            self.report.fix(rel, "encoding", ", ".join(details))
            return

        if rule["bom"] and not has_bom:
            self.report.bad(rel, "encoding", "missing UTF-8 BOM")
        if not rule["bom"] and has_bom:
            self.report.bad(rel, "encoding", "unexpected UTF-8 BOM")
        if eol == "crlf" and bare_lf:
            self.report.bad(rel, "encoding", "%d bare LF line ending(s); expected CRLF" % bare_lf)
        if eol == "lf" and crlf:
            self.report.bad(rel, "encoding", "%d CRLF line ending(s); expected LF" % crlf)

    def _check_width(self, path: Path, rel: str) -> None:
        if not any(match_glob(rel, pattern) for pattern in self.config.width_globs):
            return
        raw = path.read_bytes()
        if b"\0" in raw[:4096]:
            return
        text = raw.decode("utf-8-sig", errors="replace")
        for number, line in enumerate(text.splitlines(), 1):
            width = len(line.expandtabs(self.config.tab_width).rstrip())
            if width > self.config.width_limit:
                self.report.bad(
                    "%s:%d" % (rel, number),
                    "width",
                    "%d columns > %d" % (width, self.config.width_limit),
                )

    def _check_forbidden(self, path: Path, rel: str) -> None:
        text = path.read_bytes().decode("utf-8-sig", errors="replace")
        for rule in self.config.forbidden:
            if not any(match_glob(rel, pattern) for pattern in rule["globs"]):
                continue
            for number, line in enumerate(text.splitlines(), 1):
                if rule["pattern"].search(line):
                    self.report.bad("%s:%d" % (rel, number), rule["id"], rule["message"])

    def _check_header(self, path: Path, rel: str) -> None:
        if not any(match_glob(rel, pattern) for pattern in self.config.header_globs):
            return
        lines = path.read_bytes().decode("utf-8-sig", errors="replace").splitlines()
        first = first_significant_line(lines)
        if first != self.config.header_first_line:
            self.report.bad(rel, "header", "first significant line is %r, expected %r" % (first, self.config.header_first_line))
        if self.config.header_forbid_using:
            for number, line in enumerate(lines, 1):
                if line.strip().startswith("using namespace") and not line[:1] in ("\t", " "):
                    self.report.bad("%s:%d" % (rel, number), "header", "`using namespace` at namespace scope")

    def _check_versions(self) -> None:
        for group in self.config.versions:
            values: List[Tuple[str, ...]] = []
            for source in group["sources"]:
                path = self.config.root / source["path"]
                try:
                    text = path.read_text(encoding="utf-8-sig", errors="replace")
                except OSError:
                    self.report.bad(source["path"], "version", "file is missing")
                    values = []
                    break
                match = source["pattern"].search(text)
                if match is None:
                    self.report.bad(source["path"], "version", "version pattern did not match")
                    values = []
                    break
                if not match.groups():
                    self.report.bad(source["path"], "version", "version pattern has no capture groups")
                    values = []
                    break
                values.append(tuple(match.groups()))

            if len(values) != len(group["sources"]) or not values:
                continue
            first = values[0]
            if any(value != first for value in values[1:]):
                self.report.bad(
                    group["id"],
                    "version",
                    "version values disagree: " + ", ".join(
                        "%s=%s" % (source["path"], ".".join(value)) for source, value in zip(group["sources"], values)
                    ),
                )

    def _check_changelog(self) -> None:
        cfg = self.config.changelog
        if cfg is None:
            return
        path = self.config.root / cfg["path"]
        if not path.is_file():
            self.report.bad(cfg["path"], "changelog", "file is missing")
            return
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        if cfg["require"] not in text:
            self.report.bad(cfg["path"], "changelog", "missing required section: %s" % cfg["require"])
        if cfg["link_definitions"]:
            for heading in missing_changelog_definitions(text):
                self.report.bad(cfg["path"], "changelog", "`## [%s]` has no `[%s]: <url>` definition" % (heading, heading))

    def _check_required(self) -> None:
        for name in self.config.required_files:
            if not (self.config.root / name).is_file():
                self.report.bad(name, "required", "required file is missing")

    def _check_markdown(self, files: Sequence[Path]) -> None:
        if not self.config.markdown_links:
            return
        for path in files:
            rel = rel_path(self.config.root, path)
            if self.config.is_skipped(rel):
                continue
            if not any(match_glob(rel, pattern) for pattern in self.config.markdown_globs):
                continue
            text = path.read_bytes().decode("utf-8-sig", errors="replace")
            for number, _, target in markdown_links(text):
                if target.startswith(("http://", "https://", "#", "mailto:", "/")):
                    continue
                clean = target.split("#", 1)[0].split('"', 1)[0].strip().strip("<>")
                if not clean:
                    continue
                if not (path.parent / clean).resolve().exists():
                    self.report.bad("%s:%d" % (rel, number), "links", "`%s` does not resolve" % target)

    def _check_bilingual(self) -> None:
        for left_name, right_name in self.config.bilingual_pairs:
            left = self.config.root / left_name
            right = self.config.root / right_name
            if not left.is_file() or not right.is_file():
                self.report.bad("bilingual", "translation", "missing pair: %s / %s" % (left_name, right_name))
                continue
            left_text = left.read_text(encoding="utf-8-sig", errors="replace")
            right_text = right.read_text(encoding="utf-8-sig", errors="replace")
            if Path(right_name).name not in left_text:
                self.report.bad(left_name, "translation", "does not link to %s" % Path(right_name).name)
            if Path(left_name).name not in right_text:
                self.report.bad(right_name, "translation", "does not link to %s" % Path(left_name).name)

    def _check_languages(self) -> None:
        cfg = self.config.languages
        if cfg is None:
            return
        ordered = [cfg["default"]] + list(cfg["variants"])
        existing = []
        for name in ordered:
            if (self.config.root / name).is_file():
                existing.append(name)
            else:
                self.report.bad(name, "languages", "missing language variant")

        if not cfg["switch_line"]:
            return

        for name in existing:
            text = (self.config.root / name).read_bytes().decode("utf-8-sig", errors="replace")
            number, line = language_switch_line(text)
            if not line:
                self.report.bad(name, "languages", "no language switch line below the title")
                continue
            where = "%s:%d" % (name, number) if number else name
            if not line.startswith("**"):
                self.report.bad(where, "languages", "switch line must start with the current language in bold")
            expected = [other for other in ordered if other != name]
            found = switch_line_targets(line, ordered)
            if found != expected:
                self.report.bad(
                    where,
                    "languages",
                    "switch line lists %s, expected %s" % (found, expected),
                )

    def _check_translations(self) -> None:
        for source_name, target_name in self.config.translation_pairs:
            source = self.config.root / source_name
            target = self.config.root / target_name
            if not source.is_file():
                self.report.bad(source_name, "translation", "listed in markdown.translations.pairs but missing")
                continue
            if not target.is_file():
                self.report.bad(target_name, "translation", "missing - the translated set must stay complete")
                continue
            text = target.read_text(encoding="utf-8-sig", errors="replace")
            if self.config.translation_marker not in text:
                self.report.bad(
                    target_name,
                    "translation",
                    "no `<!-- %s ... sha256:... -->` marker" % self.config.translation_marker.rstrip(":"),
                )
                continue
            recorded = recorded_translation_sha(text, self.config.translation_marker)
            if recorded is None or self.config.translation_stale == "off":
                continue
            current = hashlib.sha256(source.read_bytes()).hexdigest()[:12]
            if recorded == current:
                continue
            message = (
                "translated from %s at sha256:%s, but the source is now sha256:%s - "
                "re-read the change and update the marker" % (source_name, recorded, current)
            )
            if self.config.translation_stale == "error":
                self.report.bad(target_name, "translation-stale", message)
            else:
                self.report.warn(target_name, "translation-stale", message)

    def _check_cjk_wrap(self) -> None:
        for _source_name, target_name in self.config.translation_pairs:
            target = self.config.root / target_name
            if not target.is_file():
                continue
            text = target.read_text(encoding="utf-8-sig", errors="replace")
            for line in cjk_wrap_lines(text):
                self.report.bad(
                    target_name,
                    "cjk-wrap",
                    "line %d: a newline between Chinese characters renders as a space; "
                    "break after punctuation, a Latin token or inline code" % line,
                )

    def _check_lang_links(self) -> None:
        for _source_name, target_name in self.config.translation_pairs:
            target = self.config.root / target_name
            if not target.is_file():
                continue
            text = target.read_text(encoding="utf-8-sig", errors="replace")
            for line, href, candidate in missing_translation_links(
                text, target.parent, self.config.translation_suffix
            ):
                self.report.bad(
                    target_name,
                    "lang-link",
                    "line %d: links to %s; use %s so the translated page stays in its language"
                    % (line, href, candidate),
                )

    def _check_gitattributes(self) -> None:
        path = self.config.root / self.config.gitattributes_path
        if not self.config.gitattributes_patterns:
            return
        if not path.is_file():
            self.report.bad(self.config.gitattributes_path, "gitattributes", "file is missing")
            return
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern in self.config.gitattributes_patterns:
            if not pattern.search(text):
                self.report.bad(self.config.gitattributes_path, "gitattributes", "missing required pattern: %s" % pattern.pattern)


def _resource_root() -> Path:
    candidates = [TOOL_ROOT, Path(sys.prefix) / "share" / "repo-rules"]
    for candidate in candidates:
        if (candidate / "presets").is_dir() or (candidate / "templates").is_dir():
            return candidate
    return TOOL_ROOT


RESOURCE_ROOT = _resource_root()


def load_preset(name: str) -> Dict[str, Any]:
    candidate = Path(name)
    if not candidate.is_file():
        if not name.endswith(".json"):
            name += ".json"
        candidate = RESOURCE_ROOT / "presets" / name
    if not candidate.is_file():
        raise ConfigError("preset not found: %s (use `repo-rules list-presets`)" % candidate)
    try:
        data = json.loads(candidate.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ConfigError("cannot read preset %s: %s" % (candidate, exc))
    if not isinstance(data, dict):
        raise ConfigError("preset root must be an object: %s" % candidate)
    return data


def write_json(path: Path, data: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def command_check(args: argparse.Namespace, fix: bool) -> int:
    root = Path(args.root).resolve()
    try:
        config = RuleConfig.load(root, args.config)
        files = git_files(root, args.staged)
        report = Checker(config).run(files, fix=fix)
    except ConfigError as exc:
        print("repo-rules: %s" % exc, file=sys.stderr)
        return 2

    for line in report.fixed:
        print("fixed  %s" % line)
    for line in report.warnings:
        print("warn   %s" % line)
    for line in report.problems:
        print("rule   %s" % line)
    if report.problems:
        print("\n%d violation(s). See the project rules for context." % len(report.problems))
        return 1
    suffix = "" if not report.warnings else ", %d warning(s)" % len(report.warnings)
    print("repo-rules: clean (%d file(s)%s)" % (len(files), suffix))
    return 0


def command_init(args: argparse.Namespace) -> int:
    root = Path(args.root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    config_path = root / CONFIG_NAME
    if config_path.exists() and not args.force:
        print("repo-rules: %s already exists; use --force to overwrite" % config_path, file=sys.stderr)
        return 2

    try:
        preset = load_preset(args.preset)
    except ConfigError as exc:
        print("repo-rules: %s" % exc, file=sys.stderr)
        return 2
    write_json(config_path, preset)
    print("wrote %s" % config_path)

    if not args.no_tool:
        tool_target = root / "scripts" / "repo_rules.py"
        tool_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(Path(__file__).resolve(), tool_target)
        print("copied tool to %s" % tool_target)

    hook_target = root / ".githooks" / "pre-commit"
    if hook_target.exists() and not args.force:
        print("kept %s (already exists)" % hook_target)
    else:
        hook_target.parent.mkdir(parents=True, exist_ok=True)
        hook_target.write_bytes(PRECOMMIT_HOOK.encode("utf-8"))
        try:
            hook_target.chmod(0o755)
        except OSError:
            pass
        print("wrote %s" % hook_target)

    workflow_source = RESOURCE_ROOT / "templates" / "github" / "workflows" / "hygiene.yml"
    workflow_target = root / ".github" / "workflows" / "hygiene.yml"
    if workflow_target.exists() and not args.force:
        print("kept %s (already exists)" % workflow_target)
    else:
        workflow_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(workflow_source, workflow_target)
        print("wrote %s" % workflow_target)

    print("\nNext:")
    print("  1. edit %s for this project" % config_path)
    print("  2. python scripts/repo_rules.py check")
    print("  3. git config core.hooksPath .githooks")
    return 0


def command_list_presets(args: argparse.Namespace) -> int:
    presets = sorted((RESOURCE_ROOT / "presets").glob("*.json"))
    for preset in presets:
        print(preset.stem)
    return 0


def command_sync(args: argparse.Namespace) -> int:
    root = Path(args.root).resolve()
    if not root.is_dir():
        print("repo-rules: not a directory: %s" % root, file=sys.stderr)
        return 2

    source = Path(getattr(args, "source", None) or __file__).resolve()
    if not source.is_file():
        print("repo-rules: checker not found: %s" % source, file=sys.stderr)
        return 2

    dest_rel = getattr(args, "dest", "scripts/repo_rules.py")
    want_hook = bool(getattr(args, "hook", False))
    check_only = bool(getattr(args, "check", False))

    jobs = [(root / dest_rel, source.read_bytes(), False)]
    if want_hook:
        jobs.append((root / ".githooks" / "pre-commit", PRECOMMIT_HOOK.encode("utf-8"), True))

    stale = []
    for dst, payload, executable in jobs:
        try:
            existing = dst.read_bytes()
        except OSError:
            existing = None
        if existing == payload:
            print("up to date: %s" % dst)
            continue
        stale.append(dst)
        if check_only:
            print("stale: %s" % dst)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(payload)
        if executable:
            try:
                dst.chmod(0o755)
            except OSError:
                pass
        print("synced: %s" % dst)

    if check_only and stale:
        print("repo-rules: %d file(s) stale (run sync without --check)" % len(stale), file=sys.stderr)
        return 1
    if stale and want_hook and not check_only:
        print("tip: git update-index --chmod=+x .githooks/pre-commit to keep the executable bit")
    print("repo-rules: %s" % ("synced %d file(s)" % len(stale) if stale else "up to date"))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="repo-rules", description="Mechanical repository hygiene checks.")
    parser.add_argument("--version", action="version", version="%(prog)s " + __version__)
    subparsers = parser.add_subparsers(dest="command")

    for name in ("check", "fix"):
        sub = subparsers.add_parser(name, help="%s repository rules" % name)
        sub.add_argument("--config", default=CONFIG_NAME, help="configuration path relative to --root")
        sub.add_argument("--root", default=".", help="repository root (default: current directory)")
        sub.add_argument("--staged", action="store_true", help="only inspect git-staged files")
        sub.set_defaults(func=lambda args, fix=(name == "fix"): command_check(args, fix))

    init = subparsers.add_parser("init", help="create repo-rules.json, tool copy and templates")
    init.add_argument("--root", default=".", help="target repository root")
    init.add_argument("--preset", default="cpp-windows", help="built-in preset name or path to a JSON file")
    init.add_argument("--force", action="store_true", help="overwrite existing generated files")
    init.add_argument("--no-tool", action="store_true", help="do not copy repo_rules.py into scripts/")
    init.set_defaults(func=command_init)

    list_presets = subparsers.add_parser("list-presets", help="list built-in presets")
    list_presets.set_defaults(func=command_list_presets)

    sync = subparsers.add_parser("sync", help="sync the vendored checker and hook into a project")
    sync.add_argument("--root", default=".", help="target repository root")
    sync.add_argument("--dest", default="scripts/repo_rules.py", help="vendored checker path relative to --root")
    sync.add_argument("--source", default=None, help="checker file to copy (default: this script)")
    sync.add_argument("--hook", action="store_true", help="also sync .githooks/pre-commit")
    sync.add_argument("--check", action="store_true", help="verify only; exit 1 when a copy is stale")
    sync.set_defaults(func=command_sync)
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "func"):
        parser.print_help()
        return 0
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
