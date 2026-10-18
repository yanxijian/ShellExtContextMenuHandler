# Changelog

All notable changes to this project are documented here. Change log entries
follow the spirit of Keep a Changelog; versions use the DLL/`project(... VERSION)`
number.

## [Unreleased]

_No unreleased changes yet._

## [1.0.0] - 2026-10-18

### Added

- Configurable Explorer context-menu extension with file, directory, background,
  drive and filesystem-object targets.
- Strict JSON validation for menu and registration configuration.
- Gate/executor chains, SVG/BMP icons and DPI-aware menu bitmaps.
- COM exception firewalls, ANSI/Unicode verb handling, long-path support and
  transactional registration with `SHChangeNotify`.
- Static CRT linking, version resource, installation scripts and license notices.
- CTest suite and CI matrix for x64 Release/Debug and x86 Release.
- Repository rules checker, pre-commit hook and hygiene CI.
- Changelog and project-local agent instructions.
- Contributing, security and issue/PR templates.

### Changed

- Standardized C/C++ source encoding/line endings and removed `#pragma comment(lib)`.
- Documented the repository's build, validation, release and contribution rules.

[Unreleased]: https://github.com/yanxijian/ShellExtContextMenuHandler/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/yanxijian/ShellExtContextMenuHandler/releases/tag/v1.0.0
