#pragma once

#include "IMenuItemPresentationGate.h"

class DemoReadOnlyPresentationGate : public IMenuItemPresentationGate
{
public:
    MenuItemState Evaluate(const MenuContext& context, const MenuItemDef& item) override;
};
