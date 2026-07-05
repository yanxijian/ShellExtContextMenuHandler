#pragma once

#include <windows.h>
#include <shlobj.h>
#include "MenuContext.h"
#include "MenuItem.h"
#include <vector>

class MenuProvider
{
public:
    explicit MenuProvider(HINSTANCE moduleInstance);

    HRESULT BuildContext(
        LPCITEMIDLIST pidlFolder,
        LPDATAOBJECT dataObject,
        MenuContext& context,
        std::vector<MenuItemDef>& visibleItems);

    bool TryGetItemByCommandOffset(UINT commandOffset, const MenuItemDef** item) const;
    bool TryGetItemByVerb(PCWSTR verb, const MenuItemDef** item) const;
    void ExecuteItem(const MenuItemDef& item, HWND hwnd) const;

    const MenuContext& GetContext() const { return m_context; }

private:
    void EnsureConfigLoaded();
    void ResetContext(MenuContext& context);
    bool TryPopulateFromDataObject(LPDATAOBJECT dataObject, MenuContext& context);
    bool TryPopulateFolderPath(LPCITEMIDLIST pidlFolder, MenuContext& context);

    HINSTANCE m_moduleInstance;
    std::wstring m_configPath;
    std::vector<MenuItemDef> m_allItems;
    std::vector<MenuItemDef> m_visibleItems;
    MenuContext m_context;
    bool m_configLoaded;
};
