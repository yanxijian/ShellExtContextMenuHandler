#include "MenuConfig.h"
#include "common.h"
#include "ShellLog.h"
#include <sstream>
#include <vector>
#include <windows.h>

namespace
{
    bool ReadUtf8File(const std::wstring& path, std::wstring& content)
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
        if (fileSize > 0
            && fread(bytes.data(), 1, static_cast<size_t>(fileSize), file) != static_cast<size_t>(fileSize))
        {
            fclose(file);
            return false;
        }

        fclose(file);

        if (bytes.empty())
        {
            content.clear();
            return true;
        }

        const int wideLength = MultiByteToWideChar(
            CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (wideLength <= 0)
        {
            return false;
        }

        std::vector<wchar_t> wideBuffer(static_cast<size_t>(wideLength));
        MultiByteToWideChar(
            CP_UTF8, 0, bytes.data(), static_cast<int>(bytes.size()), wideBuffer.data(), wideLength);
        content.assign(wideBuffer.begin(), wideBuffer.end());
        return true;
    }

    void Trim(std::wstring& value)
    {
        const wchar_t* whitespace = L" \t\r\n";
        const size_t start = value.find_first_not_of(whitespace);
        if (start == std::wstring::npos)
        {
            value.clear();
            return;
        }

        const size_t end = value.find_last_not_of(whitespace);
        value = value.substr(start, end - start + 1);
    }

    std::wstring Unquote(const std::wstring& value)
    {
        if (value.size() >= 2
            && ((value.front() == L'"' && value.back() == L'"')
                || (value.front() == L'\'' && value.back() == L'\'')))
        {
            return value.substr(1, value.size() - 2);
        }
        return value;
    }

    bool ExtractStringValue(const std::wstring& objectBody, PCWSTR key, std::wstring& outValue)
    {
        const std::wstring pattern = std::wstring(L"\"") + key + L"\"";
        const size_t keyPos = objectBody.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return false;
        }

        const size_t colonPos = objectBody.find(L':', keyPos + pattern.size());
        if (colonPos == std::wstring::npos)
        {
            return false;
        }

        const size_t startQuote = objectBody.find(L'"', colonPos + 1);
        if (startQuote == std::wstring::npos)
        {
            return false;
        }

        const size_t endQuote = objectBody.find(L'"', startQuote + 1);
        if (endQuote == std::wstring::npos)
        {
            return false;
        }

        outValue = objectBody.substr(startQuote + 1, endQuote - startQuote - 1);
        return true;
    }

    bool ExtractBoolValue(const std::wstring& objectBody, PCWSTR key, bool& outValue)
    {
        const std::wstring pattern = std::wstring(L"\"") + key + L"\"";
        const size_t keyPos = objectBody.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return false;
        }

        const size_t colonPos = objectBody.find(L':', keyPos + pattern.size());
        if (colonPos == std::wstring::npos)
        {
            return false;
        }

        std::wstring raw = objectBody.substr(colonPos + 1);
        const size_t commaPos = raw.find(L',');
        if (commaPos != std::wstring::npos)
        {
            raw = raw.substr(0, commaPos);
        }
        Trim(raw);

        if (raw == L"true")
        {
            outValue = true;
            return true;
        }
        if (raw == L"false")
        {
            outValue = false;
            return true;
        }

        return false;
    }

    bool ExtractUIntValue(const std::wstring& objectBody, PCWSTR key, UINT& outValue)
    {
        const std::wstring pattern = std::wstring(L"\"") + key + L"\"";
        const size_t keyPos = objectBody.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return false;
        }

        const size_t colonPos = objectBody.find(L':', keyPos + pattern.size());
        if (colonPos == std::wstring::npos)
        {
            return false;
        }

        std::wstring raw = objectBody.substr(colonPos + 1);
        const size_t commaPos = raw.find(L',');
        if (commaPos != std::wstring::npos)
        {
            raw = raw.substr(0, commaPos);
        }
        Trim(raw);
        outValue = static_cast<UINT>(_wtoi(raw.c_str()));
        return true;
    }

    bool ExtractStringArray(const std::wstring& objectBody, PCWSTR key, std::vector<std::wstring>& outValues)
    {
        const std::wstring pattern = std::wstring(L"\"") + key + L"\"";
        const size_t keyPos = objectBody.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return false;
        }

        const size_t colonPos = objectBody.find(L':', keyPos + pattern.size());
        const size_t arrayStart = objectBody.find(L'[', colonPos);
        const size_t arrayEnd = objectBody.find(L']', arrayStart);
        if (colonPos == std::wstring::npos || arrayStart == std::wstring::npos || arrayEnd == std::wstring::npos)
        {
            return false;
        }

        std::wstring arrayBody = objectBody.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        size_t position = 0;
        while (position < arrayBody.size())
        {
            const size_t startQuote = arrayBody.find(L'"', position);
            if (startQuote == std::wstring::npos)
            {
                break;
            }

            const size_t endQuote = arrayBody.find(L'"', startQuote + 1);
            if (endQuote == std::wstring::npos)
            {
                break;
            }

            outValues.push_back(arrayBody.substr(startQuote + 1, endQuote - startQuote - 1));
            position = endQuote + 1;
        }

        return true;
    }

    MenuActionType ParseActionType(const std::wstring& actionType)
    {
        if (actionType == L"launch")
        {
            return MenuActionType::Launch;
        }
        return MenuActionType::ShowMessage;
    }

    bool ParseMenuItemObject(const std::wstring& objectBody, MenuItemDef& item)
    {
        std::wstring actionType = L"messageBox";
        std::wstring actionTitle;
        std::wstring actionTemplate;
        std::wstring actionCommand;
        bool actionShowWindow = false;

        if (!ExtractStringValue(objectBody, L"id", item.id)
            || !ExtractStringValue(objectBody, L"label", item.label)
            || !ExtractStringValue(objectBody, L"verb", item.verb))
        {
            return false;
        }

        ExtractStringValue(objectBody, L"helpText", item.helpText);
        ExtractStringValue(objectBody, L"canonicalName", item.canonicalName);
        ExtractBoolValue(objectBody, L"separatorAfter", item.separatorAfter);

        ExtractStringArray(objectBody, L"extensions", item.filter.extensions);
        ExtractStringArray(objectBody, L"excludeExtensions", item.filter.excludeExtensions);
        ExtractUIntValue(objectBody, L"minSelection", item.filter.minSelection);
        ExtractUIntValue(objectBody, L"maxSelection", item.filter.maxSelection);
        ExtractBoolValue(objectBody, L"filesOnly", item.filter.filesOnly);
        ExtractBoolValue(objectBody, L"foldersOnly", item.filter.foldersOnly);

        ExtractStringValue(objectBody, L"actionType", actionType);
        ExtractStringValue(objectBody, L"actionTitle", actionTitle);
        ExtractStringValue(objectBody, L"actionTemplate", actionTemplate);
        ExtractStringValue(objectBody, L"actionCommand", actionCommand);
        ExtractBoolValue(objectBody, L"actionShowWindow", actionShowWindow);

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

    bool ParseMenuItemsArray(const std::wstring& json, std::vector<MenuItemDef>& items)
    {
        const size_t arrayKey = json.find(L"\"menuItems\"");
        if (arrayKey == std::wstring::npos)
        {
            return false;
        }

        const size_t arrayStart = json.find(L'[', arrayKey);
        const size_t arrayEnd = json.rfind(L']');
        if (arrayStart == std::wstring::npos || arrayEnd == std::wstring::npos || arrayEnd <= arrayStart)
        {
            return false;
        }

        const std::wstring arrayBody = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        size_t position = 0;
        while (position < arrayBody.size())
        {
            const size_t objectStart = arrayBody.find(L'{', position);
            if (objectStart == std::wstring::npos)
            {
                break;
            }

            int depth = 0;
            size_t objectEnd = std::wstring::npos;
            for (size_t i = objectStart; i < arrayBody.size(); ++i)
            {
                if (arrayBody[i] == L'{')
                {
                    ++depth;
                }
                else if (arrayBody[i] == L'}')
                {
                    --depth;
                    if (depth == 0)
                    {
                        objectEnd = i;
                        break;
                    }
                }
            }

            if (objectEnd == std::wstring::npos)
            {
                break;
            }

            MenuItemDef item;
            const std::wstring objectBody = arrayBody.substr(objectStart + 1, objectEnd - objectStart - 1);
            if (ParseMenuItemObject(objectBody, item))
            {
                items.push_back(item);
            }

            position = objectEnd + 1;
        }

        return !items.empty();
    }
}

std::vector<MenuItemDef> GetBuiltinMenuItems()
{
    MenuItemDef item;
    item.id = L"display-file-name";
    item.label = L_Menu_Text;
    item.verb = L_Verb_Name;
    item.helpText = L_Verb_Help_Text;
    item.canonicalName = L_Verb_Canonical_Name;
    item.separatorAfter = true;
    item.filter.extensions = { L_Associated_Type };
    item.filter.minSelection = 1;
    item.filter.filesOnly = true;
    item.action.type = MenuActionType::ShowMessage;
    item.action.title = L_Friendly_Menu_Name;
    item.action.templateText = L"The selected file is:\r\n\r\n%1";

    return { item };
}

bool LoadMenuConfig(const std::wstring& configPath, std::vector<MenuItemDef>& items)
{
    std::wstring json;
    if (!ReadUtf8File(configPath, json))
    {
        ShellLog(L"Config not found, using built-in menu: %s", configPath.c_str());
        items = GetBuiltinMenuItems();
        return false;
    }

    if (!ParseMenuItemsArray(json, items))
    {
        ShellLog(L"Failed to parse config, using built-in menu: %s", configPath.c_str());
        items = GetBuiltinMenuItems();
        return false;
    }

    ShellLog(L"Loaded %u menu item(s) from %s", static_cast<UINT>(items.size()), configPath.c_str());
    return true;
}
