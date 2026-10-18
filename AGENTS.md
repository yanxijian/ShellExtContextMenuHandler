# Agent instructions

This file is the project-local source of truth for agents working on
`ShellExtContextMenuHandler`. The workspace root `AGENTS.md` still applies; this
file only narrows it for this repository.

Rules marked **[CI]** are enforced by `scripts/check_repo_rules.py` and/or the
hygiene workflow. Rules marked **[review]** need judgement.

## Repository

A Windows Explorer `IContextMenu` shell extension built as a COM in-proc DLL.

- `config/` - menu and registration JSON, plus icons.
- `src/extension/` - COM entry point, class factory, handler object, exception guard.
- `src/context/` - selection context and shell target classification.
- `src/gates/`, `src/actions/`, `src/icons/`, `src/menu/` - extension points.
- `src/registry/` - registry lifecycle and registration config.
- `tests/` - CTest suite; `tools/` - scripts and config validation.

## Build and test

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

Run the repository rules before pushing:

```powershell
python scripts/check_repo_rules.py
python -m unittest discover -s scripts/tests -v
```

Install the optional pre-commit hook once per clone:

```powershell
git config core.hooksPath .githooks
```

## Hard rules

### R1 - C/C++ is UTF-8 BOM + CRLF **[CI]**

Every hand-written `.h/.hpp/.c/.cc/.cpp` file is UTF-8 with BOM and CRLF.
`src/resources/resource.h` is UTF-16LE and is excluded. Use
`python scripts/check_repo_rules.py --fix` to repair BOM/line endings.

### R2 - 140 columns, `.clang-format` is authoritative **[CI/review]**

`ColumnLimit: 140`, tabs for indentation, CRLF. The checker enforces the width
limit; exact formatting of touched files follows the repository `.clang-format`.
Do not reformat unrelated files.

### R3 - Windows libraries through CMake **[CI]**

Do not use `#pragma comment(lib, ...)` or `#pragma comment(linker, ...)`. Add the
library to `target_link_libraries` so CMake, Ninja and non-MSVC toolchains see it.

### R4 - Headers are self-contained **[CI]**

`#pragma once` is the first significant line. Do not put `using namespace` at
namespace scope. Keep includes needed by only one `.cpp` in that `.cpp`.

### R5 - One version **[CI]**

`project(... VERSION x.y.z)` in `CMakeLists.txt`,
`SHELLEXT_VERSION_*` in `include/shell_ext/common.h`, and `FILEVERSION` in
`src/resources/Version.rc` must agree.

### R6 - Changelog **[CI]**

Keep a `## [Unreleased]` section in `CHANGELOG.md`. Every `## [x]` heading needs
a matching `[x]: <url>` definition. Record behavior changes and new knobs there.

### R7 - Line endings follow `.gitattributes` **[CI]**

| Path | Repository and checkout |
|---|---|
| `*.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx` | UTF-8 BOM + CRLF, `-text` |
| `*.md` | CRLF |
| `*.bat *.cmd` | CRLF |
| CMake, scripts, JSON, YAML, `.def`, `.in` | LF |
| `src/resources/resource.h`, main `.rc` | UTF-16LE, `-text` |

### R8 - Documentation links resolve **[CI]**

Relative Markdown links must resolve. Bilingual pairs (`README.md` /
`README.en.md`, architecture docs, contributing/security) must link to each other.

## Review and change discipline

- Correctness first, then lifetime/resource ownership, threading, compatibility,
  maintainability, style. List deliberate non-changes.
- Keep a change focused. Do not mass-format or rename unrelated code.
- A behavior/config change updates tests, `tools/validate_menu_json.py` when
  applicable, README/architecture docs and `CHANGELOG.md`.
- New registry/COM paths need partial-failure rollback, idempotency and legacy
  cleanup; never delete a shared parent key.
- Before commit: build, ctest, menu validation, repository rules and
  `git diff --check` (with `core.whitespace=cr-at-eol` for CRLF files).
