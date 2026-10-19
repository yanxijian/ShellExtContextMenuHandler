# Architecture

**English** | [简体中文](ARCHITECTURE.md)

## Design boundary

This project is a Windows Explorer `IContextMenu` Shell extension targeting Windows 7 SP1 and later. The COM DLL owns the menu lifecycle, JSON owns changeable configuration (`menu.json` and `registration.json`, both parsed with `third_party/nlohmann/json`, UTF-8, BOM tolerated), and C++ owns gates, executors, Shell target recognition, and registry lifecycle. Configuration loading is fail-closed: bad field types, unknown gates/executors, unknown `actionType`, duplicate ids, and similar errors reject the whole document and fall back to the built-in menu.

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
  -> map command offset or verb/canonicalName to insertedItem
  -> skip Disabled items
  -> execute the configured executor chain
```

If Gate 1 rejects the context or no candidate remains, the extension returns `E_FAIL`. Gate 3 only controls the final presentation state. Configuration is cached after its first load in the process and the cache is mutex-protected; unknown gate/executor names reject the document instead of acting as an unfiltered pass. Every COM entry point has a `try/catch(...)` firewall so exceptions cannot cross the ABI into Explorer.

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

1. Read and strictly validate `registration.json`. An invalid file fails without modifying the registry, so an existing installation stays usable.
2. Register the COM `InprocServer32` entry.
3. Register each configured target; roll back this registration on failure.
4. Remove known legacy targets that are not enabled; if cleanup fails, keep the current registration and log the failure.
5. Send `SHChangeNotify(SHCNE_ASSOCCHANGED, ...)` so the Shell refreshes its associations.
6. Unregistration removes all known targets and this project's CLSID; missing registry keys are treated as success and the same notification is sent.

A missing configuration defaults to `file`. An existing but invalid configuration fails registration instead of silently changing the registry. Targets must be unique and known to the validation script.

## MenuContext and target classification

`ContextBuilder` constructs one `MenuContext` per context-menu request. Each selected item includes its full path, name, extension, directory flag, and file attributes.

- No selection with `folderPath`: `DirectoryBackground`
- Directory-only selection: `Directory`; one drive-root path becomes `Drive`
- Both files and directories selected: `FileSystemObject`
- Other file selection: `File`

Gates and executors read the constructed context and should not repeat filesystem queries.

## Configuration model

Root chains can be overridden per menu item: an explicitly configured per-item chain **replaces** the root default entirely (it is not appended to); root defaults apply only when the chain is missing or empty:

```json
{
  "schemaVersion": 1,
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

`schemaVersion` is optional and currently only `1` is accepted. After parsing, the document is validated for field types, required fields, Shell targets, gate/executor names, `actionType`, duplicate ids, and `actionCommand` on `launch` items. Any validation failure rejects the whole file and falls back to the built-in menu, so a partial configuration cannot silently widen filters.

Target matching runs before gates and selection filters. An empty `targets` array preserves the generic filter behavior. Actions support `messageBox` and `launch`; `%1`, `%*`, `%D`, and `%N` are expanded before execution. `launch` explicitly resolves the executable first: quoted paths are used as-is, unquoted paths with spaces follow the CreateProcess prefix-progressive search, and bare command names are resolved from the system directories and absolute PATH entries only (never the current directory); unresolvable commands are logged and rejected. The command line is rebuilt with a quoted resolved application path so unquoted spaced executables keep their arguments. Placeholder expansion is single-pass, so replacement values containing `%1`/`%N` tokens are not expanded again. `%D` is computed with a dynamic `std::wstring` and is not limited by `MAX_PATH`.

Long paths: `DragQueryFileW` is called twice to obtain the real length; directory backgrounds prefer `SHGetPathFromIDListEx`; file attribute reads retry with a `\\?\` extended path. Actual paths longer than `MAX_PATH` still require the system long-path policy and a long-path-aware Explorer.

## Gates and executors

| Stage | Interface | When | Result |
|-------|-----------|------|--------|
| Extension | `IExtensionGate` | Initialize | Allow or reject extension participation |
| Item | `IMenuItemGate` | Initialize | Include or exclude a candidate item |
| Presentation | `IMenuItemPresentationGate` | QueryContextMenu | Hidden, Disabled, or Enabled |

Extension and Item chains use AND semantics. Presentation states merge with Hidden taking precedence over Disabled, and Disabled taking precedence over Enabled.

To add a gate, implement the appropriate interface under `src/gates/` and register its name in `GateRegistry::RegisterBuiltInGates`. To add an executor, implement `IActionExecutor` under `src/actions/` and register it in `ExecutorRegistry::RegisterBuiltInExecutors`.

## Concurrency and caches

`GateRegistry`, `ExecutorRegistry`, and `IconProviderRegistry` are constructed once on first use and are read-only afterwards; function-local static initialization is thread-safe. The process-wide `MenuConfig` cache is mutex-protected, and the DPI/OS caches use the same one-time-initialization pattern. Multiple Explorer UI threads therefore cannot race on mutable `std::map`/`std::wstring` state.

## Icon loading

`IconProviderRegistry` tries providers in order:

1. SVG: `SvgIconProvider` loads a DLL-directory-relative SVG and rasterizes it with NanoSVG at the requested DPI.
2. BMP: `BitmapFileIconProvider` loads a bitmap file.
3. Empty or failed icon: `DefaultResourceIconProvider` supplies the resource fallback.

The five Demo icons are independent SVG files: `file-F.svg`, `directory-D.svg`, `directoryBackground-DB.svg`, `drive-DR.svg`, and `fileSystemObject-FS.svg`. Keep icon artwork outside C++ code.

## Source layout

```text
src/extension/  COM DLL entry, ClassFactory, FileContextMenuExt, ComGuard
src/context/   MenuContext, ContextBuilder, ShellTargetType
src/gates/     Gate interfaces, registry, built-in gates
src/actions/   Executor interfaces, registry, actions
src/icons/     SVG/BMP/default providers
src/menu/      configuration, filtering, insertion, invocation
src/registry/  COM/Shell registry helpers and registration parser
config/        menu.json, registration.json, icons/
tools/         build helpers, register.ps1, BAT templates, validation
scripts/       repository rules checker and its unit tests
tests/         CTest unit tests (compile the real sources)
third_party/   third-party libraries (nlohmann/json)
```

## Extension workflow

1. Edit `config/menu.json` or `config/registration.json`.
2. When adding a gate, executor, or Shell target, update `CMakeLists.txt`, validation, and documentation together.
3. Build Release, run the ctest unit tests, and run `python tools/validate_menu_json.py`.
4. Re-register with the scripts in the output directory.
5. Test file, directory, directory-background, drive, and mixed-selection contexts.

## Validation checklist

### Automated

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
python scripts/repo_rules.py check --config repo-rules.json
python -m unittest discover -s scripts/tests -v
git -c core.whitespace=cr-at-eol diff --check
```

CI runs the Release build, unit tests (ctest), and menu validation on push/PR; the hygiene workflow runs the repository rules checker and its tests separately. Current CTest coverage includes the COM exception boundary, strict config parsing, placeholders including long `%D`, context building, registration config parsing, and launch argument preservation. Hand-written C/C++ follows `.clang-format`, `.editorconfig`, and `AGENTS.md`.

### Manual

| Scenario | Expected |
|----------|----------|
| File context | File-target menu such as `Demo -F` |
| Directory context | `Demo -D` |
| Directory background | `Demo -DB` |
| Drive root | `Demo -DR` |
| Mixed file/directory selection | `Demo -FS` |
| MessageBox | `%N` shows the name without the parent path |
| Very long paths (>260 chars) | Menu renders and `%1`/`%*`/`%N`/`%D` expand in full; requires system long-path policy and a long-path-aware Explorer |
| After registration | Menu appears without an Explorer restart (`SHChangeNotify`) |
| Invalid `registration.json` | Registration fails; an existing installation is left intact |
| 32-bit DLL | Scripts use `SysWOW64\regsvr32.exe` |
| Read-only/temp contexts | Configured gates hide or disable items |
| High DPI | SVG icons remain clear and layout stays stable |
| After unregister | This project's targets and CLSID are removed |

## Platform and safety boundaries

- DLL bitness must match Explorer; the registration scripts select `System32`/`SysWOW64` from the PE machine type.
- Registration and unregistration require elevation.
- Registry operations delete only this project's CLSID keys; an invalid configuration never deletes an existing CLSID first.
- Gates must not perform disk scans or network I/O in the menu request path.
- The DLL links the MSVC CRT statically, so no VC++ runtime is required on the target machine.
- Validate Windows 7 support in a VM before release; the VS 2026 toolchain/static CRT Win7 compatibility must be confirmed separately.
