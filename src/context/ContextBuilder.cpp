#include "ContextBuilder.h"
#include "DpiProvider.h"
#include <ShlObj.h>
#include <Shlwapi.h>
#include <stdlib.h>

#pragma comment(lib, "shlwapi.lib")

namespace
{
    void ParsePathComponents(const std::wstring& path, SelectedItem& item)
    {
        wchar_t fileName[MAX_PATH] = {};
        wchar_t extension[MAX_PATH] = {};
        _wsplitpath_s(path.c_str(), nullptr, 0, nullptr, 0, fileName, MAX_PATH, extension, MAX_PATH);
        item.fileName = fileName;
        item.extension = extension;
    }

    bool TryReadProgId(HKEY hKeyProgID, std::wstring& progId)
    {
        if (hKeyProgID == nullptr)
        {
            return false;
        }

        wchar_t buffer[260] = {};
        LONG bufferChars = ARRAYSIZE(buffer);
        const LONG result = RegQueryValueW(hKeyProgID, nullptr, buffer, &bufferChars);
        if (result != ERROR_SUCCESS || buffer[0] == L'\0')
        {
            return false;
        }

        progId = buffer;
        return true;
    }

    bool TryPopulateFolderPath(LPCITEMIDLIST pidlFolder, MenuContext& context)
    {
        if (pidlFolder == nullptr)
        {
            return false;
        }

        wchar_t folderPath[MAX_PATH] = {};
        if (!SHGetPathFromIDListW(pidlFolder, folderPath))
        {
            return false;
        }

        context.folderPath = folderPath;
        return true;
    }

    bool TryPopulateFromDataObject(LPDATAOBJECT dataObject, MenuContext& context)
    {
        if (dataObject == nullptr)
        {
            return false;
        }

        FORMATETC format = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM storage = {};
        if (!SUCCEEDED(dataObject->GetData(&format, &storage)))
        {
            return false;
        }

        HDROP dropHandle = static_cast<HDROP>(GlobalLock(storage.hGlobal));
        if (dropHandle == nullptr)
        {
            ReleaseStgMedium(&storage);
            return false;
        }

        const UINT fileCount = DragQueryFileW(dropHandle, 0xFFFFFFFF, nullptr, 0);
        for (UINT index = 0; index < fileCount; ++index)
        {
            wchar_t selectedPath[MAX_PATH] = {};
            if (DragQueryFileW(dropHandle, index, selectedPath, ARRAYSIZE(selectedPath)) == 0)
            {
                continue;
            }

            SelectedItem item;
            item.path = selectedPath;
            item.attributes = GetFileAttributesW(selectedPath);
            item.isDirectory = item.attributes != INVALID_FILE_ATTRIBUTES
                && (item.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            ParsePathComponents(item.path, item);

            context.selected.push_back(item);
            if (item.isDirectory)
            {
                context.hasFolders = true;
            }
            else
            {
                context.hasFiles = true;
            }
        }

        GlobalUnlock(storage.hGlobal);
        ReleaseStgMedium(&storage);
        return !context.selected.empty();
    }
}

bool BuildMenuContext(
    LPCITEMIDLIST pidlFolder,
    LPDATAOBJECT dataObject,
    HKEY hKeyProgID,
    MenuContext& context)
{
    context = MenuContext();
    TryReadProgId(hKeyProgID, context.progId);
    TryPopulateFolderPath(pidlFolder, context);
    TryPopulateFromDataObject(dataObject, context);
    context.systemDpi = DpiProvider::GetSystemDpi();
    return context.GetSelectionCount() > 0 || !context.folderPath.empty();
}
