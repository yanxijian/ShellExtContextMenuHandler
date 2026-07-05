#pragma once

#include <string>
#include <vector>

std::wstring QuotePath(const std::wstring& path);
std::wstring JoinQuotedPaths(const std::vector<std::wstring>& paths);
std::wstring GetParentDirectory(const std::wstring& path);
