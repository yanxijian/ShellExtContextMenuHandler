#pragma once

#include "IActionExecutor.h"

class DemoActionLogExecutor : public IActionExecutor
{
public:
    bool CanExecute(const MenuAction& action) const override;
    bool Execute(const MenuContext& context, const MenuAction& action, HWND hwnd) override;
};
