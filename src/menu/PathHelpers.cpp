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

std::wstring ResolveModuleRelativePath(HINSTANCE module, const std::wstring& relativePath)
{
    if (relativePath.empty())
    {
        return L"";
    }

    if (PathIsRelativeW(relativePath.c_str()) == FALSE)
    {
        return relativePath;
    }

    wchar_t modulePath[MAX_PATH] = {};
    if (module == nullptr
        || GetModuleFileNameW(module, modulePath, ARRAYSIZE(modulePath)) == 0)
    {
        return relativePath;
    }

    if (!PathRemoveFileSpecW(modulePath))
    {
        return relativePath;
    }

    wchar_t combined[MAX_PATH] = {};
    if (FAILED(StringCchCopyW(combined, ARRAYSIZE(combined), modulePath))
        || FAILED(PathAppendW(combined, relativePath.c_str())))
    {
        return relativePath;
    }

    return combined;
}

bool HasExtensionIgnoreCase(const std::wstring& path, const std::wstring& extension)
{
    if (path.empty() || extension.empty())
    {
        return false;
    }

    const size_t pathLength = path.size();
    const size_t extensionLength = extension.size();
    if (pathLength < extensionLength)
    {
        return false;
    }

    return _wcsicmp(path.c_str() + (pathLength - extensionLength), extension.c_str()) == 0;
}