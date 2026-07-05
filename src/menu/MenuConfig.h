#pragma once

#include "MenuItem.h"
#include <vector>

bool LoadMenuConfig(const std::wstring& configPath, std::vector<MenuItemDef>& items);
std::vector<MenuItemDef> GetBuiltinMenuItems();
