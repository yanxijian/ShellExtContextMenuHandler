#include "DemoHideTempItemGate.h"
#include <cwctype>

bool DemoHideTempItemGate::ShouldShow(const MenuContext& context, const MenuItemDef& item)
{
    UNREFERENCED_PARAMETER(item);

    for (const auto& selected : context.selected)
    {
        std::wstring path = selected.path;
        for (auto& ch : path)
        {
            ch = static_cast<wchar_t>(towlower(ch));
        }

        if (path.find(L"\\temp\\") != std::wstring::npos)
        {
            return false;
        }
    }

    return true;
}
