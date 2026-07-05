#pragma once

#include <windows.h>
#include "MenuContext.h"
#include "MenuItem.h"

void ExecuteMenuAction(const MenuAction& action, const MenuContext& context, HWND hwnd);
