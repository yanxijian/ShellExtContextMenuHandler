#pragma once

#include "MenuContext.h"
#include <shlobj.h>

bool BuildMenuContext(
    LPCITEMIDLIST pidlFolder,
    LPDATAOBJECT dataObject,
    HKEY hKeyProgID,
    MenuContext& context);
