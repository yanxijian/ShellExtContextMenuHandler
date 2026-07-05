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

## 项目结构

```
├── config/menu.json          # 菜单配置（构建时复制到输出目录）
├── tools/register.ps1        # 注册/卸载脚本
├── FileContextMenuExt.*      # Shell COM 入口（薄封装）
├── MenuProvider.*            # 上下文构建与命令分发
├── MenuConfig.*              # JSON 配置加载
├── Filter.*                  # 过滤规则引擎
├── MenuActionHandler.*       # 动作执行（弹窗 / 启动进程）
├── PathHelpers.*             # 路径占位符展开
├── ShellLog.*                # 调试日志
├── Reg.*                     # 注册表辅助函数
└── common.h                  # COM 注册用名称常量
```

## 快速开始

### 1. 构建

使用 Visual Studio 打开 `CppShellExtContextMenuHandler.vcxproj`，选择 **x64** 配置（64 位 Windows 必须使用 x64 DLL），然后编译。

命令行构建示例：

```powershell
msbuild CppShellExtContextMenuHandler.vcxproj /p:Configuration=Release /p:Platform=x64
```

构建完成后，`menu.json` 会自动复制到输出目录（如 `x64\Release\`）。

### 2. 注册

**方式 A：PowerShell 脚本（推荐）**

以管理员身份运行：

```powershell
.\tools\register.ps1 -Action register
```

卸载：

```powershell
.\tools\register.ps1 -Action unregister
```

**方式 B：regsvr32**

```powershell
regsvr32.exe x64\Release\CppShellExtContextMenuHandler.dll
```

> 确保 `menu.json` 与 DLL 位于同一目录。使用 `register.ps1` 时会自动复制配置文件。

### 3. 验证

在资源管理器中右键 `.cpp` 文件，应看到配置中的菜单项。修改 `config/menu.json` 后重新构建并复制配置，然后重启 Explorer 或重新注册扩展。

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

在 `actionTemplate` 和 `actionCommand` 中可使用：

| 占位符 | 含义 |
|--------|------|
| `%1` | 第一个选中路径；无选中时为当前文件夹 |
| `%*` | 所有选中路径（带引号、空格分隔） |
| `%D` | 所在目录路径 |

### 配置示例

```json
{
  "menuItems": [
    {
      "id": "open-in-notepad",
      "label": "Open in &Notepad",
      "verb": "opennotepad",
      "extensions": [".txt", ".cpp"],
      "minSelection": 1,
      "maxSelection": 1,
      "actionType": "launch",
      "actionCommand": "notepad.exe %1",
      "actionShowWindow": true
    }
  ]
}
```

若 `menu.json` 缺失或解析失败，扩展会回退到 `common.h` 中定义的内置菜单项。

## 修改 COM 身份信息

发布自己的扩展时，请修改：

1. `dllmain.cpp` 中的 `CLSID_FileContextMenuExt`（使用 Visual Studio「创建 GUID」工具生成新值）
2. `common.h` 中的 `L_Friendly_Class_Name` 和 `L_Friendly_Menu_Name`

## 调试

扩展通过 `OutputDebugString` 输出日志，前缀为 `[ShellExt]`。推荐用 [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) 查看。

常见日志：

- 配置加载成功/失败
- 当前上下文无可见菜单项
- `CreateProcess` 失败

## 注意事项

- **位数匹配**：64 位 Windows 的 Explorer 只能加载 x64 DLL
- **管理员权限**：注册 COM 组件需要提升权限
- **性能**：避免在 `Initialize` / `QueryContextMenu` 中执行耗时操作；耗时任务应通过 `launch` 启动外部进程
- **Explorer 缓存**：修改 DLL 后建议注销再注册，必要时重启 Explorer

## 架构说明

```
Explorer 右键
    → IShellExtInit::Initialize   （解析选中项，过滤菜单）
    → IContextMenu::QueryContextMenu （插入可见项）
    → IContextMenu::InvokeCommand    （按 verb/offset 分发动作）
```

核心逻辑与 Shell COM 胶水代码分离：定制时只需改 `config/menu.json`（及可选的 `common.h` 回退项），不必改动 `FileContextMenuExt` 的接口实现。

## 许可证

基于微软示例代码（[Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)）修改。Microsoft 原始版权说明保留在源文件头部。
