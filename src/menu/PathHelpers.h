#pragma once

#include <windows.h>
#include <string>
#include <vector>

std::wstring QuotePath(const std::wstring& path);
std::wstring JoinQuotedPaths(const std::vector<std::wstring>& paths);
std::wstring GetParentDirectory(const std::wstring& path);
std::wstring ResolveModuleRelativePath(HINSTANCE module, const std::wstring& relativePath);
bool HasExtensionIgnoreCase(const std::wstring& path, const std::wstring& extension);