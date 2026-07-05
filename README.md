# ShellExtContextMenuHandler

**English** | [简体中文](README.zh-CN.md)

A **configurable context menu module** derived from the official Microsoft C++ Shell extension sample. Define menu items, filters, and actions in `menu.json` without changing core COM code.

Based on: [CppShellExtContextMenuHandler](https://code.msdn.microsoft.com/windowsapps/CppShellExtContextMenuHandl-410a709a)

## Features

- Registered for all file types (`*`), with runtime rules controlling visibility
- Multiple menu items via `config/menu.json`
- `messageBox` and `launch` (spawn process) actions
- Filters: extension, selection count, files vs folders
- Folder background context (current directory when nothing is selected)
- Debug logging via `OutputDebugString` ([DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview))
- **CMake** build with generated Visual Studio projects
- **Layered Gate architecture** (Extension / Item / Presentation) — see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

## Project layout

```
├── CMakeLists.txt
├── docs/
│   ├── ARCHITECTURE.md            # Architecture (English)
│   └── ARCHITECTURE.zh-CN.md      # 架构说明（简体中文）
├── README.md                      # English (default)
├── README.zh-CN.md                # 简体中文
├── config/menu.json
├── include/shell_ext/common.h
├── src/
│   ├── extension/     # Shell COM entry
│   ├── context/       # MenuContext, ContextBuilder
│   ├── gates/         # Gate interfaces, JsonFilterGate
│   ├── menu/          # MenuProvider, config, actions
│   ├── registry/
│   └── resources/
├── tools/
│   ├── generate-vs.ps1
│   └── register.ps1
└── build/             # generated (gitignored)
```

## Quick start

### 1. Generate Visual Studio projects

```powershell
.\tools\generate-vs.ps1
```

This creates `build/ShellExtContextMenuHandler.slnx` and `CppShellExtContextMenuHandler.vcxproj`.

Or manually:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

### 2. Build

```powershell
cmake --build build --config Release
```

Or open `build/ShellExtContextMenuHandler.slnx` in Visual Studio (`Release | x64`).

Output: `build/bin/Release/CppShellExtContextMenuHandler.dll` and `menu.json`.

### 3. Register

Run as administrator:

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister   # uninstall
```

`register.ps1` finds the DLL under `build/bin/Release/` and copies `config/menu.json` beside it.

### 4. Verify

Right-click a `.cpp` file in Explorer — configured menu items should appear.

## Customize menus

Edit `config/menu.json`:

| Field | Description |
|-------|-------------|
| `id` | Unique id |
| `label` | Menu text (`&` for shortcut key) |
| `verb` | Command verb for programmatic invoke |
| `helpText` | Status bar help |
| `canonicalName` | Canonical verb name |
| `separatorAfter` | Insert separator after this item |
| `extensions` | Allowed extensions, e.g. `[".cpp"]`; `[]` = no limit |
| `excludeExtensions` | Excluded extensions |
| `minSelection` | Minimum selection count (default `1`) |
| `maxSelection` | Maximum count; `0` = unlimited |
| `filesOnly` | Files only (default `true`) |
| `foldersOnly` | Folders / folder background only |
| `actionType` | `messageBox` or `launch` |
| `actionTitle` | Dialog title (`messageBox`) |
| `actionTemplate` | Dialog body template (`messageBox`) |
| `actionCommand` | Command line (`launch`) |
| `actionShowWindow` | Show window when launching (default `false`) |

### Placeholders

| Token | Meaning |
|-------|---------|
| `%1` | First selected path, or current folder if none |
| `%*` | All selected paths (quoted, space-separated) |
| `%D` | Parent directory |

If `menu.json` is missing or invalid, the extension falls back to built-in items in `include/shell_ext/common.h`.

## COM identity

When shipping your own fork, change:

1. `CLSID_FileContextMenuExt` in `src/extension/dllmain.cpp`
2. `L_Friendly_Class_Name` and `L_Friendly_Menu_Name` in `include/shell_ext/common.h`

## Debugging

Logs use prefix `[ShellExt]` via `OutputDebugString`. Use [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview).

## Notes

- **Bitness:** 64-bit Explorer requires a 64-bit DLL
- **Admin:** COM registration needs elevation
- **Regenerate:** Re-run `generate-vs.ps1` after `CMakeLists.txt` changes
- **Explorer cache:** Unregister/re-register after DLL changes; restart Explorer if needed

## Architecture (overview)

```
Right-click in Explorer
  → IShellExtInit::Initialize
  → IContextMenu::QueryContextMenu
  → IContextMenu::InvokeCommand
```

Shell COM glue stays thin; most customization is `config/menu.json` plus optional gates/executors. Details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## License

Derived from the Microsoft sample ([Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)). Original Microsoft copyright headers remain in source files.
