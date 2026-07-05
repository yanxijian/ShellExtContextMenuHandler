#include "JsonFilterGate.h"
#include <windows.h>

namespace
{
    bool PathMatchesExtension(const std::wstring& extension, const std::wstring& pattern)
    {
        if (pattern.empty())
        {
            return true;
        }

        if (extension.empty())
        {
            return false;
        }

        std::wstring normalizedPattern = pattern;
        if (normalizedPattern[0] != L'.')
        {
            normalizedPattern.insert(normalizedPattern.begin(), L'.');
        }

        return _wcsicmp(extension.c_str(), normalizedPattern.c_str()) == 0;
    }

    bool PathMatchesAnyExtension(
        const std::wstring& extension,
        const std::vector<std::wstring>& extensions)
    {
        if (extensions.empty())
        {
            return true;
        }

        for (const auto& candidate : extensions)
        {
            if (PathMatchesExtension(extension, candidate))
            {
                return true;
            }
        }

        return false;
    }
}

bool JsonFilterGate::ShouldShow(const MenuContext& context, const MenuItemDef& item)
{
    const MenuFilter& filter = item.filter;
    const UINT selectionCount = context.GetSelectionCount();

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

    for (const auto& selected : context.selected)
    {
        if (!filter.excludeExtensions.empty()
            && PathMatchesAnyExtension(selected.extension, filter.excludeExtensions))
        {
            return false;
        }

        if (!filter.extensions.empty()
            && !PathMatchesAnyExtension(selected.extension, filter.extensions))
        {
            return false;
        }

        if (filter.filesOnly && selected.isDirectory)
        {
            return false;
        }

        if (filter.foldersOnly && !selected.isDirectory)
        {
            return false;
        }
    }

    return true;
}
