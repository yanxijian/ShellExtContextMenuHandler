# ShellExtContextMenuHandler

[English](README.md) | **简体中文**

基于微软官方 C++ Shell 扩展示例改造的**可配置右键菜单模块**。在 `menu.json` 中定义菜单项、过滤规则与动作，无需改动核心 COM 代码。

来源：[CppShellExtContextMenuHandler](https://code.msdn.microsoft.com/windowsapps/CppShellExtContextMenuHandl-410a709a)

## 功能

- 注册为所有文件类型（`*`），由运行时规则控制可见性
- 通过 `config/menu.json` 配置多个菜单项
- 支持 `messageBox` 与 `launch`（启动进程）动作
- 过滤条件：扩展名、选中数量、文件/文件夹
- 文件夹背景右键（无选中时使用当前目录）
- 通过 `OutputDebugString` 输出调试日志（[DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview)）
- **CMake** 构建，可生成 Visual Studio 工程
- **分层 Gate 架构**（Extension / Item / Presentation）— 见 [docs/ARCHITECTURE.zh-CN.md](docs/ARCHITECTURE.zh-CN.md)

## 项目结构

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
│   ├── extension/     # Shell COM 入口
│   ├── context/       # MenuContext、ContextBuilder
│   ├── gates/         # Gate 接口、JsonFilterGate
│   ├── menu/          # MenuProvider、配置、动作
│   ├── registry/
│   └── resources/
├── tools/
│   ├── generate-vs.ps1
│   └── register.ps1
└── build/             # 生成目录（gitignore）
```

## 快速开始

### 1. 生成 Visual Studio 工程

```powershell
.\tools\generate-vs.ps1
```

将生成 `build/ShellExtContextMenuHandler.slnx` 与 `CppShellExtContextMenuHandler.vcxproj`。

或手动执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

### 2. 编译

```powershell
cmake --build build --config Release
```

或在 Visual Studio 中打开 `build/ShellExtContextMenuHandler.slnx`（`Release | x64`）。

输出：`build/bin/Release/CppShellExtContextMenuHandler.dll` 与 `menu.json`。

### 3. 注册

以管理员身份运行：

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister   # 卸载
```

`register.ps1` 会在 `build/bin/Release/` 下查找 DLL，并将 `config/menu.json` 复制到 DLL 同目录。

### 4. 验证

在资源管理器中右键 `.cpp` 文件，应出现配置的菜单项。

## 自定义菜单

编辑 `config/menu.json`：

| 字段 | 说明 |
|------|------|
| `id` | 唯一标识 |
| `label` | 菜单文字（`&` 为快捷键） |
| `verb` | 命令动词，供程序化调用 |
| `helpText` | 状态栏帮助文字 |
| `canonicalName` | 规范动词名 |
| `separatorAfter` | 此项后插入分隔线 |
| `extensions` | 允许的扩展名，如 `[".cpp"]`；`[]` 表示不限制 |
| `excludeExtensions` | 排除的扩展名 |
| `minSelection` | 最少选中数量（默认 `1`） |
| `maxSelection` | 最多选中数量；`0` 表示不限 |
| `filesOnly` | 仅文件（默认 `true`） |
| `foldersOnly` | 仅文件夹 / 文件夹背景 |
| `actionType` | `messageBox` 或 `launch` |
| `actionTitle` | 对话框标题（`messageBox`） |
| `actionTemplate` | 对话框正文模板（`messageBox`） |
| `actionCommand` | 命令行（`launch`） |
| `actionShowWindow` | 启动时显示窗口（默认 `false`） |

### 占位符

| 符号 | 含义 |
|------|------|
| `%1` | 第一个选中路径；无选中时为当前文件夹 |
| `%*` | 所有选中路径（带引号、空格分隔） |
| `%D` | 父目录 |

若 `menu.json` 缺失或无效，扩展会回退到 `include/shell_ext/common.h` 中的内置项。

## COM 标识

发布自己的 Fork 时，请修改：

1. `src/extension/dllmain.cpp` 中的 `CLSID_FileContextMenuExt`
2. `include/shell_ext/common.h` 中的 `L_Friendly_Class_Name` 与 `L_Friendly_Menu_Name`

## 调试

日志前缀为 `[ShellExt]`，通过 `OutputDebugString` 输出。可使用 [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) 查看。

## 注意事项

- **位数：** 64 位 Explorer 需要 64 位 DLL
- **权限：** COM 注册需要提升权限
- **重新生成：** 修改 `CMakeLists.txt` 后请重新运行 `generate-vs.ps1`
- **Explorer 缓存：** 更新 DLL 后请注销/重新注册；必要时重启 Explorer

## 架构概览

```
资源管理器右键
  → IShellExtInit::Initialize
  → IContextMenu::QueryContextMenu
  → IContextMenu::InvokeCommand
```

Shell COM 层保持精简；多数定制通过 `config/menu.json` 及可选的 gate/executor 完成。详见 [docs/ARCHITECTURE.zh-CN.md](docs/ARCHITECTURE.zh-CN.md)。

## 许可证

派生自微软示例（[Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)）。源文件中保留微软原始版权头。
