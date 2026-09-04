# ShellExtContextMenuHandler

**简体中文** | [English](README.en.md)

基于微软官方 C++ Shell 扩展示例改造的 Windows Explorer 右键菜单扩展。菜单项、Shell 注册目标、过滤器、Gate、Executor 和图标都可以配置，复杂逻辑通过 C++ 扩展。

## 免责声明

本项目主要由 AI Agent 辅助开发，部分代码、设计和文档内容尚未经过充分、严谨和全面的测试验证。项目仅供学习、研究和技术参考，不保证在所有系统、环境和使用场景下的正确性、稳定性或安全性。

在将本项目用于重要项目、商业软件或生产环境之前，请根据实际需求进行充分的代码审查、功能定制、安全评估和兼容性测试。因直接或间接使用本项目造成的任何数据丢失、系统故障、业务中断或其他损失，项目作者不承担任何责任。

## 功能概览

- 默认注册五类 Shell 目标：文件、目录、目录背景、驱动器和文件系统对象。
- 使用 `config/menu.json` 配置菜单项、目标过滤、选择过滤、Gate/Executor 链和动作参数。
- 使用 `config/registration.json` 控制 DLL 注册到哪些 Shell 注册位置。
- 支持 `messageBox` 和 `launch` 动作，以及自定义 Executor。
- 支持 SVG/BMP 图标，SVG 由现有 NanoSVG 提供器加载并按 DPI 栅格化。
- 三层 Gate：Extension、Item、Presentation，可分别控制扩展参与、菜单候选和最终显示状态。
- 内置 Gate：`extensionPass`、`jsonFilter`、`presentationPass`、`demo:hideTemp`、`demo:readOnlyDisable`；内置 Executor：`messageBox`、`launch`、`demo:actionLog`。
- 构建输出包含可双击的 `register.bat` 和 `unregister.bat`，会请求管理员权限调用 `regsvr32`。
- 通过 `OutputDebugString` 输出 `[ShellExt]` 调试日志。
- 内置 CTest 单元测试覆盖配置解析、占位符展开、上下文构建和启动命令解析，CI 自动运行。

## 快速开始

### 环境

- Windows x64
- Visual Studio 2026 C++ 工具链
- CMake 3.20 或更高版本
- Python 3，用于配置校验

项目的设计目标是 Windows 7 SP1 及之后版本，但发布前必须在目标 Windows 版本和对应位数的 Explorer 中验证。

### 构建

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

单元测试默认关闭（`SHELLEXT_BUILD_TESTS=ON` 启用），位于 `tests/`，直接编译真实源码而非副本。

也可以运行 `tools/generate-vs.ps1` 生成 Visual Studio 工程。

构建输出位于 `build/bin/Release/`，包括：

```text
CppShellExtContextMenuHandler.dll
menu.json
registration.json
register.bat
unregister.bat
icons/
```

### 注册、测试和卸载

推荐直接双击 DLL 输出目录中的 `register.bat`。它会使用当前目录的 DLL，并通过 UAC 提权注册。卸载时双击 `unregister.bat`。

也可以在管理员 PowerShell 中执行：

```powershell
.\tools\register.ps1 -Action register
.\tools\register.ps1 -Action unregister
```

使用 `register.ps1` 时，脚本会把 `menu.json`、`registration.json` 和 `config/icons/` 复制到 DLL 同目录。更新 DLL 或配置后，先卸载再重新注册；Explorer 缓存未刷新时可重启 Explorer。

### 手动验证

1. 先运行 `unregister.bat`，再运行 `register.bat`。
2. 右键一个普通文件，确认出现 `Demo -F`。
3. 右键文件夹，确认出现 `Demo -D`。
4. 在文件夹空白区域右键，确认出现 `Demo -DB`。
5. 右键驱动器根目录，确认出现 `Demo -DR`。
6. 同时选中文件和目录，确认出现 `Demo -FS`。
7. 执行菜单项，MessageBox 应显示目标名称。

## 配置参考

### Shell 注册目标

`config/registration.json`：

```json
{
  "schemaVersion": 1,
  "shellRegistrations": [
    "file",
    "directory",
    "directoryBackground",
    "drive",
    "fileSystemObject"
  ]
}
```

| 目标 | 注册表位置 | 运行时上下文 |
|------|------------|--------------|
| `file` | `*` | 仅文件选择 |
| `directory` | `Directory` | 仅目录选择 |
| `directoryBackground` | `Directory\\Background` | 无选中、当前目录背景 |
| `drive` | `Drive` | 驱动器根目录 |
| `fileSystemObject` | `AllFilesystemObjects` | 文件和目录混合选择 |

注册配置缺失时默认使用 `file`。配置文件存在但格式错误、版本不支持、目标未知或目标重复时，注册会失败，不会静默修改注册表。卸载会清理所有已知目标，因此可以清理旧版本留下的注册项。

### `menu.json` 根级字段

```json
{
  "extensionGates": ["extensionPass"],
  "itemGates": ["jsonFilter"],
  "presentationGates": ["presentationPass"],
  "executors": ["messageBox"],
  "menuItems": []
}
```

| 字段 | 说明 |
|------|------|
| `extensionGates` | Gate 1 链；决定当前扩展是否参与 |
| `itemGates` | Gate 2 链；决定菜单项是否进入候选集，`gates` 是兼容别名 |
| `presentationGates` | Gate 3 链；决定隐藏、禁用或启用 |
| `executors` | 默认动作执行器链 |
| `menuItems` | 菜单项定义数组 |

`menu.json` 必须是 UTF-8 编码（可带 BOM），由 `third_party/nlohmann/json` 解析；字段类型不匹配视为配置错误，回退到内置菜单。

菜单项链字段的覆盖语义：显式配置的链**整体替换**根级默认链，而不是追加；根级默认链只在配置缺失或为空时使用。

### 菜单项字段

| 字段 | 说明 |
|------|------|
| `id` | 唯一标识 |
| `label` | 菜单文字，`&` 表示快捷键 |
| `verb` | 字符串命令名；InvokeCommand 按它或 `canonicalName` 匹配 |
| `helpText` | Explorer 状态栏帮助文字 |
| `canonicalName` | 规范命令名；InvokeCommand 匹配时与 `verb` 等效接受 |
| `icon` | 相对 DLL 目录的 SVG/BMP 路径 |
| `targets` | 目标数组：`file`、`directory`、`directoryBackground`、`drive`、`fileSystemObject` |
| `separatorAfter` | 此项后插入分隔线 |
| `extensions` | 允许的扩展名数组，例如 `[".cpp", ".h"]` |
| `excludeExtensions` | 排除的扩展名数组 |
| `minSelection` / `maxSelection` | 选择数量；`maxSelection: 0` 表示不限 |
| `filesOnly` / `foldersOnly` | 限制选择内容类型 |
| `actionType` | `messageBox` 或 `launch` |
| `actionTitle` / `actionTemplate` | MessageBox 标题和内容 |
| `actionCommand` / `actionShowWindow` | 启动命令和窗口显示选项 |
| `extensionGates` / `itemGates` / `presentationGates` / `executors` | 对根级链的逐项覆盖；显式配置的链替换根级默认 |

`targets` 为空时保持兼容行为，由 `filesOnly`、`foldersOnly` 等过滤条件决定；新菜单项建议显式填写 `targets`。

### 占位符

| 占位符 | 含义 |
|--------|------|
| `%1` | 第一个选中对象的完整路径；无选中时为当前目录 |
| `%*` | 所有选中路径，按引用格式拼接 |
| `%D` | 第一个选中对象的父目录；无选中时为当前目录 |
| `%N` | 第一个选中对象或当前目录的名称，不含父路径 |

配置无效或缺失时，运行时回退到 `include/shell_ext/common.h` 中的内置菜单项和默认 Gate 链。

占位符展开为单趟扫描：替换后的结果不会被重新解析，因此选中对象的名称中包含 `%1`、`%N` 等 token 也是安全的。

### launch 命令解析

`launch` 动作在启动前显式解析 `actionCommand` 中的可执行文件：

- 带引号的路径原样使用。
- 不带引号且含空格的路径按 CreateProcess 文档规则做前缀渐进匹配。
- 裸命令名（如 `notepad.exe`）只在系统目录和 PATH 的绝对项中搜索，**不搜索当前目录**，避免当前目录同名程序被劫持。
- 解析失败时记录日志并拒绝启动。

### 超长路径

选中对象路径不受 `MAX_PATH` 限制，完整保留到上下文和占位符中。

## 二次开发

### 增加菜单项

1. 在 `config/menu.json` 的 `menuItems` 中增加对象。
2. 为对象设置唯一 `id`、`verb`、`targets`、过滤条件和动作字段。
3. 如果使用图标，把 SVG/BMP 放在 `config/icons/`，并填写相对路径。
4. 运行 `python tools/validate_menu_json.py`。

### 增加 Gate

1. 在 `src/gates/` 添加实现 `IExtensionGate`、`IMenuItemGate` 或 `IMenuItemPresentationGate` 的类。
2. 在 `GateRegistry::RegisterBuiltInGates` 中注册名称。
3. 在 `menu.json` 的对应 Gate 链中使用名称。

Extension Gate 返回是否继续参与；Item Gate 返回是否进入候选集；Presentation Gate 返回 `Hidden`、`Disabled` 或 `Enabled`。

### 增加 Executor

1. 在 `src/actions/` 添加实现 `IActionExecutor` 的类。
2. 在 `ExecutorRegistry::RegisterBuiltInExecutors` 中注册名称。
3. 在根级或菜单项的 `executors` 数组中配置名称。
4. 在 `MenuAction` 中增加必要的配置字段，并在 `MenuConfig` 解析。

### 增加 Shell 目标

1. 在 `ShellTargetType` 增加抽象枚举。
2. 在 `ParseShellTargetType` 和 `GetShellTargetRegistryFileType` 增加配置名称与注册表位置映射。
3. 在 `ContextBuilder` 增加实际上下文识别。
4. 在菜单目标匹配和注册/卸载的已知目标列表中同步处理。
5. 更新 `tools/validate_menu_json.py`、README 和架构文档。

### 修改 COM 注册

注册入口位于 `src/extension/dllmain.cpp`，注册表辅助函数位于 `src/registry/Reg.cpp`。注册流程必须保留部分失败回滚、重复执行幂等和旧目标清理逻辑。不要直接删除整个 `ContextMenuHandlers` 父键，只删除本项目 CLSID 对应的子键。

## 调试与排错

- 使用 DebugView 查看 `[ShellExt]` 日志。
- 确认 DLL 位数与 Explorer 位数一致。
- 修改 DLL 或配置后重新注册，必要时重启 Explorer。
- 如果菜单没有出现，先确认 `registration.json` 的目标、菜单项 `targets`、`filesOnly/foldersOnly` 和 Gate 链是否同时允许当前上下文。
- 如果注册失败，检查管理员权限、`registration.json` 格式和 `regsvr32` 位数。

## 项目结构

```text
config/                 运行时菜单、注册目标和图标
src/extension/          COM DLL 入口和 Shell 扩展对象
src/context/            选择上下文和 Shell 目标识别
src/gates/              Gate 接口、注册表和内置 Gate
src/actions/            Executor 接口、注册表和动作实现
src/icons/              SVG/BMP 图标提供器
src/menu/               配置、过滤、菜单插入和命令执行
src/registry/           COM/Shell 注册表辅助
tools/                  构建辅助、注册脚本和配置校验
tests/                  CTest 单元测试
third_party/            第三方库（nlohmann/json）
docs/                   架构与验证文档
```

## 注意事项

- 64 位 Explorer 需要 64 位 DLL；32 位 Explorer 需要对应位数 DLL。
- COM 注册需要管理员权限。
- Win7 支持是设计目标，发布前必须在 Win7 SP1 x64 虚拟机完成回归。
- 不要修改 `build/`、`build-*` 或第三方目录中的生成文件。

## COM 标识与许可证

发布 Fork 时请修改 `src/extension/dllmain.cpp` 中的 `CLSID_FileContextMenuExt` 以及 `include/shell_ext/common.h` 中的友好名称。

本项目派生自微软示例，源文件保留微软原始版权头，许可证为 [Ms-PL](https://www.microsoft.com/opensource/licenses.mspx#Ms-PL)。
