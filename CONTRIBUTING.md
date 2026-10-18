# 贡献指南

**简体中文** | [English](CONTRIBUTING.en.md)

感谢改进 **ShellExtContextMenuHandler**。

## 开发环境

- Windows x64
- Visual Studio 2026 C++ 工具链
- CMake 3.20 或更高版本
- Python 3.9 或更高版本

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DSHELLEXT_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/validate_menu_json.py
```

提交前运行仓库规则和检查器测试：

```powershell
python scripts/check_repo_rules.py
python -m unittest discover -s scripts/tests -v
```

可选安装 pre-commit hook：

```powershell
git config core.hooksPath .githooks
```

## 代码约定

- 手写 C/C++ 使用 UTF-8 BOM + CRLF；仓库根规则见 [AGENTS.md](AGENTS.md)。
- 遵循 `.clang-format` 和 `.editorconfig`，行宽上限 140。
- 通过 CMake `target_link_libraries` 链接 Windows 库，不使用 `#pragma comment(lib)`。
- 新增行为必须有测试；配置格式变更同步更新 `tools/validate_menu_json.py` 和文档。
- 用户可见变更记录到 `CHANGELOG.md` 的 `[Unreleased]`。
- 不提交 `build/`、`cmake-build-*` 等生成物。

## Pull request

1. 从 `master` 创建分支。
2. 保持一个 PR 聚焦一个问题。
3. 确认 CI、CTest、配置校验和仓库规则全部通过。
4. 填写 PR 模板。

## 报告问题

使用 Bug report issue 模板，附上 Windows 版本、DLL 位数、复现步骤、预期行为和实际行为。安全问题请按 [SECURITY.md](SECURITY.md) 私下报告。
