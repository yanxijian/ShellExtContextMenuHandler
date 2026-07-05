#include "Filter.h"
#include <windows.h>
#include <algorithm>
#include <cwctype>

namespace
{
    std::wstring ToLower(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        return value;
    }

    bool IsDirectoryPath(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
}

bool PathMatchesExtension(const std::wstring& path, const std::wstring& extension)
{
    if (extension.empty())
    {
        return true;
    }

    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    if (dot == nullptr)
    {
        return false;
    }

    std::wstring normalizedExtension = extension;
    if (normalizedExtension[0] != L'.')
    {
        normalizedExtension.insert(normalizedExtension.begin(), L'.');
    }

    return _wcsicmp(dot, normalizedExtension.c_str()) == 0;
}

bool PathMatchesAnyExtension(
    const std::wstring& path,
    const std::vector<std::wstring>& extensions)
{
    if (extensions.empty())
    {
        return true;
    }

    for (const auto& extension : extensions)
    {
        if (PathMatchesExtension(path, extension))
        {
            return true;
        }
    }

    return false;
}

bool MenuItemMatchesContext(const MenuItemDef& item, const MenuContext& context)
{
    const MenuFilter& filter = item.filter;
    const UINT selectionCount = context.selectionCount;

    if (filter.minSelection > 0 && selectionCount < filter.minSelection)
    {
        return false;
    }

    if (filter.maxSelection > 0 && selectionCount > filter.maxSelection)
    {
        return false;
    }

    if (filter.foldersOnly)
    {
        if (!context.hasFolders)
        {
            return false;
        }

        if (filter.filesOnly && context.hasFiles)
        {
            return false;
        }
    }
    else if (filter.filesOnly && context.hasFolders && !context.hasFiles)
    {
        return false;
    }

    if (selectionCount == 0)
    {
        return filter.foldersOnly && !context.folderPath.empty();
    }

    for (const auto& path : context.selectedPaths)
    {
        if (!filter.excludeExtensions.empty()
            && PathMatchesAnyExtension(path, filter.excludeExtensions))
        {
            return false;
        }

        if (!filter.extensions.empty()
            && !PathMatchesAnyExtension(path, filter.extensions))
        {
            return false;
        }

        if (filter.filesOnly && IsDirectoryPath(path))
        {
            return false;
        }

        if (filter.foldersOnly && !IsDirectoryPath(path))
        {
            return false;
        }
    }

    return true;
}
