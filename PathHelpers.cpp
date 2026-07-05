#include "PathHelpers.h"
#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

std::wstring QuotePath(const std::wstring& path)
{
    if (path.empty())
    {
        return L"\"\"";
    }

    if (path.find(L' ') == std::wstring::npos && path.find(L'\t') == std::wstring::npos)
    {
        return path;
    }

    return L"\"" + path + L"\"";
}

std::wstring JoinQuotedPaths(const std::vector<std::wstring>& paths)
{
    std::wstring joined;
    for (size_t i = 0; i < paths.size(); ++i)
    {
        if (i > 0)
        {
            joined.append(L" ");
        }
        joined.append(QuotePath(paths[i]));
    }
    return joined;
}

std::wstring GetParentDirectory(const std::wstring& path)
{
    wchar_t directory[MAX_PATH] = {};
    if (FAILED(StringCchCopy(directory, ARRAYSIZE(directory), path.c_str())))
    {
        return L"";
    }

    if (!PathRemoveFileSpecW(directory))
    {
        return L"";
    }

    return directory;
}
