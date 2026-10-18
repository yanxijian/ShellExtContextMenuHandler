#include "MenuConfig.h"

#include "MenuGateChains.h"
#include "ShellLog.h"
#include "common.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <climits>
#include <cstring>
#include <mutex>
#include <set>
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

	std::wstring FieldName(const char* key)
	{
		return std::wstring(key, key + std::strlen(key));
	}

	bool ValidateOptionalString(const Json& object, const char* key, std::wstring& error, bool required = false)
	{
		const auto field = object.find(key);
		if (field == object.end())
		{
			if (required)
			{
				error = L"missing required field '" + FieldName(key) + L"'";
				return false;
			}
			return true;
		}

		if (!field->is_string())
		{
			error = L"field '" + FieldName(key) + L"' must be a string";
			return false;
		}

		if (required && field->get_ref<const std::string&>().empty())
		{
			error = L"field '" + FieldName(key) + L"' must not be empty";
			return false;
		}

		return true;
	}

	bool ValidateOptionalBool(const Json& object, const char* key, std::wstring& error)
	{
		const auto field = object.find(key);
		if (field == object.end())
		{
			return true;
		}

		if (!field->is_boolean())
		{
			error = L"field '" + FieldName(key) + L"' must be a boolean";
			return false;
		}

		return true;
	}

	bool ValidateOptionalUInt(const Json& object, const char* key, std::wstring& error)
	{
		const auto field = object.find(key);
		if (field == object.end())
		{
			return true;
		}

		if (!field->is_number_unsigned() || field->get<std::uint64_t>() > UINT_MAX)
		{
			error = L"field '" + FieldName(key) + L"' must be an unsigned 32-bit integer";
			return false;
		}

		return true;
	}

	bool ValidateOptionalStringArray(const Json& object, const char* key, std::wstring& error)
	{
		const auto field = object.find(key);
		if (field == object.end())
		{
			return true;
		}

		if (!field->is_array())
		{
			error = L"field '" + FieldName(key) + L"' must be an array of strings";
			return false;
		}

		for (const auto& element : *field)
		{
			if (!element.is_string() || element.get_ref<const std::string&>().empty())
			{
				error = L"field '" + FieldName(key) + L"' contains an empty or non-string element";
				return false;
			}
		}

		return true;
	}

	bool ValidateTargets(const Json& item, std::wstring& error)
	{
		const auto field = item.find("targets");
		if (field == item.end())
		{
			return true;
		}

		if (!field->is_array())
		{
			error = L"field 'targets' must be an array of shell target names";
			return false;
		}

		for (const auto& element : *field)
		{
			if (!element.is_string() || element.get_ref<const std::string&>().empty())
			{
				error = L"field 'targets' contains an empty or non-string element";
				return false;
			}

			const std::wstring targetName = Utf8ToWide(element.get_ref<const std::string&>());
			ShellTargetType targetType;
			if (!ParseShellTargetType(targetName, targetType))
			{
				error = L"field 'targets' contains an unknown target: " + targetName;
				return false;
			}
		}

		return true;
	}

	bool ValidateMenuItem(const Json& item, size_t index, std::set<std::string>& ids, std::wstring& error)
	{
		if (!item.is_object())
		{
			error = L"menuItems[" + std::to_wstring(index) + L"] must be an object";
			return false;
		}

		if (!ValidateOptionalString(item, "id", error, true) || !ValidateOptionalString(item, "label", error, true)
			|| !ValidateOptionalString(item, "verb", error, true))
		{
			return false;
		}

		const std::string id = item["id"].get<std::string>();
		if (!ids.insert(id).second)
		{
			error = L"duplicate menu item id: " + Utf8ToWide(id);
			return false;
		}

		if (!ValidateOptionalString(item, "helpText", error) || !ValidateOptionalString(item, "canonicalName", error)
			|| !ValidateOptionalString(item, "icon", error) || !ValidateOptionalString(item, "actionTitle", error)
			|| !ValidateOptionalString(item, "actionTemplate", error) || !ValidateOptionalString(item, "actionCommand", error)
			|| !ValidateOptionalBool(item, "separatorAfter", error) || !ValidateOptionalBool(item, "filesOnly", error)
			|| !ValidateOptionalBool(item, "foldersOnly", error) || !ValidateOptionalBool(item, "actionShowWindow", error)
			|| !ValidateOptionalUInt(item, "minSelection", error) || !ValidateOptionalUInt(item, "maxSelection", error)
			|| !ValidateOptionalStringArray(item, "extensions", error) || !ValidateOptionalStringArray(item, "excludeExtensions", error)
			|| !ValidateOptionalStringArray(item, "extensionGates", error) || !ValidateOptionalStringArray(item, "itemGates", error)
			|| !ValidateOptionalStringArray(item, "gates", error) || !ValidateOptionalStringArray(item, "presentationGates", error)
			|| !ValidateOptionalStringArray(item, "executors", error) || !ValidateTargets(item, error))
		{
			return false;
		}

		const auto actionTypeField = item.find("actionType");
		if (actionTypeField != item.end() && !actionTypeField->is_string())
		{
			error = L"field 'actionType' must be a string";
			return false;
		}

		const std::wstring actionType =
			actionTypeField == item.end() ? L"messageBox" : Utf8ToWide(actionTypeField->get_ref<const std::string&>());
		if (actionType != L"messageBox" && actionType != L"launch")
		{
			error = L"field 'actionType' must be 'messageBox' or 'launch'";
			return false;
		}

		if (actionType == L"launch")
		{
			const auto command = item.find("actionCommand");
			if (command == item.end() || !command->is_string() || command->get_ref<const std::string&>().empty())
			{
				error = L"field 'actionCommand' is required for launch items";
				return false;
			}
		}

		return true;
	}

	bool ValidateMenuDocument(const Json& root, std::wstring& error)
	{
		if (!root.is_object())
		{
			error = L"menu.json root must be an object";
			return false;
		}

		const auto schemaVersion = root.find("schemaVersion");
		if (schemaVersion != root.end())
		{
			if (!schemaVersion->is_number_unsigned() || schemaVersion->get<std::uint64_t>() != 1)
			{
				error = L"unsupported menu.json schemaVersion";
				return false;
			}
		}

		for (const char* key : {"extensionGates", "itemGates", "presentationGates", "executors", "gates"})
		{
			if (!ValidateOptionalStringArray(root, key, error))
			{
				return false;
			}
		}

		const auto items = root.find("menuItems");
		if (items == root.end() || !items->is_array() || items->empty())
		{
			error = L"field 'menuItems' must be a non-empty array";
			return false;
		}

		std::set<std::string> ids;
		for (size_t index = 0; index < items->size(); ++index)
		{
			if (!ValidateMenuItem((*items)[index], index, ids, error))
			{
				return false;
			}
		}

		return true;
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
	static std::mutex cacheMutex;

	std::lock_guard<std::mutex> cacheLock(cacheMutex);

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

	std::wstring validationError;
	if (!ValidateMenuDocument(root, validationError))
	{
		document.items = GetBuiltinMenuItems();
		ApplyDefaultMenuGateChains(document.globalChains);
		ShellLog(L"Invalid menu config (%s): %s", configPath.c_str(), validationError.c_str());
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
