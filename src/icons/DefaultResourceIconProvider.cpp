#include "DefaultResourceIconProvider.h"
#include "DpiProvider.h"
#include "resource.h"

bool DefaultResourceIconProvider::CanProvide(const std::wstring& iconSpec) const
{
    return iconSpec.empty();
}

HBITMAP DefaultResourceIconProvider::LoadIcon(
    const std::wstring& iconSpec,
    UINT dpi,
    HINSTANCE module)
{
    UNREFERENCED_PARAMETER(iconSpec);
    return DpiProvider::LoadMenuBitmapScaled(
        module,
        MAKEINTRESOURCEW(IDB_OK),
        dpi);
}