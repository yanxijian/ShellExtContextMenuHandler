#pragma once

#include <string>
#include "MenuContext.h"

std::wstring ExpandActionPlaceholders(const std::wstring& text, const MenuContext& context);