# Contributing

**English** | [简体中文](CONTRIBUTING.md)

Thanks for improving **ShellExtContextMenuHandler**.

## Development setup

- Windows x64
- Visual Studio 2026 C++ toolchain
- CMake 3.20 or later
- Python 3.9 or later

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

Before pushing, run the repository rules and the checker tests:

```powershell
python scripts/repo_rules.py check --config repo-rules.json
python -m unittest discover -s scripts/tests -v
```

Install the optional pre-commit hook:

```powershell
git config core.hooksPath .githooks
```

## Coding guidelines

- Hand-written C/C++ is UTF-8 BOM + CRLF; see [AGENTS.md](AGENTS.md).
- Follow `.clang-format` and `.editorconfig`; the column limit is 140.
- Link Windows libraries through CMake `target_link_libraries`, not `#pragma comment(lib)`.
- Add tests for behavior changes; update `tools/validate_menu_json.py` and docs when configuration changes.
- Record user-visible changes under `[Unreleased]` in `CHANGELOG.md`.
- Do not commit `build/`, `cmake-build-*` or other generated output.

## Pull requests

1. Branch from `master`.
2. Keep a PR focused on one concern.
3. Ensure CI, CTest, config validation and repository rules are green.
4. Fill out the pull request template.

## Reporting bugs

Use the bug report issue template and include the Windows version, DLL bitness, reproduction steps, expected behavior and actual behavior. Report security issues privately as described in [SECURITY.md](SECURITY.md).
