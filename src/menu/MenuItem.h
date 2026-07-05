#pragma once

#include <windows.h>
#include <string>
#include <vector>

enum class MenuActionType
{
    ShowMessage,
    Launch
};

struct MenuFilter
{
    std::vector<std::wstring> extensions;
    std::vector<std::wstring> excludeExtensions;
    UINT minSelection = 1;
    UINT maxSelection = 0;
    bool filesOnly = true;
    bool foldersOnly = false;
};

struct MenuAction
{
    MenuActionType type = MenuActionType::ShowMessage;
    std::wstring title;
    std::wstring templateText;
    std::wstring command;
    bool showWindow = false;
};

struct MenuItemDef
{
    std::wstring id;
    std::wstring label;
    std::wstring verb;
    std::wstring helpText;
    std::wstring canonicalName;
    std::wstring icon;
    bool separatorAfter = false;
    MenuFilter filter;
    MenuAction action;

    std::vector<std::wstring> extensionGates;
    std::vector<std::wstring> itemGates;
    std::vector<std::wstring> presentationGates;
    std::vector<std::wstring> executors;
};
