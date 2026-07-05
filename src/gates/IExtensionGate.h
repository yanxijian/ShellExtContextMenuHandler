#pragma once

#include "MenuContext.h"

struct IExtensionGate
{
    virtual ~IExtensionGate() = default;
    virtual bool ShouldActivate(const MenuContext& context) = 0;
};
