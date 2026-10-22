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

__version__ = "0.10.0"
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

# The checker lives in scripts/ for a project and at the root for RepoRules itself.
tool=scripts/repo_rules.py
[ -f "$tool" ] || tool=repo_rules.py

for candidate in python3 python py; do
\tif command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c 'import sys' >/dev/null 2>&1; then
\t\texec "$candidate" "$tool" check --config repo-rules.json --staged
\tfi
done

echo "pre-commit: no Python interpreter found; skipping repository rules" >&2
exit 0
"""

COMMIT_MSG_HOOK = """#!/bin/sh
# commit-msg hook installed by `repo-rules init`.
#
# Rejects a commit message that is not English when `commit_message.english` is
# on. Install once per clone:
#   git config core.hooksPath .githooks
#
# The same check runs in CI; this hook is a convenience.
# $1 is the path of the message file git just wrote.

set -e
root=$(git rev-parse --show-toplevel)
cd "$root"
[ -f repo-rules.json ] || exit 0

# The checker lives in scripts/ for a project and at the root for RepoRules itself.
tool=scripts/repo_rules.py
[ -f "$tool" ] || tool=repo_rules.py

for candidate in python3 python py; do
\tif command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c 'import sys' >/dev/null 2>&1; then
\t\texec "$candidate" "$tool" check-msg "$1" --config repo-rules.json
\tfi
done

echo "commit-msg: no Python interpreter found; skipping the message check" >&2
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
        self.translation_cover: List[Dict[str, Any]] = []
        self.translation_untracked = False
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
            for index, entry in enumerate(_as_list(translations.get("cover", []), "markdown.translations.cover")):
                if not isinstance(entry, dict):
                    raise ConfigError("markdown.translations.cover[%d] must be an object" % index)
                missing = _as_str(
                    entry.get("missing", "error"), "markdown.translations.cover[%d].missing" % index
                )
                if missing not in ("error", "warning"):
                    raise ConfigError(
                        "markdown.translations.cover[%d].missing must be error or warning" % index
                    )
                self.translation_cover.append(
                    {
                        "from": _as_str(entry.get("from"), "markdown.translations.cover[%d].from" % index),
                        "to": _as_str(entry.get("to"), "markdown.translations.cover[%d].to" % index),
                        "ignore": _as_globs(
                            entry.get("ignore", []), "markdown.translations.cover[%d].ignore" % index
                        ),
                        "missing": missing,
                    }
                )
            self.translation_untracked = _as_bool(
                translations.get("untracked", False), "markdown.translations.untracked"
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
            language_stale = _as_str(languages.get("stale", "off"), "markdown.languages.stale")
            if language_stale not in ("off", "warning", "error"):
                raise ConfigError("markdown.languages.stale must be off, warning or error")
            self.languages = {
                "default": default_name,
                "variants": variants,
                "switch_line": _as_bool(languages.get("switch_line", True), "markdown.languages.switch_line"),
                "stale": language_stale,
            }

        # comments: code comments must be in English (non-Latin script is flagged).
        comments = data.get("comments", {})
        self.comment_globs: List[str] = []
        self.comment_allow_files: List[str] = []
        self.comment_allow_marker = "cjk-ok"
        if comments:
            if not isinstance(comments, dict):
                raise ConfigError("comments must be an object")
            if _as_bool(comments.get("english", True), "comments.english"):
                self.comment_globs = _as_globs(
                    comments.get(
                        "globs",
                        [
                            "*.c",
                            "*.cc",
                            "*.cpp",
                            "*.cxx",
                            "*.h",
                            "*.hh",
                            "*.hpp",
                            "*.hxx",
                            "*.py",
                            "*.sh",
                            "*.ps1",
                            "*.js",
                            "*.bat",
                            "*.cmd",
                        ],
                    ),
                    "comments.globs",
                )
                self.comment_allow_files = _as_globs(
                    comments.get("allow_files", []), "comments.allow_files"
                )
                self.comment_allow_marker = _as_str(
                    comments.get("allow_marker", "cjk-ok"), "comments.allow_marker"
                )

        # commit_message: commit messages are English (checked by the commit-msg hook).
        commit_message = data.get("commit_message", {})
        self.commit_message_english = False
        self.commit_message_marker = "cjk-ok"
        if commit_message:
            if not isinstance(commit_message, dict):
                raise ConfigError("commit_message must be an object")
            self.commit_message_english = _as_bool(
                commit_message.get("english", True), "commit_message.english"
            )
            self.commit_message_marker = _as_str(
                commit_message.get("allow_marker", "cjk-ok"), "commit_message.allow_marker"
            )

        # readme_badges: the shields block belongs under the title, above the prose.
        readme_badges = data.get("readme_badges", {})
        self.badge_files: List[str] = []
        self.badge_min = 0
        self.badge_required: List[str] = []
        if readme_badges:
            if not isinstance(readme_badges, dict):
                raise ConfigError("readme_badges must be an object")
            self.badge_files = [
                _as_str(item, "readme_badges.files[%d]" % index)
                for index, item in enumerate(
                    _as_list(readme_badges.get("files", ["README.md"]), "readme_badges.files")
                )
            ]
            self.badge_min = int(readme_badges.get("min", 1))
            self.badge_required = [
                _as_str(item, "readme_badges.require[%d]" % index)
                for index, item in enumerate(_as_list(readme_badges.get("require", []), "readme_badges.require"))
            ]

        # about: the declared GitHub description, checked for shape and language order.
        about = data.get("about", {})
        self.about_description = ""
        self.about_order = "en-zh"
        if about:
            if not isinstance(about, dict):
                raise ConfigError("about must be an object")
            self.about_description = _as_str(about.get("description", ""), "about.description")
            self.about_order = _as_str(about.get("order", "en-zh"), "about.order")
            if self.about_order not in ("en-zh", "zh-en"):
                raise ConfigError("about.order must be en-zh or zh-en")

        # actions: the major version each GitHub Action must be pinned to.
        actions = data.get("actions", {})
        self.actions_globs: List[str] = []
        self.actions_require: Dict[str, str] = {}
        self.actions_allow_sha = True
        if actions:
            if not isinstance(actions, dict):
                raise ConfigError("actions must be an object")
            self.actions_globs = [
                _as_str(item, "actions.globs[%d]" % index)
                for index, item in enumerate(
                    _as_list(
                        actions.get("globs", [".github/workflows/*.yml", ".github/workflows/*.yaml"]),
                        "actions.globs",
                    )
                )
            ]
            require = actions.get("require", {})
            if not isinstance(require, dict):
                raise ConfigError("actions.require must be an object")
            for key, value in require.items():
                self.actions_require[_as_str(key, "actions.require key")] = _as_str(
                    value, "actions.require[%s]" % key
                )
            self.actions_allow_sha = _as_bool(actions.get("allow_sha", True), "actions.allow_sha")

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


NON_LATIN = re.compile(
    "["
    "\u3040-\u30ff"  # Hiragana, Katakana
    "\u3400-\u4dbf"  # CJK extension A
    "\u4e00-\u9fff"  # CJK unified ideographs
    "\uac00-\ud7af"  # Hangul syllables
    "\uff00-\uffef"  # fullwidth forms
    "\u3000-\u303f"  # CJK punctuation
    "]"
)

C_STYLE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".js", ".mjs"}
HASH_SUFFIXES = {".py", ".sh", ".bash", ".ps1", ".psm1", ".yml", ".yaml", ".rb", ".pl"}
BATCH_SUFFIXES = {".bat", ".cmd"}


def comment_segments(text: str, suffix: str) -> List[Tuple[int, str]]:
    """Return (line number, comment text) for whole-line and trailing comments.

    Deliberately conservative: only shapes that are unambiguously comments, so the
    check never fires on a string literal. C-style blocks are tracked across lines.
    """
    segments: List[Tuple[int, str]] = []
    in_block = False
    for number, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if not stripped:
            continue
        if suffix in C_STYLE_SUFFIXES:
            if in_block:
                segments.append((number, stripped))
                if "*/" in stripped:
                    in_block = False
                continue
            if stripped.startswith("/*"):
                segments.append((number, stripped))
                if "*/" not in stripped:
                    in_block = True
                continue
            if stripped.startswith("//"):
                segments.append((number, stripped))
                continue
            index = line.find("//")
            if index > 0 and line[:index].count('"') % 2 == 0:
                segments.append((number, line[index:]))
            continue
        if suffix in BATCH_SUFFIXES:
            if stripped.upper().startswith("REM ") or stripped.startswith("::"):
                segments.append((number, stripped))
            continue
        if suffix in HASH_SUFFIXES:
            if stripped.startswith("#"):
                segments.append((number, stripped))
                continue
            index = line.find("#")
            if index > 0 and line[:index].count('"') % 2 == 0 and line[:index].count("'") % 2 == 0:
                segments.append((number, line[index:]))
    return segments


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

# `- uses: owner/action@v7` / `uses: owner/action@<sha>` inside a workflow.
USES_LINE = re.compile(r"^\s*(?:-\s*)?uses:\s*([A-Za-z0-9_.\-]+/[A-Za-z0-9_.\-/]+)@(\S+)")
SHA_REF = re.compile(r"^[0-9a-f]{40}$")


def action_major(ref: str) -> Optional[int]:
    """The major number of a tag such as `v7`, `7` or `v7.0.1`; None if unparsable."""
    match = re.match(r"^v?(\d+)(?:\.|$)", ref)
    return int(match.group(1)) if match else None


BADGE_IMAGE = re.compile(r"!\[[^\]]*\]\(([^)\s]+)")


def badge_block(text: str) -> Tuple[int, List[str]]:
    """The shield block directly under the title (or under the language line).

    Returns (1-based line number of the first badge, badge lines). Blank lines are
    skipped; the block ends at the first line that is neither blank nor a badge,
    which is how "badges at the top" is told apart from "badges somewhere".
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
        return 0, []

    number, switch = language_switch_line(text)
    is_switch = bool(switch) and switch.startswith("**") and "](" in switch
    start = number if is_switch else heading + 1  # 0-based index of the next line

    badges: List[str] = []
    for line in lines[start:]:
        stripped = line.strip()
        if not stripped:
            continue
        if "![" in stripped and "](" in stripped:
            badges.append(stripped)
            continue
        break
    return (start + 1 if badges else 0), badges


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
            self._check_comments(path, rel)

        self._check_versions()
        self._check_changelog()
        self._check_required()
        self._check_markdown(files)
        self._check_bilingual()
        self._check_languages()
        self._check_readme_badges()
        self._check_about()
        self._check_actions(files)
        if self.config.translation_pairs:
            self._check_translations()
            if self.config.translation_cjk_wrap:
                self._check_cjk_wrap()
            if self.config.translation_keep_language:
                self._check_lang_links()
        if self.config.translation_cover:
            self._check_translation_coverage()
        if self.config.translation_untracked:
            self._check_translation_untracked()
        self._check_gitattributes()
        return self.report

    def _check_actions(self, files: Sequence[Path]) -> None:
        """Every listed GitHub Action must use the configured major version."""
        if not self.config.actions_require:
            return
        for path in files:
            rel = rel_path(self.config.root, path)
            if self.config.is_skipped(rel):
                continue
            if not any(match_glob(rel, pattern) for pattern in self.config.actions_globs):
                continue
            try:
                text = path.read_bytes().decode("utf-8-sig", errors="replace")
            except OSError:
                continue
            for number, line in enumerate(text.splitlines(), 1):
                match = USES_LINE.match(line)
                if not match:
                    continue
                action, ref = match.group(1), match.group(2)
                wanted = self.config.actions_require.get(action)
                if wanted is None:
                    continue
                where = "%s:%d" % (rel, number)
                if SHA_REF.match(ref):
                    if self.config.actions_allow_sha:
                        continue
                    self.report.bad(
                        where,
                        "actions",
                        "%s is pinned by a commit SHA; this repository expects @%s"
                        % (action, wanted),
                    )
                    continue
                actual_major = action_major(ref)
                wanted_major = action_major(wanted)
                if actual_major is not None and wanted_major is not None:
                    if actual_major == wanted_major:
                        continue
                elif ref == wanted:
                    continue
                self.report.bad(
                    where,
                    "actions",
                    "%s@%s, expected @%s" % (action, ref, wanted),
                )

    def _check_readme_badges(self) -> None:
        if not self.config.badge_files:
            return
        for name in self.config.badge_files:
            path = self.config.root / name
            if not path.is_file():
                continue  # a missing file is reported by the languages/required rules
            text = path.read_bytes().decode("utf-8-sig", errors="replace")
            number, badges = badge_block(text)
            if not badges:
                self.report.bad(
                    name,
                    "readme-badges",
                    "no badge block directly under the title (or under the language line) - "
                    "put the shields above the first paragraph",
                )
                continue
            found = sum(len(BADGE_IMAGE.findall(line)) for line in badges)
            where = "%s:%d" % (name, number)
            if found < self.config.badge_min:
                self.report.bad(
                    where,
                    "readme-badges",
                    "%d badge(s), expected at least %d" % (found, self.config.badge_min),
                )
            joined = "\n".join(badges)
            seen: Dict[str, int] = {}
            for target in BADGE_IMAGE.findall(joined):
                seen[target] = seen.get(target, 0) + 1
            for target, count in seen.items():
                if count > 1:
                    self.report.bad(
                        where,
                        "readme-badges",
                        "the same badge appears %d times: %s" % (count, target),
                    )
            for pattern in self.config.badge_required:
                if pattern not in joined:
                    self.report.bad(where, "readme-badges", "missing required badge: %s" % pattern)

    def _check_about(self) -> None:
        """The declared About description: one ` | ` separator, and the language order."""
        if not self.config.about_description and self.config.about_order == "en-zh":
            return  # block absent
        text = self.config.about_description.strip()
        if not text:
            self.report.bad("repo-rules.json", "about", "about.description is empty")
            return
        parts = [part.strip() for part in text.split("|")]
        if len(parts) != 2 or not all(parts):
            self.report.bad(
                "repo-rules.json",
                "about",
                "about.description must be `left | right` with exactly one pipe and no empty side",
            )
            return
        left, right = parts
        if self.config.about_order == "en-zh":
            first_lang, second_lang = "English", "Chinese"
            ok_first, ok_second = not NON_LATIN.search(left), bool(NON_LATIN.search(right))
        else:
            first_lang, second_lang = "Chinese", "English"
            ok_first, ok_second = bool(NON_LATIN.search(left)), not NON_LATIN.search(right)
        if not ok_first:
            self.report.bad(
                "repo-rules.json",
                "about",
                "about.order is %s, so the left side must be %s: %s"
                % (self.config.about_order, first_lang, left),
            )
        if not ok_second:
            self.report.bad(
                "repo-rules.json",
                "about",
                "about.order is %s, so the right side must be %s: %s"
                % (self.config.about_order, second_lang, right),
            )

    def _check_comments(self, path: Path, rel: str) -> None:
        if not self.config.comment_globs:
            return
        if not any(match_glob(rel, glob) for glob in self.config.comment_globs):
            return
        if any(match_glob(rel, glob) for glob in self.config.comment_allow_files):
            return
        try:
            text = path.read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            return
        marker = self.config.comment_allow_marker
        for number, comment in comment_segments(text, path.suffix.lower()):
            if marker and marker in comment:
                continue
            if NON_LATIN.search(comment):
                self.report.bad(
                    "%s:%d" % (rel, number),
                    "comments",
                    "comment is not English: write code comments in English "
                    "(append `%s` on the line for a quotation or a data sample)" % marker,
                )

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

        if cfg["switch_line"]:
            self._check_language_switch_lines(cfg, ordered, existing)
        self._check_language_staleness(cfg, existing)

    def _check_language_switch_lines(self, cfg, ordered, existing) -> None:
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

    def _check_language_staleness(self, cfg, existing) -> None:
        """Each variant records the default file's hash; a changed default means a stale variant."""
        if cfg["stale"] == "off":
            return
        source_name = cfg["default"]
        source = self.config.root / source_name
        if not source.is_file():
            return  # already reported as a missing variant
        current = hashlib.sha256(source.read_bytes()).hexdigest()[:12]
        marker = self.config.translation_marker
        for name in existing:
            if name == source_name:
                continue
            text = (self.config.root / name).read_bytes().decode("utf-8-sig", errors="replace")
            recorded = recorded_translation_sha(text, marker)
            if recorded is None:
                self.report.bad(
                    name,
                    "languages",
                    "no `<!-- %s %s sha256:... -->` marker - markdown.languages.stale is on"
                    % (marker, source_name),
                )
                continue
            if recorded == current:
                continue
            message = (
                "translated from %s at sha256:%s, but the source is now sha256:%s - "
                "re-read the change and update the marker" % (source_name, recorded, current)
            )
            if cfg["stale"] == "error":
                self.report.bad(name, "language-stale", message)
            else:
                self.report.warn(name, "language-stale", message)

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

    def _check_translation_coverage(self) -> None:
        """A declared directory pair must be complete: no source doc without its translation."""
        for entry in self.config.translation_cover:
            source_root = self.config.root / entry["from"]
            target_root = self.config.root / entry["to"]
            if not source_root.is_dir():
                self.report.bad(entry["from"], "translation-cover", "cover source directory does not exist")
                continue
            for path in sorted(source_root.rglob("*.md")):
                relative = path.relative_to(source_root).as_posix()
                if any(match_glob(relative, glob) for glob in entry["ignore"]):
                    continue
                if (target_root / relative).is_file():
                    continue
                where = "%s/%s" % (entry["to"].rstrip("/"), relative)
                message = (
                    "no translation next to %s/%s - translate it, or list it in "
                    "markdown.translations.cover[].ignore to record that it is English-only by design"
                    % (entry["from"].rstrip("/"), relative)
                )
                if entry["missing"] == "warning":
                    self.report.warn(where, "translation-cover", message)
                else:
                    self.report.bad(where, "translation-cover", message)

    def _check_translation_untracked(self) -> None:
        """A file that looks like a translation must be registered in `pairs`."""
        tracked = {target for _source, target in self.config.translation_pairs}
        # A README tracked by markdown.languages carries a translated-from marker
        # too, so it must not be reported as untracked here.
        if self.config.languages:
            tracked.add(self.config.languages["default"])
            tracked.update(self.config.languages["variants"])
        candidates: List[str] = []

        for entry in self.config.translation_cover:
            source_root = self.config.root / entry["from"]
            target_root = self.config.root / entry["to"]
            if not source_root.is_dir() or not target_root.is_dir():
                continue
            for path in sorted(target_root.rglob("*.md")):
                relative = path.relative_to(target_root).as_posix()
                if (source_root / relative).is_file():
                    candidates.append("%s/%s" % (entry["to"].rstrip("/"), relative))

        suffix = self.config.translation_suffix
        if suffix:
            for path in sorted(self.config.root.rglob("*" + suffix)):
                if self.config.is_skipped(rel_path(self.config.root, path)):
                    continue
                candidates.append(rel_path(self.config.root, path))

        for name in candidates:
            if name in tracked:
                continue
            self.report.bad(
                name,
                "translation-untracked",
                "looks like a translation but is not listed in markdown.translations.pairs, "
                "so it is never checked for staleness",
            )

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

    msg_hook = root / ".githooks" / "commit-msg"
    if msg_hook.exists() and not args.force:
        print("kept %s (already exists)" % msg_hook)
    else:
        msg_hook.parent.mkdir(parents=True, exist_ok=True)
        msg_hook.write_bytes(COMMIT_MSG_HOOK.encode("utf-8"))
        try:
            msg_hook.chmod(0o755)
        except OSError:
            pass
        print("wrote %s" % msg_hook)

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


def command_check_msg(args: argparse.Namespace) -> int:
    """Reject a commit message that is not English (run from the commit-msg hook)."""
    path = Path(args.message_file)
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print("repo-rules: cannot read the message file: %s" % exc, file=sys.stderr)
        return 2

    root = Path(getattr(args, "root", ".")).resolve()
    config_path = root / getattr(args, "config", CONFIG_NAME)
    try:
        config = RuleConfig.load(root, config_path.name)
    except ConfigError as exc:
        print("repo-rules: %s" % exc, file=sys.stderr)
        return 2

    if not config.commit_message_english:
        return 0

    marker = config.commit_message_marker
    subject = next((line for line in text.splitlines() if line.strip()), "")
    if marker and marker in text:
        return 0
    if not NON_LATIN.search(text):
        return 0

    print("repo-rules: the commit message must be English", file=sys.stderr)
    print("", file=sys.stderr)
    print("  subject: %s" % subject, file=sys.stderr)
    print("", file=sys.stderr)
    print(
        "Rewrite it in English. To quote a title or a data sample, put `%s` in the message."
        % marker,
        file=sys.stderr,
    )
    return 1


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
        jobs.append((root / ".githooks" / "commit-msg", COMMIT_MSG_HOOK.encode("utf-8"), True))

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

    check_msg = subparsers.add_parser("check-msg", help="verify a commit message is English")
    check_msg.add_argument("message_file", help="path of the message file (git passes it to commit-msg)")
    check_msg.add_argument("--config", default=CONFIG_NAME, help="configuration path relative to --root")
    check_msg.add_argument("--root", default=".", help="repository root (default: current directory)")
    check_msg.set_defaults(func=command_check_msg)

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
