#include "DemoReadOnlyPresentationGate.h"

MenuItemState DemoReadOnlyPresentationGate::Evaluate(
    const MenuContext& context,
    const MenuItemDef& item)
{
    UNREFERENCED_PARAMETER(item);

    for (const auto& selected : context.selected)
    {
        if (!selected.isDirectory
            && selected.attributes != INVALID_FILE_ATTRIBUTES
            && (selected.attributes & FILE_ATTRIBUTE_READONLY) != 0)
        {
            return MenuItemState::Disabled;
        }
    }

    return MenuItemState::Enabled;
}
