# Architecture

**English** | [简体中文](ARCHITECTURE.md)

## Design boundary

This project is a Windows Explorer `IContextMenu` Shell extension targeting Windows 7 SP1 and later. The COM DLL owns the menu lifecycle, JSON owns changeable configuration, and C++ owns gates, executors, Shell target recognition, and registry lifecycle.

## Runtime flow

```text
Explorer calls IShellExtInit::Initialize
  -> ContextBuilder builds MenuContext
     - selected paths, names, extensions, attributes, ProgID
     - ShellTargetType: file / directory / directoryBackground / drive / fileSystemObject
  -> Gate 1: global extensionGates
  -> per-item target matching and Gate 2: itemGates
  -> candidateItems

Explorer calls IContextMenu::QueryContextMenu
  -> Gate 3: presentationGates
  -> Hidden is omitted; Disabled is inserted disabled; Enabled is inserted normally
  -> command IDs are assigned to insertedItems
  -> IconProviderRegistry loads SVG/BMP/default icons

Explorer calls IContextMenu::InvokeCommand
  -> map command offset or verb to insertedItem
  -> skip Disabled items
  -> execute the configured executor chain
```

If Gate 1 rejects the context or no candidate remains, the extension returns `E_FAIL`. Gate 3 only controls the final presentation state. Configuration is cached after its first load in the process.

## Shell registration targets

`config/registration.json` is read by `DllRegisterServer`:

| Abstract target | Registry location | Runtime recognition |
|-----------------|-------------------|---------------------|
| `file` | `*` | File-only selection |
| `directory` | `Directory` | Directory-only selection |
| `directoryBackground` | `Directory\\Background` | Empty selection with a current directory |
| `drive` | `Drive` | One drive-root path such as `C:\\` |
| `fileSystemObject` | `AllFilesystemObjects` | A mixed file-and-directory selection |

The `AllFilesystemObjects` registration covers the Windows Shell scope, while the current Demo `fileSystemObject` menu item is shown only for mixed selections to avoid duplicating the single-file `-F` and single-directory `-D` items.

Registration behavior:

1. Register the COM `InprocServer32` entry.
2. Read and validate `registration.json`.
3. Register each configured target; roll back this registration on failure.
4. Remove known legacy targets that are not enabled; if cleanup fails, keep the current registration and log the failure.
5. Unregistration removes all known targets and this project's CLSID; missing registry keys are treated as success.

A missing configuration defaults to `file`. An existing but invalid configuration fails registration instead of silently changing the registry. Targets must be unique and known to the validation script.

## MenuContext and target classification

`ContextBuilder` constructs one `MenuContext` per context-menu request. Each selected item includes its full path, name, extension, directory flag, and file attributes.

- No selection with `folderPath`: `DirectoryBackground`
- Directory-only selection: `Directory`; one drive-root path becomes `Drive`
- Both files and directories selected: `FileSystemObject`
- Other file selection: `File`

Gates and executors read the constructed context and should not repeat filesystem queries.

## Configuration model

Root chains can be overridden per menu item:

```json
{
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter"],
  "presentationGates": ["presentationPass"],
  "executors": ["messageBox"],
  "menuItems": [
    {
      "id": "example",
      "label": "&Example",
      "verb": "example",
      "targets": ["file"],
      "icon": "icons/example.svg",
      "actionType": "messageBox",
      "actionTitle": "Example",
      "actionTemplate": "Selected: %N"
    }
  ]
}
```

Target matching runs before gates and selection filters. An empty `targets` array preserves the generic filter behavior. Actions support `messageBox` and `launch`; `%1`, `%*`, `%D`, and `%N` are expanded before execution.

## Gates and executors

| Stage | Interface | When | Result |
|-------|-----------|------|--------|
| Extension | `IExtensionGate` | Initialize | Allow or reject extension participation |
| Item | `IMenuItemGate` | Initialize | Include or exclude a candidate item |
| Presentation | `IMenuItemPresentationGate` | QueryContextMenu | Hidden, Disabled, or Enabled |

Extension and Item chains use AND semantics. Presentation states merge with Hidden taking precedence over Disabled, and Disabled taking precedence over Enabled.

To add a gate, implement the appropriate interface under `src/gates/` and register its name in `GateRegistry::RegisterBuiltInGates`. To add an executor, implement `IActionExecutor` under `src/actions/` and register it in `ExecutorRegistry::RegisterBuiltInExecutors`.

## Icon loading

`IconProviderRegistry` tries providers in order:

1. SVG: `SvgIconProvider` loads a DLL-directory-relative SVG and rasterizes it with NanoSVG at the requested DPI.
2. BMP: `BitmapFileIconProvider` loads a bitmap file.
3. Empty or failed icon: `DefaultResourceIconProvider` supplies the resource fallback.

The five Demo icons are independent SVG files: `file-F.svg`, `directory-D.svg`, `directoryBackground-DB.svg`, `drive-DR.svg`, and `fileSystemObject-FS.svg`. Keep icon artwork outside C++ code.

## Source layout

```text
src/extension/  COM DLL entry, ClassFactory, FileContextMenuExt
src/context/   MenuContext, ContextBuilder, ShellTargetType
src/gates/     Gate interfaces, registry, built-in gates
src/actions/   Executor interfaces, registry, actions
src/icons/     SVG/BMP/default providers
src/menu/      configuration, filtering, insertion, invocation
src/registry/  COM/Shell registry helpers and registration parser
config/        menu.json, registration.json, icons/
tools/         build helpers, register.ps1, BAT templates, validation
```

## Extension workflow

1. Edit `config/menu.json` or `config/registration.json`.
2. When adding a gate, executor, or Shell target, update `CMakeLists.txt`, validation, and documentation together.
3. Build Release and run `python tools/validate_menu_json.py`.
4. Re-register with the scripts in the output directory.
5. Test file, directory, directory-background, drive, and mixed-selection contexts.

## Validation checklist

### Automated

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
git diff --check
```

CI runs the Release build and menu validation on push/PR. Hand-written C/C++ also follows the root `.clang-format` and `.cursor/rules/` encoding, naming, and parameter rules.

### Manual

| Scenario | Expected |
|----------|----------|
| File context | File-target menu such as `Demo -F` |
| Directory context | `Demo -D` |
| Directory background | `Demo -DB` |
| Drive root | `Demo -DR` |
| Mixed file/directory selection | `Demo -FS` |
| MessageBox | `%N` shows the name without the parent path |
| Read-only/temp contexts | Configured gates hide or disable items |
| High DPI | SVG icons remain clear and layout stays stable |
| After unregister | This project's targets and CLSID are removed |

## Platform and safety boundaries

- DLL bitness must match Explorer.
- Registration and unregistration require elevation.
- Registry operations delete only this project's CLSID keys.
- Gates must not perform disk scans or network I/O in the menu request path.
- Validate Windows 7 support in a VM before release.
