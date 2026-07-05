#pragma once

#include "GateTypes.h"
#include "MenuItem.h"

struct InsertedMenuItem
{
    MenuItemDef item;
    MenuItemState state = MenuItemState::Enabled;
};
