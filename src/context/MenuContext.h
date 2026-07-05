#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct SelectedItem
{
    std::wstring path;
    std::wstring fileName;
    std::wstring extension;
    bool isDirectory = false;
    DWORD attributes = INVALID_FILE_ATTRIBUTES;
};

struct MenuContext
{
    std::vector<SelectedItem> selected;
    std::wstring folderPath;
    std::wstring progId;

    bool hasFiles = false;
    bool hasFolders = false;

    UINT GetSelectionCount() const
    {
        return static_cast<UINT>(selected.size());
    }

    std::vector<std::wstring> GetSelectedPaths() const
    {
        std::vector<std::wstring> paths;
        paths.reserve(selected.size());
        for (const auto& item : selected)
        {
            paths.push_back(item.path);
        }
        return paths;
    }
};
