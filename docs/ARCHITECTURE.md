# 架构说明

**简体中文** | [English](ARCHITECTURE.en.md)

**设计目标：** Windows 7 SP1 及之后（x86 / x64 Explorer）

本文档为架构基线。Win7 支持是**设计与验证目标**；发布前须在 Win7 SP1 x64 虚拟机完成手动回归（见下文验证清单）。

## 目标

- 可供 Fork 的高级 Shell 右键菜单 Demo
- 仅以 `IContextMenu` 为必需集成路径（Win7 至 Win11）
- 可插拔 Gate 实现分层可见性判断
- JSON 配置菜单规则与 Gate/Executor 链路，C++ 实现复杂逻辑
- 可插拔动作执行器链（`ExecutorRegistry`）
- SVG 光栅化图标 + DPI 感知回退（`IconProviderRegistry`）

## 非目标

- 以 Win11 精简菜单 / `IExplorerCommand` 为主路径（见路线图附录）
- 仅用 JSON 表达全部业务规则
- 在 `Initialize` / `QueryContextMenu` 中做重 IO

## 调用流程

```
Initialize
  ContextBuilder（路径、属性、progId — 只解析一次）
    → Gate 1（全局 extensionGates 链）
    → Gate 2（逐项 itemGates 链）→ candidateItems
      （可选：逐项 extensionGates 附加检查）
    → 无候选项 → E_FAIL
    → 否则     → S_OK

QueryContextMenu
  Gate 3（逐项 presentationGates 链）→ insertedItems
    → Hidden   ：不插入菜单
    → Disabled ：插入但 MFS_DISABLED
    → Enabled  ：正常插入
  仅为 insertedItems 分配 cmd ID
  加载图标（SVG / BMP / 默认资源，按 DPI 缩放）

InvokeCommand
  按 offset/verb 映射 insertedItems
  → Disabled 项不执行
  → executors 链（按 JSON 或默认顺序）
```

## Gate 语义

| Gate | 时机 | 通过 | 失败 |
|------|------|------|------|
| Gate 1 | `Initialize` | 扩展可参与 | `E_FAIL`，不使用本扩展 |
| Gate 2 | `Initialize` | 项进入 `candidateItems` | 省略该项 |
| Gate 3 | `QueryContextMenu` | `Enabled` / `Disabled` / `Hidden` | `Hidden` 不插入 |

**链式求值：**

- Extension / Item：AND，任一失败则拒绝
- Presentation：`Hidden` > `Disabled` > `Enabled`
- 名称 `custom:MyGate` 会剥离 `custom:` 前缀；`demo:*` 等保留完整注册名

**重要：** Gate 1 可通过，但 Gate 2 无候选项时，`Initialize` 仍返回 `E_FAIL`。

Gate 1 必须极快（不扫盘、不访问网络）。JSON 配置在进程内首次加载后缓存。

## MenuContext

每次右键在 `ContextBuilder` 中构建一次。Gate 只读 `MenuContext`，不得对已解析项再次调用 `GetFileAttributes`。

建议检测维度（文档/Fork 示例，Demo 不必全实现）：

- 扩展名、ProgID、文件/文件夹名模式
- 路径前缀、盘符、UNC
- 选中数量、文件+文件夹混合选中
- 属性：目录、只读、隐藏、重解析点
- 文件夹背景（无选中、`folderPath` 有值）

内置 Demo Gate：

| 名称 | 类型 | 行为 |
|------|------|------|
| `demo:hideTemp` | Item | 路径含 `\temp\` 时隐藏该项 |
| `demo:readOnlyDisable` | Presentation | 只读文件时 Disabled |

## 命令 ID 映射

- `idCmdFirst + offset` 对应 `insertedItems[offset]`
- 分隔线不占用命令 ID
- `GetCommandString` 与 `InvokeCommand` 使用同一 `insertedItems` 列表
- 也支持通过 verb 查找

## 配置模型

`config/menu.json` 支持根级与逐项覆盖的链路字段：

```json
{
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter", "demo:hideTemp"],
  "presentationGates": ["presentationPass", "demo:readOnlyDisable"],
  "executors": ["demo:actionLog", "messageBox", "launch"],
  "menuItems": [
    {
      "id": "example",
      "label": "&Example",
      "verb": "example",
      "icon": "icons/example.svg",
      "itemGates": ["jsonFilter"],
      "executors": ["messageBox"]
    }
  ]
}
```

| 字段 | 作用 |
|------|------|
| `extensionGates` | Gate 1 链；逐项非空时对该项附加检查 |
| `itemGates` | Gate 2 链；`gates` 为其别名 |
| `presentationGates` | Gate 3 链 |
| `executors` | 动作执行器链 |
| `icon` | 相对 DLL 目录的图标路径（SVG 或 BMP） |

空数组时回退默认链：`extensionPass` → `jsonFilter` → `presentationPass` → `messageBox` + `launch`。

C++ 在 `GateRegistry` / `ExecutorRegistry` 中注册自定义 Gate 与 Executor。

## 平台说明（Win7 SP1+）

| 主题 | 做法 |
|------|------|
| Shell API | `IShellExtInit`、`IContextMenu` |
| 构建 | `_WIN32_WINNT=0x0601`、`WINVER=0x0601` |
| 较新 API | `OsVersion` 检测后动态加载 |
| DPI/图标 | Win7 系统 DPI；Win8.1+ per-monitor（`DpiProvider`） |
| 注册 | `*` 处理器 + 运行时 Gate；DLL 位数须与 Explorer 一致 |
| 工具链 | 声称支持 Win7 前须在 Win7 VM 验证 VS 产物 |

## 源码布局

```
src/
  extension/     COM Shell 入口（薄层）
  context/       MenuContext、ContextBuilder
  gates/         Gate 接口、注册表、JsonFilterGate、Demo Gate
  actions/       IActionExecutor、ExecutorRegistry、MessageBox/Launch
  platform/      OsVersion、DpiProvider
  icons/         SVG/BMP 图标提供器、DPI 光栅化
  menu/          MenuProvider、MenuConfig、MenuGateChains
  registry/      COM 注册辅助
  resources/     RC、DEF、位图
config/
  menu.json
  icons/         SVG 等资源
tools/
  validate_menu_json.py
  register.ps1
```

## 路线图

| 阶段 | 范围 | 状态 |
|------|------|------|
| **P0** | ContextBuilder、Gate 1/2/3、JsonFilterGate、candidate/inserted 分离 | ✅ 完成 |
| **P1** | `IActionExecutor` 责任链 | ✅ 完成 |
| **P2** | `platform/DpiProvider`、`OsVersion` | ✅ 完成 |
| **P3** | SVG 图标提供器、逐项 `icon` | ✅ 完成 |
| **P4** | JSON Gate/Executor 链、Demo 扩展 | ✅ 完成 |
| **质量** | CI 构建、`menu.json` 校验、Win7 VM 手动回归 | 🔄 进行中 |
| **附录** | `IExplorerCommand` 试验（非 Win7 基线） | 未开始 |

## 验证清单

### 自动化（CI / 本地）

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
```

GitHub Actions 工作流 `.github/workflows/ci.yml` 在 push/PR 时执行 Release 构建与 JSON 校验。

### 手动（发布 / Fork 前）

| 场景 | 预期 |
|------|------|
| 右键 `.cpp` 文件 | 显示配置项；DebugView 可见 `[ShellExt]` 日志 |
| `%TEMP%` 下 `.cpp` | `demo:hideTemp` 过滤菜单项 |
| 只读 `.cpp` | 菜单项灰色（`demo:readOnlyDisable`） |
| 文件夹背景 | `foldersOnly` 项可见 |
| 多选 / 扩展名过滤 | `jsonFilter` 规则生效 |
| 点击菜单 | `demo:actionLog` 日志 + messageBox/launch 执行 |
| Win7 SP1 x64 VM | DLL 加载、菜单显示、动作执行无崩溃 |
| Win10 / Win11 x64 | 同上；高 DPI 下图标清晰 |

## Fork 指南

1. 修改 `dllmain.cpp` / `include/shell_ext/common.h` 中的 CLSID 与显示名称
2. 编辑 `config/menu.json`（含 Gate/Executor 链与图标路径）
3. 新增 `src/gates/MyGate.cpp` 并在 `GateRegistry::RegisterBuiltInGates` 注册
4. 新增 `src/actions/MyExecutor.cpp` 并在 `ExecutorRegistry::RegisterBuiltInExecutors` 注册
5. 运行自动化验证；在 Win7 SP1 x64 虚拟机完成上表手动项
