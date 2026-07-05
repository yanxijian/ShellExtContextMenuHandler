#pragma once

#include "MenuGateChains.h"
#include "MenuItem.h"
#include <vector>

struct MenuConfigDocument
{
    MenuGateChains globalChains;
    std::vector<MenuItemDef> items;
};

bool LoadMenuConfigDocument(const std::wstring& configPath, MenuConfigDocument& document);
bool LoadMenuConfig(const std::wstring& configPath, std::vector<MenuItemDef>& items);
std::vector<MenuItemDef> GetBuiltinMenuItems();
