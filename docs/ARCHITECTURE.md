# 架构说明

**简体中文** | [English](ARCHITECTURE.en.md)

## 设计边界

本项目是一个 Windows Explorer `IContextMenu` Shell 扩展，设计目标为 Windows 7 SP1 及之后版本。COM DLL 负责上下文菜单生命周期；JSON 负责可变配置；C++ 负责 Gate、Executor、Shell 目标识别和注册表生命周期。

## 运行时流程

```text
Explorer 调用 IShellExtInit::Initialize
  -> ContextBuilder 构建 MenuContext
     - 选中路径、文件名、扩展名、属性、ProgID
     - ShellTargetType：file / directory / directoryBackground / drive / fileSystemObject
  -> Gate 1：全局 extensionGates
  -> 逐项目标匹配和 Gate 2：itemGates
  -> candidateItems

Explorer 调用 IContextMenu::QueryContextMenu
  -> Gate 3：presentationGates
  -> Hidden 不插入，Disabled 插入但禁用，Enabled 正常插入
  -> 为 insertedItems 分配命令 ID
  -> IconProviderRegistry 按 SVG/BMP/default 加载图标

Explorer 调用 IContextMenu::InvokeCommand
  -> 根据命令 offset 或 verb 找到 insertedItem
  -> Disabled 项不执行
  -> ExecutorRegistry 按配置顺序执行动作
```

Gate 1 没有通过或没有候选项时，扩展返回 `E_FAIL`；Gate 3 只影响当前菜单项的最终显示状态。配置在进程内首次加载后缓存。

## Shell 注册目标

注册配置位于 `config/registration.json`，由 `DllRegisterServer` 读取。目标映射如下：

| 抽象目标 | 注册位置 | 上下文识别 |
|----------|----------|------------|
| `file` | `*` | 仅文件 |
| `directory` | `Directory` | 仅目录 |
| `directoryBackground` | `Directory\\Background` | 无选中且存在当前目录 |
| `drive` | `Drive` | 单个盘符根目录，例如 `C:\\` |
| `fileSystemObject` | `AllFilesystemObjects` | 同时存在文件和目录的混合选择 |

`fileSystemObject` 的注册范围是 Windows Shell 的文件系统对象范围，但当前 Demo 菜单只在混合选择上下文显示，避免与单文件 `-F` 和单目录 `-D` 重复。

注册流程：

1. 注册 COM `InprocServer32`。
2. 读取并校验 `registration.json`。
3. 注册配置中的目标；任一目标失败时回滚本次注册。
4. 清理已知但未启用的旧目标；清理失败时保留当前注册并记录日志。
5. 卸载时清理所有已知目标和本项目 CLSID，目标不存在视为成功。

缺失 `registration.json` 时默认注册 `file`；存在但无效时注册失败，不静默降级。配置目标必须唯一且属于校验脚本允许的目标集合。

## MenuContext 与目标分类

`ContextBuilder` 每次右键只构建一次 `MenuContext`。选择对象包含完整路径、文件名、扩展名、目录标记和文件属性。

- 无选中且有 `folderPath`：`DirectoryBackground`
- 仅目录选择：`Directory`；单个盘符根目录进一步识别为 `Drive`
- 同时有文件和目录：`FileSystemObject`
- 其余文件选择：`File`

Gate 和 Executor 只读取已构建的 `MenuContext`，不应在 Gate 内重复访问文件系统。

## 配置模型

根级链可以被菜单项局部覆盖：

```json
{
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

菜单项目标先于 Gate 和选择过滤执行。目标为空时保持兼容行为，由 `filesOnly`、`foldersOnly` 和扩展名等过滤条件决定。

动作支持：

- `messageBox`：展开占位符后显示 MessageBox。
- `launch`：展开占位符后启动命令行。

占位符：`%1` 为第一个完整路径，`%*` 为所有路径，`%D` 为父目录，`%N` 为第一个对象或当前目录的名称。

## Gate 与 Executor 扩展

Gate 分为三类：

| 类型 | 接口 | 时机 | 结果 |
|------|------|------|------|
| Extension | `IExtensionGate` | Initialize | 允许或拒绝扩展参与 |
| Item | `IMenuItemGate` | Initialize | 进入或排除候选菜单项 |
| Presentation | `IMenuItemPresentationGate` | QueryContextMenu | Hidden、Disabled 或 Enabled |

Extension/Item 链按 AND 语义求值；Presentation 状态按 Hidden 优先于 Disabled，Disabled 优先于 Enabled 的规则合并。

新增 Gate：在 `src/gates/` 实现接口，并在 `GateRegistry::RegisterBuiltInGates` 注册名称。新增 Executor：在 `src/actions/` 实现 `IActionExecutor`，并在 `ExecutorRegistry::RegisterBuiltInExecutors` 注册名称。

## 图标加载

`IconProviderRegistry` 按提供器链处理 `icon`：

1. SVG：`SvgIconProvider` 使用 NanoSVG 读取 DLL 目录相对路径并按 DPI 栅格化。
2. BMP：`BitmapFileIconProvider` 加载位图文件。
3. 空图标或失败回退：`DefaultResourceIconProvider` 使用资源位图。

Demo 的 `file-F.svg`、`directory-D.svg`、`directoryBackground-DB.svg`、`drive-DR.svg`、`fileSystemObject-FS.svg` 都是独立 SVG，不应把它们硬编码进 C++。

## 源码布局

```text
src/extension/  COM DLL 入口、ClassFactory、FileContextMenuExt
src/context/   MenuContext、ContextBuilder、ShellTargetType
src/gates/     Gate 接口、注册表、内置 Gate
src/actions/   Executor 接口、注册表、动作实现
src/icons/     SVG/BMP/default 图标提供器
src/menu/      配置解析、过滤、菜单插入和命令执行
src/registry/  COM/Shell 注册表辅助和注册配置解析
config/        menu.json、registration.json、icons/
tools/         构建辅助、register.ps1、BAT 模板、配置校验
```

## 二次开发流程

1. 修改 `config/menu.json` 或 `config/registration.json`。
2. 新增 C++ Gate、Executor 或 Shell target 时同步更新 `CMakeLists.txt`、配置校验和文档。
3. 构建 Release 并运行 `python tools/validate_menu_json.py`。
4. 使用输出目录的 `register.bat` 重新注册。
5. 在文件、目录、目录背景、驱动器和混合选择场景逐项验证。

## 验证清单

### 自动化

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
python tools/validate_menu_json.py
git diff --check
```

CI 在 push/PR 时执行 Release 构建和菜单配置校验。手写 C/C++ 还应遵守根目录 `.clang-format` 及 `.cursor/rules/` 中的编码、命名和参数规则。

### 手动

| 场景 | 预期 |
|------|------|
| 文件右键 | 只显示文件目标菜单，如 `Demo -F` |
| 文件夹右键 | 显示 `Demo -D` |
| 文件夹背景右键 | 显示 `Demo -DB` |
| 驱动器根目录右键 | 显示 `Demo -DR` |
| 文件与目录混选 | 显示 `Demo -FS` |
| MessageBox | `%N` 显示名称，不包含父路径 |
| 只读/临时目录 | 对应 Gate 按配置隐藏或禁用 |
| 高 DPI | SVG 图标清晰，菜单布局不抖动 |
| 卸载后注册表 | 本项目目标和 CLSID 清理完整 |

## 平台和安全边界

- DLL 位数必须匹配 Explorer。
- 注册和卸载需要管理员权限。
- 注册表操作只删除本项目 CLSID 对应键。
- Gate 不应执行扫描磁盘、网络访问等重 IO。
- Win7 支持在发布前必须通过虚拟机回归。
