#pragma once
#include "MenuContext.h"
#include "MenuItem.h"

#include <windows.h>

void ExecuteMenuAction(const MenuAction& action, const MenuContext& context, HWND hwnd);