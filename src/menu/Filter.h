#pragma once

#include "MenuContext.h"
#include "MenuItem.h"

bool PathMatchesExtension(const std::wstring& path, const std::wstring& extension);
bool PathMatchesAnyExtension(const std::wstring& path, const std::vector<std::wstring>& extensions);
bool MenuItemMatchesContext(const MenuItemDef& item, const MenuContext& context);
