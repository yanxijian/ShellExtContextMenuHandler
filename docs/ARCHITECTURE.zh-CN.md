# 架构说明

[English](ARCHITECTURE.md) | **简体中文**

**设计目标：** Windows 7 SP1 及之后（x86 / x64 Explorer）

本文档为纠错后的架构基线。Win7 支持是**设计与验证目标**，不代表所有构建配置已在 Win7 实机上验证。

## 目标

- 可供 Fork 的高级 Shell 右键菜单 Demo
- 仅以 `IContextMenu` 为必需集成路径（Win7 至 Win11）
- 可插拔 Gate 实现分层可见性判断
- JSON 处理简单规则，C++ 处理复杂逻辑
- 可插拔动作执行器（P1+）
- SVG 光栅化图标 + DPI 回退（P3+）

## 非目标

- 以 Win11 精简菜单 / `IExplorerCommand` 为主路径
- 仅用 JSON 表达全部业务规则
- 在 `Initialize` / `QueryContextMenu` 中做重 IO

## 调用流程

```
Initialize
  ContextBuilder（路径、属性、progId — 只解析一次）
    → Gate 1（ExtensionGate）
    → Gate 2（逐项 ItemGate）→ candidateItems
    → 无候选项 → E_FAIL
    → 否则     → S_OK

QueryContextMenu
  Gate 3（逐项 PresentationGate）→ insertedItems
    → Hidden   ：不插入菜单
    → Disabled ：插入但 MFS_DISABLED
    → Enabled  ：正常插入
  仅为 insertedItems 分配 cmd ID
  加载图标（P3+）

InvokeCommand
  按 offset/verb 映射 insertedItems
  → Disabled 项不执行
  → 动作执行器链（P1+）
```

## Gate 语义

| Gate | 时机 | 通过 | 失败 |
|------|------|------|------|
| Gate 1 | `Initialize` | 扩展可参与 | `E_FAIL`，不使用本扩展 |
| Gate 2 | `Initialize` | 项进入 `candidateItems` | 省略该项 |
| Gate 3 | `QueryContextMenu` | `Enabled` / `Disabled` / `Hidden` | `Hidden` 不插入 |

**重要：** Gate 1 可通过，但 Gate 2 无候选项时，`Initialize` 仍返回 `E_FAIL`（与当前行为一致）。

Gate 1 必须极快（不扫盘、不访问网络）。JSON 配置在进程内首次加载后缓存。

## MenuContext

每次右键在 `ContextBuilder` 中构建一次。Gate 只读 `MenuContext`，不得对已解析项再次调用 `GetFileAttributes`。

建议检测维度（文档/Fork 示例，Demo 不必全实现）：

- 扩展名、ProgID、文件/文件夹名模式
- 路径前缀、盘符、UNC
- 选中数量、文件+文件夹混合选中
- 属性：目录、只读、隐藏、重解析点
- 文件夹背景（无选中、`folderPath` 有值）

## 命令 ID 映射

- `idCmdFirst + offset` 对应 `insertedItems[offset]`
- 分隔线不占用命令 ID
- `GetCommandString` 与 `InvokeCommand` 使用同一 `insertedItems` 列表
- 也支持通过 verb 查找

## 配置模型

`config/menu.json`：声明式文案、过滤、动作参数。

C++：`GateRegistry` 注册 Gate（P1+ 为 `ExecutorRegistry` 注册动作）。

未来 JSON 字段示例：

```json
"gates": ["jsonFilter", "custom:MyGate"]
```

P0 在代码中注册默认 Gate：`extensionPass`、`jsonFilter`、`presentationPass`。

## 平台说明（Win7 SP1+）

| 主题 | 做法 |
|------|------|
| Shell API | `IShellExtInit`、`IContextMenu` |
| 构建 | `_WIN32_WINNT=0x0601`、`WINVER=0x0601` |
| 较新 API | 检测系统版本后动态加载 |
| DPI/图标 | Win7 用系统 DPI；Win8.1+ 支持 per-monitor（P2/P3） |
| 注册 | `*` 处理器 + 运行时 Gate；DLL 位数与 Explorer 一致 |
| 工具链 | 声称支持 Win7 前须在 Win7 VM 验证 VS 2026 产物 |

## 源码布局

```
src/
  extension/     COM 薄壳
  context/       MenuContext、ContextBuilder
  gates/         Gate 接口、注册表、JsonFilterGate
  menu/          MenuProvider、配置、动作
  registry/      注册表辅助
  resources/     RC、DEF、位图
```

## 路线图

| 阶段 | 内容 |
|------|------|
| **P0** | ContextBuilder、Gate 1/2/3 骨架、JsonFilterGate、candidate/inserted 分离、Win7 CMake 宏 |
| **P1** | `IActionExecutor` 责任链 |
| **P2** | `platform/DpiProvider`、`OsVersion` |
| **P3** | SVG 图标提供器 |
| **P4** | 示例 Gate/Executor、JSON `gates` 数组 |
| **附录** | `IExplorerCommand` 试验（非 Win7 基线） |

## Fork 指南

1. 修改 `dllmain.cpp` / `include/shell_ext/common.h` 中的 CLSID 与名称
2. 多数菜单改动只需编辑 `config/menu.json`
3. 新增 `src/gates/MyGate.cpp` 并在 `GateRegistry` 注册
4. 新增 `src/actions/MyExecutor.cpp`（P1+）并注册
5. 发布前在 Win7 SP1 x64 虚拟机测试
