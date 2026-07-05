#pragma once

#include "GateTypes.h"
#include "MenuContext.h"
#include "MenuItem.h"

struct IMenuItemPresentationGate
{
    virtual ~IMenuItemPresentationGate() = default;
    virtual MenuItemState Evaluate(const MenuContext& context, const MenuItemDef& item) = 0;
};
