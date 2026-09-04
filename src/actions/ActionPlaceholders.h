#pragma once
#include "MenuContext.h"

#include <string>

std::wstring ExpandActionPlaceholders(const std::wstring& text, const MenuContext& context);