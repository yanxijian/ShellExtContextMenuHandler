#pragma once

#include "IMenuItemGate.h"

class JsonFilterGate : public IMenuItemGate
{
public:
    bool ShouldShow(const MenuContext& context, const MenuItemDef& item) override;
};
