#pragma once
#include "MenuContext.h"
#include "MenuItem.h"

#include <windows.h>

class IActionExecutor
{
public:
	virtual ~IActionExecutor() = default;

	virtual bool CanExecute(const MenuAction& action) const = 0;
	virtual bool Execute(const MenuContext& context, const MenuAction& action, HWND hwnd) = 0;
};