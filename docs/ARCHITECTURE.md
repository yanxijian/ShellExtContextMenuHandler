# Architecture

**English** | [简体中文](ARCHITECTURE.zh-CN.md)

**Design target:** Windows 7 SP1 and later (x86 / x64 Explorer)

This document is the corrected architecture baseline. Win7 support is a **design and validation goal**, not a claim that every build configuration has been verified on Win7 hardware.

## Goals

- Fork-friendly advanced demo for Shell context menu extensions
- `IContextMenu` as the only required integration path (Win7 through Win11)
- Pluggable gates for layered visibility checks
- JSON for simple menu rules, C++ for complex logic
- Pluggable action executors (P1+)
- SVG rasterized icons with DPI fallbacks (P3+)

## Non-goals

- Win11 compact menu / `IExplorerCommand` as the primary path
- Expressing all business rules in JSON alone
- Heavy I/O inside `Initialize` or `QueryContextMenu`

## Call flow

```
Initialize
  ContextBuilder  (paths, attributes, progId — parsed once)
    -> Gate 1 (ExtensionGate)
    -> Gate 2 (ItemGate per menu item) -> candidateItems
    -> no candidates => E_FAIL
    -> otherwise     => S_OK

QueryContextMenu
  Gate 3 (PresentationGate per candidate) -> insertedItems
    -> Hidden   : not inserted
    -> Disabled : inserted with MFS_DISABLED
    -> Enabled  : inserted normally
  Assign cmd IDs only to insertedItems (non-Hidden)
  Load icons (P3+)

InvokeCommand
  Map offset/verb to insertedItems
  -> skip execution when Disabled
  -> Action executor chain (P1+)
```

## Gate semantics

| Gate | When | Pass | Fail |
|------|------|------|------|
| Gate 1 | `Initialize` | Extension may participate | `E_FAIL`, handler not used |
| Gate 2 | `Initialize` | Item enters `candidateItems` | Item omitted |
| Gate 3 | `QueryContextMenu` | `Enabled` / `Disabled` / `Hidden` | `Hidden` omits item |

**Important:** Gate 1 may pass while Gate 2 yields zero items. In that case `Initialize` still returns `E_FAIL` (same as today).

Gate 1 should be very fast (no disk scans, no network). JSON config is cached at process scope after first load.

## MenuContext

Built once per right-click in `ContextBuilder`. Gates read `MenuContext` only; they must not call `GetFileAttributes` again for items already parsed.

Suggested detection dimensions (documentation / fork examples, not all implemented in the demo):

- Extension, ProgID, file/folder name patterns
- Path prefix, drive, UNC
- Selection count, mixed file+folder selection
- Attributes: directory, readonly, hidden, reparse point
- Folder background (no selection, `folderPath` set)

## Command ID mapping

- `idCmdFirst + offset` maps to `insertedItems[offset]`
- Separators do not consume command IDs
- `GetCommandString` and `InvokeCommand` use the same `insertedItems` list
- Verb lookup is also supported via `GetCommandString` / `InvokeCommand`

## Configuration model

`config/menu.json` — declarative labels, filters, actions.

C++ gate / executor registration — `GateRegistry` (and `ExecutorRegistry` in P1).

Future JSON field:

```json
"gates": ["jsonFilter", "custom:MyGate"]
```

P0 registers default gates in code: `extensionPass`, `jsonFilter`, `presentationPass`.

## Platform notes (Win7 SP1+)

| Topic | Approach |
|-------|----------|
| Shell API | `IShellExtInit`, `IContextMenu` |
| Build | `_WIN32_WINNT=0x0601`, `WINVER=0x0601` |
| Newer APIs | Dynamic load after OS version check |
| DPI / icons | System DPI on Win7; per-monitor on Win8.1+ (P2/P3) |
| Registration | `*` handler + runtime gates; match DLL bitness to Explorer |
| Toolchain | VS 2026 output must be validated on Win7 VM before claiming support |

## Source layout

```
src/
  extension/     COM shell entry (thin)
  context/       MenuContext, ContextBuilder
  gates/         Gate interfaces, registry, JsonFilterGate
  menu/          MenuProvider, MenuItem, config, actions
  registry/      COM registration helpers
  resources/     RC, DEF, bitmaps
```

## Roadmap

| Phase | Scope |
|-------|--------|
| **P0** | ContextBuilder, Gate 1/2/3 skeleton, JsonFilterGate, candidate/inserted split, Win7 CMake macros |
| **P1** | `IActionExecutor` chain |
| **P2** | `platform/DpiProvider`, `OsVersion` |
| **P3** | SVG icon provider |
| **P4** | Sample custom gates/executors, JSON `gates` array |
| **Appendix** | `IExplorerCommand` experiments (not Win7 baseline) |

## Fork guide

1. Change CLSID and names in `dllmain.cpp` / `include/shell_ext/common.h`
2. Edit `config/menu.json` for most menu changes
3. Add `src/gates/MyGate.cpp` and register in `GateRegistry`
4. Add `src/actions/MyExecutor.cpp` (P1+) and register
5. Test on Win7 SP1 x64 VM before release