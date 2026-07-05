#pragma once

#include "IMenuItemGate.h"

class DemoHideTempItemGate : public IMenuItemGate
{
public:
    bool ShouldShow(const MenuContext& context, const MenuItemDef& item) override;
};
