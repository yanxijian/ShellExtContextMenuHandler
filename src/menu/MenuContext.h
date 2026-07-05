#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct MenuContext
{
    std::vector<std::wstring> selectedPaths;
    std::wstring folderPath;
    UINT selectionCount = 0;
    bool hasFiles = false;
    bool hasFolders = false;
};
