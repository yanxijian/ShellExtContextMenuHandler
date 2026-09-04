# ShellExtContextMenuHandler

**English** | [简体中文](README.md)

A configurable Windows Explorer context-menu extension derived from Microsoft's C++ Shell extension sample. Menu items, Shell registration targets, filters, gates, executors, and icons are configurable; complex behavior stays in C++.

## Disclaimer

This project was developed primarily with the assistance of AI Agents. Parts of the code, design, and documentation have not undergone sufficient, rigorous, or comprehensive testing. The project is provided for learning, research, and technical reference only; correctness, stability, and security are not guaranteed for all systems, environments, or use cases.

Before using this project in an important project, commercial software, or production environment, perform appropriate code review, customization, security assessment, and compatibility testing. The project author assumes no responsibility for data loss, system failures, business interruptions, or any other direct or indirect losses resulting from the use of this project.

## Features

- Registers five Shell target types by default: file, directory, directory background, drive, and file-system object.
- Configures menu items, target filters, selection filters, gate/executor chains, and actions in `config/menu.json`.
- Configures COM/Shell registration targets in `config/registration.json`.
- Supports `messageBox` and `launch` actions plus custom executors.
- Supports SVG/BMP icons; SVG files are loaded and rasterized by the existing NanoSVG provider with DPI scaling.
- Three gate stages: Extension, Item, and Presentation.
- Built-in gates: `extensionPass`, `jsonFilter`, `presentationPass`, `demo:hideTemp`, `demo:readOnlyDisable`; built-in executors: `messageBox`, `launch`, `demo:actionLog`.
- Build output includes double-clickable `register.bat` and `unregister.bat` scripts with UAC elevation.
- Debug logging through `OutputDebugString` with the `[ShellExt]` prefix.

## Quick Start

### Requirements

- Windows x64
- Visual Studio 2026 C++ toolchain
- CMake 3.20 or later
- Python 3 for configuration validation

Windows 7 SP1 and later are the design targets. Validate the final DLL on the Windows version and Explorer bitness you intend to support.

### Build

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
```

`tools/generate-vs.ps1` can also be used to generate the Visual Studio project.

The output is in `build/bin/Release/`:

```text
CppShellExtContextMenuHandler.dll
menu.json
registration.json
register.bat
unregister.bat
icons/
```

### Register, Test, and Unregister

Double-click `register.bat` in the DLL output directory. It uses the DLL beside the batch file and requests elevation before calling `regsvr32`. Double-click `unregister.bat` to remove the extension.

Alternatively, use an elevated PowerShell:

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister
```

`register.ps1` copies `menu.json`, `registration.json`, and `config/icons/` beside the DLL. After changing the DLL or configuration, unregister and register again; restart Explorer if its cache has not refreshed.

### Manual smoke test

1. Run `unregister.bat`, then run `register.bat`.
2. Right-click a regular file and verify `Demo -F`.
3. Right-click a directory and verify `Demo -D`.
4. Right-click empty space in a directory and verify `Demo -DB`.
5. Right-click a drive root and verify `Demo -DR`.
6. Select both a file and a directory and verify `Demo -FS`.
7. Invoke an item and verify that the MessageBox shows the target name.

## Configuration

### Shell registration targets

`config/registration.json`:

```json
{
  "schemaVersion": 1,
  "shellRegistrations": [
    "file",
    "directory",
    "directoryBackground",
    "drive",
    "fileSystemObject"
  ]
}
```

| Target | Registry location | Runtime context |
|--------|------------------|-----------------|
| `file` | `*` | File-only selection |
| `directory` | `Directory` | Directory-only selection |
| `directoryBackground` | `Directory\\Background` | Empty selection in a directory |
| `drive` | `Drive` | Drive root |
| `fileSystemObject` | `AllFilesystemObjects` | Mixed file-and-directory selection |

If `registration.json` is missing, registration defaults to `file`. If the file exists but has an invalid schema, unknown target, duplicate target, or invalid array, registration fails instead of silently changing the registry. Unregistration removes all known targets so old registrations can be cleaned up.

### Root-level `menu.json` fields

```json
{
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter"],
  "presentationGates": ["presentationPass"],
  "executors": ["messageBox"],
  "menuItems": []
}
```

| Field | Description |
|-------|-------------|
| `extensionGates` | Gate 1 chain; decides whether the extension participates |
| `itemGates` | Gate 2 chain; decides whether an item becomes a candidate; `gates` is an alias |
| `presentationGates` | Gate 3 chain; returns hidden, disabled, or enabled |
| `executors` | Default action executor chain |
| `menuItems` | Menu item definitions |

### Per-item fields

| Field | Description |
|-------|-------------|
| `id` | Unique identifier |
| `label` | Menu text; `&` marks an accelerator |
| `verb` | String command name |
| `helpText` | Explorer status-bar help text |
| `canonicalName` | Canonical command name |
| `icon` | SVG/BMP path relative to the DLL directory |
| `targets` | `file`, `directory`, `directoryBackground`, `drive`, or `fileSystemObject` |
| `separatorAfter` | Insert a separator after the item |
| `extensions` | Allowed extensions such as `[".cpp", ".h"]` |
| `excludeExtensions` | Extensions to reject |
| `minSelection` / `maxSelection` | Selection count; `maxSelection: 0` means unlimited |
| `filesOnly` / `foldersOnly` | Restrict selected object types |
| `actionType` | `messageBox` or `launch` |
| `actionTitle` / `actionTemplate` | MessageBox title and content |
| `actionCommand` / `actionShowWindow` | Launch command and window option |
| `extensionGates` / `itemGates` / `presentationGates` / `executors` | Per-item overrides of root chains |

An empty `targets` array preserves compatibility with the generic selection filters. New items should set `targets` explicitly.

### Placeholders

| Token | Meaning |
|-------|---------|
| `%1` | Full path of the first selected object, or current directory |
| `%*` | All selected paths, quoted and joined |
| `%D` | Parent directory of the first selected object, or current directory |
| `%N` | Name of the first selected object or current directory, without its parent path |

If `menu.json` is missing or invalid, the extension falls back to the built-in item in `include/shell_ext/common.h` and the default gate chain.

## Extending the project

### Add a menu item

1. Add an object to `menuItems` in `config/menu.json`.
2. Set a unique `id`, `verb`, `targets`, filters, and action fields.
3. Put SVG/BMP assets in `config/icons/` and reference them with a relative path.
4. Run `python tools/validate_menu_json.py`.

### Add a gate

1. Add an implementation of `IExtensionGate`, `IMenuItemGate`, or `IMenuItemPresentationGate` under `src/gates/`.
2. Register its name in `GateRegistry::RegisterBuiltInGates`.
3. Use the name in the appropriate chain in `menu.json`.

Extension gates decide whether the extension participates, item gates decide candidate membership, and presentation gates return `Hidden`, `Disabled`, or `Enabled`.

### Add an executor

1. Add an `IActionExecutor` implementation under `src/actions/`.
2. Register it in `ExecutorRegistry::RegisterBuiltInExecutors`.
3. Add its name to a root or per-item `executors` array.
4. Extend `MenuAction` and `MenuConfig` if the executor needs new configuration fields.

### Add a Shell target

1. Add an enum value to `ShellTargetType`.
2. Update `ParseShellTargetType` and `GetShellTargetRegistryFileType`.
3. Add runtime recognition in `ContextBuilder`.
4. Update target matching and the known registration/unregistration lists.
5. Update `tools/validate_menu_json.py`, README, and architecture docs.

### Change COM registration

Registration entry points are in `src/extension/dllmain.cpp`; registry helpers are in `src/registry/Reg.cpp`. Preserve rollback on partial registration, idempotent unregistration, and cleanup of legacy targets. Delete only this project's CLSID subkey, never an entire `ContextMenuHandlers` parent key.

## Debugging and troubleshooting

- Use DebugView to inspect `[ShellExt]` logs.
- Match the DLL bitness to Explorer.
- Re-register after changing the DLL or configuration; restart Explorer when necessary.
- If a menu is missing, check registration targets, item `targets`, `filesOnly/foldersOnly`, and gate chains together.
- If registration fails, check elevation, `registration.json`, and the `regsvr32` bitness.

## Project layout

```text
config/                 Runtime menu, registration targets, and icons
src/extension/          COM DLL entry point and Shell extension object
src/context/            Selection context and Shell target recognition
src/gates/              Gate interfaces, registry, and built-in gates
src/actions/            Executor interfaces, registry, and actions
src/icons/              SVG/BMP icon providers
src/menu/               Configuration, filtering, insertion, and invocation
src/registry/           COM/Shell registry helpers
tools/                  Build helpers, registration scripts, and validation
docs/                   Architecture and validation documentation
```

## Notes

- A 64-bit Explorer requires a 64-bit DLL; use the matching bitness for 32-bit Explorer.
- COM registration requires administrator privileges.
- Windows 7 support is a design target; complete the Win7 SP1 x64 regression before release.
- Do not edit generated files under `build/`, `build-*`, or third-party directories.

## COM identity and license

When shipping a fork, change `CLSID_FileContextMenuExt` in `src/extension/dllmain.cpp` and the friendly names in `include/shell_ext/common.h`.

This project is derived from Microsoft's sample. Source files retain the original Microsoft headers and use the [Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL).
