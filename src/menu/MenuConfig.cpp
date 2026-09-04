#include "MenuConfig.h"

#include "MenuGateChains.h"
#include "ShellLog.h"
#include "common.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <climits>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
	using Json = nlohmann::json;

	bool ReadUtf8File(const std::wstring& path, std::string& content)
	{
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
		{
			return false;
		}

		if (fseek(file, 0, SEEK_END) != 0)
		{
			fclose(file);
			return false;
		}

		const long fileSize = ftell(file);
		if (fileSize < 0)
		{
			fclose(file);
			return false;
		}

		if (fseek(file, 0, SEEK_SET) != 0)
		{
			fclose(file);
			return false;
		}

		std::vector<char> bytes(static_cast<size_t>(fileSize));
		if (fileSize > 0 && fread(bytes.data(), 1, static_cast<size_t>(fileSize), file) != static_cast<size_t>(fileSize))
		{
			fclose(file);
			return false;
		}

		fclose(file);
		content.assign(bytes.begin(), bytes.end());

		// Strip a UTF-8 byte order mark if present.
		if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF && static_cast<unsigned char>(content[1]) == 0xBB
			&& static_cast<unsigned char>(content[2]) == 0xBF)
		{
			content.erase(0, 3);
		}

		return true;
	}

	std::wstring Utf8ToWide(const std::string& value)
	{
		if (value.empty())
		{
			return std::wstring();
		}

		const int wideLength = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (wideLength <= 0)
		{
			return std::wstring();
		}

		std::wstring wide(static_cast<size_t>(wideLength), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &wide[0], wideLength);
		return wide;
	}

	bool GetString(const Json& object, const char* key, std::wstring& outValue)
	{
		const auto field = object.find(key);
		if (field == object.end() || !field->is_string())
		{
			return false;
		}

		outValue = Utf8ToWide(field->get_ref<const std::string&>());
		return true;
	}

	bool GetBool(const Json& object, const char* key, bool& outValue)
	{
		const auto field = object.find(key);
		if (field == object.end() || !field->is_boolean())
		{
			return false;
		}

		outValue = field->get<bool>();
		return true;
	}

	bool GetUInt(const Json& object, const char* key, UINT& outValue)
	{
		const auto field = object.find(key);
		if (field == object.end() || !field->is_number_unsigned())
		{
			return false;
		}

		const std::uint64_t value = field->get<std::uint64_t>();
		if (value > UINT_MAX)
		{
			return false;
		}

		outValue = static_cast<UINT>(value);
		return true;
	}

	void GetStringArray(const Json& object, const char* key, std::vector<std::wstring>& outValues)
	{
		const auto field = object.find(key);
		if (field == object.end() || !field->is_array())
		{
			return;
		}

		std::vector<std::wstring> values;
		values.reserve(field->size());
		for (const auto& element : *field)
		{
			if (!element.is_string())
			{
				return;
			}
			values.push_back(Utf8ToWide(element.get_ref<const std::string&>()));
		}

		outValues.insert(outValues.end(), values.begin(), values.end());
	}

	MenuActionType ParseActionType(const std::wstring& actionType)
	{
		if (actionType == L"launch")
		{
			return MenuActionType::Launch;
		}
		return MenuActionType::ShowMessage;
	}

	bool ParseMenuItemObject(const Json& object, MenuItemDef& item)
	{
		if (!object.is_object())
		{
			return false;
		}

		if (!GetString(object, "id", item.id) || !GetString(object, "label", item.label) || !GetString(object, "verb", item.verb))
		{
			return false;
		}

		GetString(object, "helpText", item.helpText);
		GetString(object, "canonicalName", item.canonicalName);
		GetString(object, "icon", item.icon);
		GetBool(object, "separatorAfter", item.separatorAfter);

		GetStringArray(object, "extensions", item.filter.extensions);
		GetStringArray(object, "excludeExtensions", item.filter.excludeExtensions);
		GetUInt(object, "minSelection", item.filter.minSelection);
		GetUInt(object, "maxSelection", item.filter.maxSelection);
		GetBool(object, "filesOnly", item.filter.filesOnly);
		GetBool(object, "foldersOnly", item.filter.foldersOnly);

		std::vector<std::wstring> targetNames;
		GetStringArray(object, "targets", targetNames);
		for (const auto& targetName : targetNames)
		{
			ShellTargetType targetType;
			if (!ParseShellTargetType(targetName, targetType))
			{
				return false;
			}
			item.targets.push_back(targetType);
		}

		GetStringArray(object, "extensionGates", item.extensionGates);
		GetStringArray(object, "itemGates", item.itemGates);
		if (item.itemGates.empty())
		{
			GetStringArray(object, "gates", item.itemGates);
		}
		GetStringArray(object, "presentationGates", item.presentationGates);
		GetStringArray(object, "executors", item.executors);

		std::wstring actionType = L"messageBox";
		GetString(object, "actionType", actionType);
		std::wstring actionTitle;
		std::wstring actionTemplate;
		std::wstring actionCommand;
		bool actionShowWindow = false;
		GetString(object, "actionTitle", actionTitle);
		GetString(object, "actionTemplate", actionTemplate);
		GetString(object, "actionCommand", actionCommand);
		GetBool(object, "actionShowWindow", actionShowWindow);

		item.action.type = ParseActionType(actionType);
		item.action.title = actionTitle;
		item.action.templateText = actionTemplate;
		item.action.command = actionCommand;
		item.action.showWindow = actionShowWindow;

		if (item.helpText.empty())
		{
			item.helpText = item.label;
		}
		if (item.canonicalName.empty())
		{
			item.canonicalName = item.verb;
		}

		return true;
	}

	void ParseRootChains(const Json& root, MenuGateChains& chains)
	{
		GetStringArray(root, "extensionGates", chains.extensionGates);
		GetStringArray(root, "itemGates", chains.itemGates);
		if (chains.itemGates.empty())
		{
			GetStringArray(root, "gates", chains.itemGates);
		}
		GetStringArray(root, "presentationGates", chains.presentationGates);
		GetStringArray(root, "executors", chains.executors);
	}
} // namespace

std::vector<MenuItemDef> GetBuiltinMenuItems()
{
	MenuItemDef item;
	item.id = L"display-file-name";
	item.label = L_Menu_Text;
	item.verb = L_Verb_Name;
	item.helpText = L_Verb_Help_Text;
	item.canonicalName = L_Verb_Canonical_Name;
	item.separatorAfter = true;
	item.targets = {ShellTargetType::File};
	item.filter.extensions = {L_Associated_Type};
	item.filter.minSelection = 1;
	item.filter.filesOnly = true;
	item.action.type = MenuActionType::ShowMessage;
	item.action.title = L_Friendly_Menu_Name;
	item.action.templateText = L"The selected file is:\r\n\r\n%1";

	return {item};
}

bool LoadMenuConfigDocument(const std::wstring& configPath, MenuConfigDocument& document)
{
	static std::wstring cachedConfigPath;
	static MenuConfigDocument cachedDocument;
	static bool hasCachedDocument = false;
	static bool cachedLoadSucceeded = false;

	if (hasCachedDocument && cachedConfigPath == configPath)
	{
		document = cachedDocument;
		return cachedLoadSucceeded;
	}

	document = {};

	std::string jsonText;
	if (!ReadUtf8File(configPath, jsonText))
	{
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
		ShellLog(L"Config not found, using built-in menu: %s", configPath.c_str());
		cachedDocument = document;
		cachedConfigPath = configPath;
		hasCachedDocument = true;
		cachedLoadSucceeded = false;
		return false;
	}

	const Json root = Json::parse(jsonText, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
		ShellLog(L"Failed to parse config, using built-in menu: %s", configPath.c_str());
		cachedDocument = document;
		cachedConfigPath = configPath;
		hasCachedDocument = true;
		cachedLoadSucceeded = false;
		return false;
	}

	// Config-specified chains fully replace the defaults; defaults only fill in
	// chains that the config leaves unspecified (applied below).
	ParseRootChains(root, document.globalChains);

	std::vector<MenuItemDef> items;
	const auto menuItems = root.find("menuItems");
	if (menuItems != root.end() && menuItems->is_array())
	{
		for (const auto& element : *menuItems)
		{
			MenuItemDef item;
			if (ParseMenuItemObject(element, item))
			{
				items.push_back(item);
			}
		}
	}

	if (items.empty())
	{
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
		ShellLog(L"Failed to parse config, using built-in menu: %s", configPath.c_str());
		cachedDocument = document;
		cachedConfigPath = configPath;
		hasCachedDocument = true;
		cachedLoadSucceeded = false;
		return false;
	}

	document.items = items;
	ApplyDefaultMenuGateChains(document.globalChains);

	ShellLog(L"Loaded %u menu item(s) from %s", static_cast<UINT>(document.items.size()), configPath.c_str());
	cachedDocument = document;
	cachedConfigPath = configPath;
	hasCachedDocument = true;
	cachedLoadSucceeded = true;
	return true;
}

bool LoadMenuConfig(const std::wstring& configPath, std::vector<MenuItemDef>& items)
{
	MenuConfigDocument document;
	const bool loaded = LoadMenuConfigDocument(configPath, document);
	items = document.items;
	return loaded;
}
