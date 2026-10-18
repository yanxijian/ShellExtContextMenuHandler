#include "MenuProvider.h"

#include "ContextBuilder.h"
#include "ExecutorRegistry.h"
#include "GateRegistry.h"
#include "MenuConfig.h"
#include "PathHelpers.h"
#include "ShellLog.h"

#include <Shlwapi.h>
#include <algorithm>
#include <strsafe.h>

namespace
{
	std::wstring NormalizeGateName(const std::wstring& name)
	{
		const std::wstring prefix = L"custom:";
		if (name.size() > prefix.size() && _wcsnicmp(name.c_str(), prefix.c_str(), static_cast<int>(prefix.size())) == 0)
		{
			return name.substr(prefix.size());
		}
		return name;
	}

	bool HasExtensionGate(const std::wstring& name)
	{
		return GateRegistry::Instance().GetExtensionGate(NormalizeGateName(name)) != nullptr;
	}

	bool HasItemGate(const std::wstring& name)
	{
		return GateRegistry::Instance().GetItemGate(NormalizeGateName(name)) != nullptr;
	}

	bool HasPresentationGate(const std::wstring& name)
	{
		return GateRegistry::Instance().GetPresentationGate(NormalizeGateName(name)) != nullptr;
	}

	bool HasExecutor(const std::wstring& name)
	{
		return ExecutorRegistry::Instance().GetExecutor(name) != nullptr;
	}

	template <typename Predicate>
	bool ValidateChain(const std::wstring& kind, const std::vector<std::wstring>& names, Predicate&& exists)
	{
		for (const auto& name : names)
		{
			if (!exists(name))
			{
				ShellLog(L"Unknown %s name in menu.json: %s", kind.c_str(), name.c_str());
				return false;
			}
		}
		return true;
	}

	bool ValidateConfigChains(const MenuConfigDocument& document)
	{
		if (!ValidateChain(L"extension gate", document.globalChains.extensionGates, HasExtensionGate)
			|| !ValidateChain(L"item gate", document.globalChains.itemGates, HasItemGate)
			|| !ValidateChain(L"presentation gate", document.globalChains.presentationGates, HasPresentationGate)
			|| !ValidateChain(L"executor", document.globalChains.executors, HasExecutor))
		{
			return false;
		}

		for (const auto& item : document.items)
		{
			if (!ValidateChain(L"extension gate", item.extensionGates, HasExtensionGate)
				|| !ValidateChain(L"item gate", item.itemGates, HasItemGate)
				|| !ValidateChain(L"presentation gate", item.presentationGates, HasPresentationGate)
				|| !ValidateChain(L"executor", item.executors, HasExecutor))
			{
				return false;
			}
		}

		return true;
	}
} // namespace

MenuProvider::MenuProvider(HINSTANCE moduleInstance)
	: m_moduleInstance(moduleInstance)
	, m_configLoaded(false)
	, m_initialized(false)
{
	const std::wstring modulePath = GetModuleFilePath(moduleInstance);
	if (!modulePath.empty())
	{
		const size_t separator = modulePath.find_last_of(L"\\/");
		if (separator != std::wstring::npos)
		{
			m_configPath = modulePath.substr(0, separator + 1) + L"menu.json";
		}
	}
}

void MenuProvider::EnsureConfigLoaded()
{
	if (m_configLoaded)
	{
		return;
	}

	MenuConfigDocument document;
	bool loaded = false;
	if (m_configPath.empty())
	{
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
		loaded = true;
	}
	else
	{
		loaded = LoadMenuConfigDocument(m_configPath, document);
		if (loaded && !ValidateConfigChains(document))
		{
			ShellLog(L"Invalid chain configuration, using built-in menu: %s", m_configPath.c_str());
			loaded = false;
		}
	}

	if (!loaded)
	{
		document = {};
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
	}

	m_globalChains = document.globalChains;
	m_allItems = document.items;
	m_configLoaded = true;
}

bool MenuProvider::PassesTargetFilter(const MenuItemDef& item) const
{
	if (item.targets.empty())
	{
		return true;
	}

	return std::any_of(item.targets.begin(), item.targets.end(),
					   [this](ShellTargetType configuredTarget)
					   {
						   return IsShellTargetMatch(configuredTarget, m_context.targetType);
					   });
}

bool MenuProvider::PassesExtensionGates(const MenuItemDef& item) const
{
	if (item.extensionGates.empty())
	{
		return true;
	}

	return GateRegistry::Instance().EvaluateExtensionChain(m_context, item.extensionGates);
}

const std::vector<std::wstring>& MenuProvider::ResolveItemGates(const MenuItemDef& item) const
{
	return item.itemGates.empty() ? m_globalChains.itemGates : item.itemGates;
}

const std::vector<std::wstring>& MenuProvider::ResolvePresentationGates(const MenuItemDef& item) const
{
	return item.presentationGates.empty() ? m_globalChains.presentationGates : item.presentationGates;
}

const std::vector<std::wstring>& MenuProvider::ResolveExecutors(const MenuItemDef& item) const
{
	return item.executors.empty() ? m_globalChains.executors : item.executors;
}

HRESULT MenuProvider::Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY hKeyProgID)
{
	m_candidateItems.clear();
	m_insertedItems.clear();
	m_initialized = false;

	if (!BuildMenuContext(pidlFolder, dataObject, hKeyProgID, m_context))
	{
		return E_FAIL;
	}

	EnsureConfigLoaded();

	if (!GateRegistry::Instance().EvaluateExtensionChain(m_context, m_globalChains.extensionGates))
	{
		ShellLog(L"Extension gate chain rejected current context.");
		return E_FAIL;
	}

	for (const auto& item : m_allItems)
	{
		if (!PassesTargetFilter(item))
		{
			continue;
		}

		if (!PassesExtensionGates(item))
		{
			continue;
		}

		if (GateRegistry::Instance().EvaluateItemChain(m_context, item, ResolveItemGates(item)))
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
		const MenuItemState state =
			GateRegistry::Instance().EvaluatePresentationChain(m_context, candidate, ResolvePresentationGates(candidate));
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
		const MenuItemDef& definition = insertedItem.item;
		// GetCommandString(GCS_VERBW) hands out the canonical name while hosts
		// may also invoke with the configured verb, so accept either spelling.
		if (_wcsicmp(definition.verb.c_str(), verb) == 0 || _wcsicmp(definition.canonicalName.c_str(), verb) == 0)
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

	ExecutorRegistry::Instance().ExecuteChain(m_context, item.item.action, hwnd, ResolveExecutors(item.item));
}