# Architecture

**English** | [简体中文](ARCHITECTURE.md)

**Design target:** Windows 7 SP1 and later (x86 / x64 Explorer)

This document is the architecture baseline. Win7 support is a **design and validation goal**; complete the manual Win7 SP1 x64 checklist below before claiming release support.

## Goals

- Fork-friendly advanced demo for Shell context menu extensions
- `IContextMenu` as the only required integration path (Win7 through Win11)
- Pluggable gates for layered visibility checks
- JSON for menu rules and gate/executor chains; C++ for complex logic
- Pluggable action executor chain (`ExecutorRegistry`)
- SVG rasterized icons with DPI-aware fallbacks (`IconProviderRegistry`)

## Non-goals

- Win11 compact menu / `IExplorerCommand` as the primary path (see roadmap appendix)
- Expressing all business rules in JSON alone
- Heavy I/O inside `Initialize` or `QueryContextMenu`

## Call flow

```
Initialize
  ContextBuilder  (paths, attributes, progId — parsed once)
    -> Gate 1 (global extensionGates chain)
    -> Gate 2 (per-item itemGates chain) -> candidateItems
      (optional: per-item extensionGates extra check)
    -> no candidates => E_FAIL
    -> otherwise     => S_OK

QueryContextMenu
  Gate 3 (per-item presentationGates chain) -> insertedItems
    -> Hidden   : not inserted
    -> Disabled : inserted with MFS_DISABLED
    -> Enabled  : inserted normally
  Assign cmd IDs only to insertedItems
  Load icons (SVG / BMP / default resource, DPI-scaled)

InvokeCommand
  Map offset/verb to insertedItems
  -> skip execution when Disabled
  -> executors chain (JSON order or defaults)
```

## Gate semantics

| Gate | When | Pass | Fail |
|------|------|------|------|
| Gate 1 | `Initialize` | Extension may participate | `E_FAIL`, handler not used |
| Gate 2 | `Initialize` | Item enters `candidateItems` | Item omitted |
| Gate 3 | `QueryContextMenu` | `Enabled` / `Disabled` / `Hidden` | `Hidden` omits item |

**Chain evaluation:**

- Extension / Item: AND — any failure rejects
- Presentation: `Hidden` > `Disabled` > `Enabled`
- Name `custom:MyGate` strips the `custom:` prefix; `demo:*` keeps the full registered name

Gate 1 should be very fast. JSON config is cached after first load.

## MenuContext

Built once per right-click in `ContextBuilder`. Gates read `MenuContext` only.

Built-in demo gates:

| Name | Type | Behavior |
|------|------|----------|
| `demo:hideTemp` | Item | Hides item when path contains `\temp\` |
| `demo:readOnlyDisable` | Presentation | Disabled on readonly files |

## Configuration model

`config/menu.json` supports root-level and per-item chain overrides:

```json
{
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter", "demo:hideTemp"],
  "presentationGates": ["presentationPass", "demo:readOnlyDisable"],
  "executors": ["demo:actionLog", "messageBox", "launch"],
  "menuItems": [ { "id": "example", "verb": "example", "icon": "icons/example.svg" } ]
}
```

| Field | Role |
|-------|------|
| `extensionGates` | Gate 1 chain; per-item array adds an extra check for that item |
| `itemGates` | Gate 2 chain; `gates` is an alias |
| `presentationGates` | Gate 3 chain |
| `executors` | Action executor chain |
| `icon` | Icon path relative to the DLL directory |

Empty arrays fall back to defaults: `extensionPass` → `jsonFilter` → `presentationPass` → `messageBox` + `launch`.

Register custom gates/executors in `GateRegistry` / `ExecutorRegistry`.

## Platform notes (Win7 SP1+)

| Topic | Approach |
|-------|----------|
| Shell API | `IShellExtInit`, `IContextMenu` |
| Build | `_WIN32_WINNT=0x0601`, `WINVER=0x0601` |
| Newer APIs | Dynamic load after `OsVersion` check |
| DPI / icons | System DPI on Win7; per-monitor on Win8.1+ |
| Registration | `*` handler + runtime gates; match DLL bitness |
| Toolchain | Validate VS output on Win7 VM before claiming support |

## Source layout

```
src/
  extension/     COM shell entry (thin)
  context/       MenuContext, ContextBuilder
  gates/         Gate interfaces, registry, JsonFilterGate, demo gates
  actions/       IActionExecutor, ExecutorRegistry, MessageBox/Launch
  platform/      OsVersion, DpiProvider
  icons/         SVG/BMP providers, DPI rasterization
  menu/          MenuProvider, MenuConfig, MenuGateChains
  registry/      COM registration helpers
  resources/     RC, DEF, bitmaps
config/
  menu.json
  icons/
tools/
  validate_menu_json.py
  register.ps1
```

## Roadmap

| Phase | Scope | Status |
|-------|--------|--------|
| **P0** | ContextBuilder, Gate 1/2/3, JsonFilterGate, candidate/inserted split | Done |
| **P1** | `IActionExecutor` chain | Done |
| **P2** | `platform/DpiProvider`, `OsVersion` | Done |
| **P3** | SVG icon provider, per-item `icon` | Done |
| **P4** | JSON gate/executor chains, demo extensions | Done |
| **Quality** | CI build, menu.json validation, Win7 VM manual pass | In progress |
| **Appendix** | `IExplorerCommand` experiments (not Win7 baseline) | Not started |

## Validation checklist

### Automated (CI / local)

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
```

Workflow `.github/workflows/ci.yml` runs Release build and JSON validation on push/PR.

### Manual (before release / fork)

| Scenario | Expected |
|----------|----------|
| Right-click `.cpp` | Menu items appear; `[ShellExt]` logs in DebugView |
| `.cpp` under `%TEMP%` | Items filtered by `demo:hideTemp` |
| Readonly `.cpp` | Items shown disabled |
| Folder background | `foldersOnly` items visible |
| Multi-select / extensions | `jsonFilter` rules apply |
| Invoke menu item | `demo:actionLog` log + messageBox/launch runs |
| Win7 SP1 x64 VM | No crash; menu and actions work |
| Win10 / Win11 x64 | Same; crisp icons at high DPI |

## Fork guide

1. Change CLSID and names in `dllmain.cpp` / `include/shell_ext/common.h`
2. Edit `config/menu.json` (chains, icons, items)
3. Add `src/gates/MyGate.cpp` and register in `GateRegistry`
4. Add `src/actions/MyExecutor.cpp` and register in `ExecutorRegistry`
5. Run automated validation; complete the manual checklist on Win7 SP1 x64 VM
