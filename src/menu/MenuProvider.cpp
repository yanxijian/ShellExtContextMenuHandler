#include "MenuProvider.h"
#include "ContextBuilder.h"
#include "GateRegistry.h"
#include "MenuActionHandler.h"
#include "MenuConfig.h"
#include "ShellLog.h"
#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

MenuProvider::MenuProvider(HINSTANCE moduleInstance)
    : m_moduleInstance(moduleInstance),
    m_configLoaded(false),
    m_initialized(false)
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

bool MenuProvider::EvaluateItemGate(const MenuContext& context, const MenuItemDef& item) const
{
    IMenuItemGate* gate = GateRegistry::Instance().DefaultItemGate();
    return gate != nullptr && gate->ShouldShow(context, item);
}

MenuItemState MenuProvider::EvaluatePresentationGate(
    const MenuContext& context,
    const MenuItemDef& item) const
{
    IMenuItemPresentationGate* gate = GateRegistry::Instance().DefaultPresentationGate();
    if (gate == nullptr)
    {
        return MenuItemState::Enabled;
    }

    return gate->Evaluate(context, item);
}

HRESULT MenuProvider::Initialize(
    LPCITEMIDLIST pidlFolder,
    LPDATAOBJECT dataObject,
    HKEY hKeyProgID)
{
    m_candidateItems.clear();
    m_insertedItems.clear();
    m_initialized = false;

    if (!BuildMenuContext(pidlFolder, dataObject, hKeyProgID, m_context))
    {
        return E_FAIL;
    }

    IExtensionGate* extensionGate = GateRegistry::Instance().DefaultExtensionGate();
    if (extensionGate == nullptr || !extensionGate->ShouldActivate(m_context))
    {
        ShellLog(L"Extension gate rejected current context.");
        return E_FAIL;
    }

    EnsureConfigLoaded();
    for (const auto& item : m_allItems)
    {
        if (EvaluateItemGate(m_context, item))
        {
            m_candidateItems.push_back(item);
        }
    }

    if (m_candidateItems.empty())
    {
        ShellLog(L"No candidate menu items for current selection.");
        return E_FAIL;
    }

    m_initialized = true;
    return S_OK;
}

void MenuProvider::BuildInsertedItems(std::vector<InsertedMenuItem>& insertedItems)
{
    insertedItems.clear();
    if (!m_initialized)
    {
        return;
    }

    for (const auto& candidate : m_candidateItems)
    {
        const MenuItemState state = EvaluatePresentationGate(m_context, candidate);
        if (state == MenuItemState::Hidden)
        {
            continue;
        }

        InsertedMenuItem inserted;
        inserted.item = candidate;
        inserted.state = state;
        insertedItems.push_back(inserted);
    }

    m_insertedItems = insertedItems;
}

bool MenuProvider::TryGetInsertedItemByCommandOffset(UINT commandOffset, const InsertedMenuItem** item) const
{
    if (commandOffset >= m_insertedItems.size())
    {
        return false;
    }

    *item = &m_insertedItems[commandOffset];
    return true;
}

bool MenuProvider::TryGetInsertedItemByVerb(PCWSTR verb, const InsertedMenuItem** item) const
{
    if (verb == nullptr)
    {
        return false;
    }

    for (const auto& insertedItem : m_insertedItems)
    {
        if (_wcsicmp(insertedItem.item.verb.c_str(), verb) == 0)
        {
            *item = &insertedItem;
            return true;
        }
    }

    return false;
}

void MenuProvider::ExecuteItem(const InsertedMenuItem& item, HWND hwnd) const
{
    if (item.state == MenuItemState::Disabled)
    {
        ShellLog(L"Skipped disabled menu item: %s", item.item.id.c_str());
        return;
    }

    ExecuteMenuAction(item.item.action, m_context, hwnd);
}
