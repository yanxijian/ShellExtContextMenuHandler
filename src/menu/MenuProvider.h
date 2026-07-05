#pragma once

#include <windows.h>
#include <shlobj.h>
#include "InsertedMenuItem.h"
#include "MenuContext.h"
#include "MenuItem.h"
#include <vector>

class MenuProvider
{
public:
    explicit MenuProvider(HINSTANCE moduleInstance);

    HRESULT Initialize(
        LPCITEMIDLIST pidlFolder,
        LPDATAOBJECT dataObject,
        HKEY hKeyProgID);

    void BuildInsertedItems(std::vector<InsertedMenuItem>& insertedItems);

    bool TryGetInsertedItemByCommandOffset(UINT commandOffset, const InsertedMenuItem** item) const;
    bool TryGetInsertedItemByVerb(PCWSTR verb, const InsertedMenuItem** item) const;
    void ExecuteItem(const InsertedMenuItem& item, HWND hwnd) const;

    const MenuContext& GetContext() const { return m_context; }
    const std::vector<MenuItemDef>& GetCandidateItems() const { return m_candidateItems; }

private:
    void EnsureConfigLoaded();
    bool EvaluateItemGate(const MenuContext& context, const MenuItemDef& item) const;
    MenuItemState EvaluatePresentationGate(const MenuContext& context, const MenuItemDef& item) const;

    HINSTANCE m_moduleInstance;
    std::wstring m_configPath;
    std::vector<MenuItemDef> m_allItems;
    std::vector<MenuItemDef> m_candidateItems;
    std::vector<InsertedMenuItem> m_insertedItems;
    MenuContext m_context;
    bool m_configLoaded;
    bool m_initialized;
};
