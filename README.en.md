# ShellExtContextMenuHandler

**English** | [简体中文](README.md)

A **configurable context menu module** derived from the official Microsoft C++ Shell extension sample. Define menu items, filters, gate/executor chains, and icons in `menu.json` without changing core COM code.

Based on: [CppShellExtContextMenuHandler](https://code.msdn.microsoft.com/windowsapps/CppShellExtContextMenuHandl-410a709a)

## Features

- Registers all supported Shell targets by default, with runtime target filters and gates controlling visibility
- Multiple menu items and gate/executor chains via `config/menu.json`
- Shell registration targets via `config/registration.json`: `file`, `directory`, `directoryBackground`, `drive`, and `fileSystemObject`
- `messageBox` and `launch` actions plus custom executors
- Filters: extension, selection count, files vs folders
- Demo coverage for file, directory, directory background, drive, and file system object targets
- SVG/BMP icons with DPI-aware scaling
- Built-in demo gates (`demo:hideTemp`, `demo:readOnlyDisable`) and `demo:actionLog` executor
- Debug logging via `OutputDebugString` ([DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview))
- **CMake** build; GitHub Actions CI for Release build and `menu.json` validation
- **Layered Gate architecture** — see [docs/ARCHITECTURE.en.md](docs/ARCHITECTURE.en.md)
- Build output includes double-clickable `register.bat` and `unregister.bat` scripts with UAC elevation

## Project layout

```
├── CMakeLists.txt
├── .github/workflows/ci.yml
├── docs/
├── config/menu.json, config/registration.json, config/icons/
├── src/extension, context, gates, actions, platform, icons, menu, registry, resources
├── tools/generate-vs.ps1, register.ps1, validate_menu_json.py
└── build/             # generated (gitignored)
```

## Quick start

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
.\tools\register.ps1 -Action register   # administrator
```

The build output contains `register.bat`, `unregister.bat`, and the configuration and icon files beside the DLL.

Right-click a `.cpp` file in Explorer to verify. Full manual checklist: [docs/ARCHITECTURE.en.md](docs/ARCHITECTURE.en.md).

## Customize menus

Edit `config/menu.json`.

### Root-level chains (optional)

| Field | Description |
|-------|-------------|
| `extensionGates` | Gate 1 chain (default `extensionPass`) |
| `itemGates` | Gate 2 chain (default `jsonFilter`); `gates` is an alias |
| `presentationGates` | Gate 3 chain (default `presentationPass`) |
| `executors` | Action chain (default `messageBox`, `launch`) |

### Per-item fields

Includes `id`, `label`, `verb`, `icon`, optional `targets`, filter fields, action fields, and optional per-item `extensionGates` / `itemGates` / `presentationGates` / `executors` overrides.

### Shell registration targets

Edit the `shellRegistrations` array in `config/registration.json`. The demo enables all five supported targets by default.
The abstract targets map to registry locations as follows: `file` to `*`, `directory` to `Directory`, `directoryBackground` to `Directory\\Background`, `drive` to `Drive`, and `fileSystemObject` to `AllFilesystemObjects`. To avoid duplicate menu entries, the `fileSystemObject` demo is shown only for mixed file-and-directory selections; single files and directories use `-F` and `-D` respectively.

### Placeholders

| Token | Meaning |
|-------|---------|
| `%1` | First selected path, or current folder |
| `%*` | All selected paths (quoted) |
| `%N` | Name of the first selected object or current folder |
| `%N` | Name of the first selected object or current folder |
| `%D` | Parent directory |

## COM identity

When shipping a fork, change CLSID and friendly names in `dllmain.cpp` and `include/shell_ext/common.h`.

## Notes

- 64-bit Explorer requires a 64-bit DLL
- COM registration needs elevation
- Complete the Win7 SP1 x64 manual checklist in the architecture doc before release

## License

Derived from the Microsoft sample ([Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)).
