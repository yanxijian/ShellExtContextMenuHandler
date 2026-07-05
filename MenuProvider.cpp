#include "MenuProvider.h"
#include "Filter.h"
#include "MenuActionHandler.h"
#include "MenuConfig.h"
#include "ShellLog.h"
#include <ShlObj.h>
#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

MenuProvider::MenuProvider(HINSTANCE moduleInstance)
    : m_moduleInstance(moduleInstance),
    m_configLoaded(false)
{
    wchar_t modulePath[MAX_PATH] = {};
    if (GetModuleFileNameW(moduleInstance, modulePath, ARRAYSIZE(modulePath)) != 0)
    {
        wchar_t configPath[MAX_PATH] = {};
        if (SUCCEEDED(StringCchCopy(configPath, ARRAYSIZE(configPath), modulePath))
            && PathRemoveFileSpecW(configPath))
        {
            StringCchCatW(configPath, ARRAYSIZE(configPath), L"\\menu.json");
            m_configPath = configPath;
        }
    }
}

void MenuProvider::EnsureConfigLoaded()
{
    if (m_configLoaded)
    {
        return;
    }

    if (m_configPath.empty())
    {
        m_allItems = GetBuiltinMenuItems();
    }
    else
    {
        LoadMenuConfig(m_configPath, m_allItems);
    }

    m_configLoaded = true;
}

void MenuProvider::ResetContext(MenuContext& context)
{
    context.selectedPaths.clear();
    context.folderPath.clear();
    context.selectionCount = 0;
    context.hasFiles = false;
    context.hasFolders = false;
}

bool MenuProvider::TryPopulateFolderPath(LPCITEMIDLIST pidlFolder, MenuContext& context)
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

bool MenuProvider::TryPopulateFromDataObject(LPDATAOBJECT dataObject, MenuContext& context)
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

        context.selectedPaths.push_back(selectedPath);
        const DWORD attributes = GetFileAttributesW(selectedPath);
        if (attributes != INVALID_FILE_ATTRIBUTES
            && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
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

    context.selectionCount = static_cast<UINT>(context.selectedPaths.size());
    return context.selectionCount > 0;
}

HRESULT MenuProvider::BuildContext(
    LPCITEMIDLIST pidlFolder,
    LPDATAOBJECT dataObject,
    MenuContext& context,
    std::vector<MenuItemDef>& visibleItems)
{
    EnsureConfigLoaded();
    ResetContext(context);
    visibleItems.clear();

    TryPopulateFolderPath(pidlFolder, context);
    TryPopulateFromDataObject(dataObject, context);

    if (context.selectionCount == 0 && context.folderPath.empty())
    {
        return E_FAIL;
    }

    for (const auto& item : m_allItems)
    {
        if (MenuItemMatchesContext(item, context))
        {
            visibleItems.push_back(item);
        }
    }

    m_context = context;
    m_visibleItems = visibleItems;

    if (visibleItems.empty())
    {
        ShellLog(L"No visible menu items for current selection.");
        return E_FAIL;
    }

    return S_OK;
}

bool MenuProvider::TryGetItemByCommandOffset(UINT commandOffset, const MenuItemDef** item) const
{
    if (commandOffset >= m_visibleItems.size())
    {
        return false;
    }

    *item = &m_visibleItems[commandOffset];
    return true;
}

bool MenuProvider::TryGetItemByVerb(PCWSTR verb, const MenuItemDef** item) const
{
    if (verb == nullptr)
    {
        return false;
    }

    for (const auto& visibleItem : m_visibleItems)
    {
        if (_wcsicmp(visibleItem.verb.c_str(), verb) == 0)
        {
            *item = &visibleItem;
            return true;
        }
    }

    return false;
}

void MenuProvider::ExecuteItem(const MenuItemDef& item, HWND hwnd) const
{
    ExecuteMenuAction(item.action, m_context, hwnd);
}
