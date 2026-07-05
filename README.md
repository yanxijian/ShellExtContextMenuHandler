# ShellExtContextMenuHandler

**简体中文** | [English](README.en.md)

基于微软官方 C++ Shell 扩展示例改造的**可配置右键菜单模块**。在 `menu.json` 中定义菜单项、过滤规则、Gate/Executor 链与图标，无需改动核心 COM 代码。

来源：[CppShellExtContextMenuHandler](https://code.msdn.microsoft.com/windowsapps/CppShellExtContextMenuHandl-410a709a)

## 功能

- 注册为所有文件类型（`*`），由运行时 Gate 控制可见性
- 通过 `config/menu.json` 配置多个菜单项与 Gate/Executor 链路
- 支持 `messageBox` 与 `launch`（启动进程）动作及自定义 Executor
- 过滤条件：扩展名、选中数量、文件/文件夹
- 文件夹背景右键（无选中时使用当前目录）
- SVG/BMP 图标，DPI 感知缩放（Win7 系统 DPI，Win8.1+ per-monitor）
- 内置 Demo Gate：`demo:hideTemp`、`demo:readOnlyDisable`；Demo Executor：`demo:actionLog`
- 通过 `OutputDebugString` 输出调试日志（[DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview)）
- **CMake** 构建；GitHub Actions CI 执行 Release 构建与 `menu.json` 校验
- **分层 Gate 架构**（Extension / Item / Presentation）— 见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

## 项目结构

```
├── CMakeLists.txt
├── .github/workflows/ci.yml
├── docs/
│   ├── ARCHITECTURE.md
│   └── ARCHITECTURE.en.md
├── README.md / README.en.md
├── config/
│   ├── menu.json
│   └── icons/              # SVG 等资源
├── include/shell_ext/common.h
├── src/
│   ├── extension/          # Shell COM 入口
│   ├── context/            # MenuContext、ContextBuilder
│   ├── gates/              # Gate 注册表、JsonFilterGate、Demo Gate
│   ├── actions/            # ExecutorRegistry、MessageBox/Launch
│   ├── platform/           # OsVersion、DpiProvider
│   ├── icons/              # SVG/BMP 图标提供器
│   ├── menu/               # MenuProvider、MenuConfig
│   ├── registry/
│   └── resources/
├── tools/
│   ├── generate-vs.ps1
│   ├── register.ps1
│   └── validate_menu_json.py
└── build/                  # 生成目录（gitignore）
```

## 快速开始

### 1. 生成 Visual Studio 工程

```powershell
.\tools\generate-vs.ps1
```

或：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

### 2. 编译

```powershell
cmake --build build --config Release
```

输出：`build/bin/Release/CppShellExtContextMenuHandler.dll`、`menu.json`、`icons/`。

### 3. 注册

以管理员身份运行：

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister
```

脚本会将 `config/menu.json` 与 `config/icons/` 复制到 DLL 同目录。

### 4. 验证

```powershell
python tools/validate_menu_json.py
```

在资源管理器中右键 `.cpp` 文件，应出现配置的菜单项。完整手动清单见 [docs/ARCHITECTURE.md#验证清单](docs/ARCHITECTURE.md#验证清单)。

## 自定义菜单

编辑 `config/menu.json`。

### 根级链路（可选，空则使用默认链）

| 字段 | 说明 |
|------|------|
| `extensionGates` | Gate 1 链，默认 `["extensionPass"]` |
| `itemGates` | Gate 2 链，默认 `["jsonFilter"]`；`gates` 为其别名 |
| `presentationGates` | Gate 3 链，默认 `["presentationPass"]` |
| `executors` | 动作链，默认 `["messageBox", "launch"]` |

### 菜单项字段

| 字段 | 说明 |
|------|------|
| `id` | 唯一标识 |
| `label` | 菜单文字（`&` 为快捷键） |
| `verb` | 命令动词 |
| `helpText` | 状态栏帮助文字 |
| `canonicalName` | 规范动词名 |
| `icon` | 相对 DLL 目录的图标路径，如 `icons/cpp.svg` |
| `separatorAfter` | 此项后插入分隔线 |
| `extensions` / `excludeExtensions` | 扩展名过滤 |
| `minSelection` / `maxSelection` | 选中数量（`maxSelection` 为 `0` 表示不限） |
| `filesOnly` / `foldersOnly` | 文件/文件夹限定 |
| `actionType` | `messageBox` 或 `launch` |
| `actionTitle` / `actionTemplate` | 对话框（`messageBox`） |
| `actionCommand` / `actionShowWindow` | 启动命令（`launch`） |
| `extensionGates` / `itemGates` / `presentationGates` / `executors` | 逐项覆盖根级链路 |

### 占位符

| 符号 | 含义 |
|------|------|
| `%1` | 第一个选中路径；无选中时为当前文件夹 |
| `%*` | 所有选中路径（带引号、空格分隔） |
| `%D` | 父目录 |

若 `menu.json` 缺失或无效，扩展回退到 `include/shell_ext/common.h` 中的内置项与默认链路。

## COM 标识

发布 Fork 时请修改：

1. `src/extension/dllmain.cpp` 中的 `CLSID_FileContextMenuExt`
2. `include/shell_ext/common.h` 中的友好名称

## 调试

日志前缀 `[ShellExt]`，通过 `OutputDebugString` 输出。使用 [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) 查看。

## 注意事项

- **位数：** 64 位 Explorer 需要 64 位 DLL
- **权限：** COM 注册需要提升权限
- **Explorer 缓存：** 更新 DLL 后请注销/重新注册；必要时重启 Explorer
- **Win7：** 发布前请在 Win7 SP1 x64 虚拟机完成架构文档中的手动验证清单

## 架构概览

```
资源管理器右键
  → IShellExtInit::Initialize（Gate 1/2）
  → IContextMenu::QueryContextMenu（Gate 3、图标）
  → IContextMenu::InvokeCommand（Executor 链）
```

详见 [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)。

## 许可证

派生自微软示例（[Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)）。源文件保留微软原始版权头。
