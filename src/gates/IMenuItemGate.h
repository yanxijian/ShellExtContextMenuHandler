#pragma once

#include "MenuContext.h"
#include "MenuItem.h"

struct IMenuItemGate
{
    virtual ~IMenuItemGate() = default;
    virtual bool ShouldShow(const MenuContext& context, const MenuItemDef& item) = 0;
};
