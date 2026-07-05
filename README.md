# ShellExtContextMenuHandler

基于微软官方 C++ Shell 扩展示例改造的**可配置右键菜单模块**。通过 `menu.json` 定义菜单项、过滤规则和动作，无需修改核心 COM 代码即可定制 Explorer 右键菜单。

原始 Demo 来源：[CppShellExtContextMenuHandler](https://code.msdn.microsoft.com/windowsapps/CppShellExtContextMenuHandl-410a709a)

## 特性

- 注册到所有文件类型（`*`），在运行时按规则决定是否显示菜单
- 通过 `config/menu.json` 配置多个菜单项
- 支持 `messageBox` 与 `launch`（启动外部进程）两种动作
- 支持扩展名、选中数量、文件/文件夹等过滤条件
- 支持文件夹背景右键（无选中项时读取当前目录）
- 内置调试日志（`OutputDebugString`，可用 DebugView 查看）
- **CMake** 构建，可生成 Visual Studio 解决方案

## 项目结构

```
├── CMakeLists.txt                 # 构建入口（单一事实来源）
├── config/
│   └── menu.json                  # 菜单配置
├── include/
│   └── shell_ext/
│       └── common.h               # COM 注册用名称常量
├── src/
│   ├── extension/                 # Shell COM 入口
│   │   ├── dllmain.cpp
│   │   ├── ClassFactory.*
│   │   └── FileContextMenuExt.*
│   ├── menu/                      # 菜单引擎
│   │   ├── MenuProvider.*
│   │   ├── MenuConfig.*
│   │   ├── Filter.*
│   │   ├── MenuActionHandler.*
│   │   ├── PathHelpers.*
│   │   └── ShellLog.*
│   ├── registry/                  # 注册表辅助
│   │   └── Reg.*
│   └── resources/                 # 资源与模块导出
│       ├── ShellExtContextMenuHandler.rc
│       ├── GlobalExportFunctions.def
│       ├── resource.h
│       └── OK.bmp
├── tools/
│   ├── generate-vs.ps1            # 由 CMake 生成 VS 工程
│   └── register.ps1               # 注册/卸载脚本
└── build/                         # 生成目录（git 忽略）
    ├── ShellExtContextMenuHandler.slnx   # VS 2026 解决方案
    ├── CppShellExtContextMenuHandler.vcxproj
    └── bin/Release/*.dll
```

## 快速开始

### 1. 生成 Visual Studio 工程

```powershell
.\tools\generate-vs.ps1
```

这会在 `build/` 目录生成 `ShellExtContextMenuHandler.slnx`（VS 2026 格式）及 `CppShellExtContextMenuHandler.vcxproj`。可直接用 Visual Studio 打开该解决方案。

也可手动执行：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

### 2. 构建

**命令行：**

```powershell
cmake --build build --config Release
```

**Visual Studio：** 打开 `build/ShellExtContextMenuHandler.slnx`，选择 `Release | x64` 后编译。

构建产物位于 `build/bin/Release/`：

- `CppShellExtContextMenuHandler.dll`
- `menu.json`（自动复制）

### 3. 注册

以管理员身份运行：

```powershell
.\tools\register.ps1 -Action register
```

卸载：

```powershell
.\tools\register.ps1 -Action unregister
```

> `register.ps1` 会自动在 `build/bin/Release/` 等常见输出路径中查找 DLL，并将 `config/menu.json` 复制到 DLL 同目录。

### 4. 验证

在资源管理器中右键 `.cpp` 文件，应看到配置中的菜单项。

## 定制菜单

编辑 `config/menu.json`。每个菜单项支持以下字段：

| 字段 | 说明 |
|------|------|
| `id` | 唯一标识 |
| `label` | 菜单显示文字（`&` 标记快捷键字母） |
| `verb` | 命令动词，用于程序化调用 |
| `helpText` | 状态栏帮助文字 |
| `canonicalName` | 规范动词名 |
| `separatorAfter` | 是否在此项后插入分隔线 |
| `extensions` | 允许的扩展名列表，如 `[".cpp", ".h"]`，空数组表示不限制 |
| `excludeExtensions` | 排除的扩展名 |
| `minSelection` | 最少选中数量，默认 `1` |
| `maxSelection` | 最多选中数量，`0` 表示不限制 |
| `filesOnly` | 仅对文件显示，默认 `true` |
| `foldersOnly` | 仅对文件夹/目录背景显示 |
| `actionType` | `messageBox` 或 `launch` |
| `actionTitle` | 弹窗标题（`messageBox`） |
| `actionTemplate` | 弹窗内容模板（`messageBox`） |
| `actionCommand` | 命令行（`launch`） |
| `actionShowWindow` | 启动进程时是否显示窗口，默认 `false` |

### 占位符

| 占位符 | 含义 |
|--------|------|
| `%1` | 第一个选中路径；无选中时为当前文件夹 |
| `%*` | 所有选中路径（带引号、空格分隔） |
| `%D` | 所在目录路径 |

若 `menu.json` 缺失或解析失败，扩展会回退到 `include/shell_ext/common.h` 中定义的内置菜单项。

## 修改 COM 身份信息

发布自己的扩展时，请修改：

1. `src/extension/dllmain.cpp` 中的 `CLSID_FileContextMenuExt`
2. `include/shell_ext/common.h` 中的 `L_Friendly_Class_Name` 和 `L_Friendly_Menu_Name`

## 调试

扩展通过 `OutputDebugString` 输出日志，前缀为 `[ShellExt]`。推荐用 [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) 查看。

## 注意事项

- **位数匹配**：64 位 Windows 的 Explorer 只能加载 x64 DLL
- **管理员权限**：注册 COM 组件需要提升权限
- **重新生成工程**：修改 `CMakeLists.txt` 后重新运行 `generate-vs.ps1`
- **Explorer 缓存**：修改 DLL 后建议注销再注册，必要时重启 Explorer

## 架构说明

```
Explorer 右键
    → IShellExtInit::Initialize        （解析选中项，过滤菜单）
    → IContextMenu::QueryContextMenu （插入可见项）
    → IContextMenu::InvokeCommand    （按 verb/offset 分发动作）
```

核心逻辑与 Shell COM 胶水代码分离：定制时只需改 `config/menu.json`（及可选的 `common.h` 回退项）。

## 许可证

基于微软示例代码（[Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)）修改。Microsoft 原始版权说明保留在源文件头部。
