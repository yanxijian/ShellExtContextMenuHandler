#include "BitmapFileIconProvider.h"
#include "DpiProvider.h"
#include "PathHelpers.h"
#include "ShellLog.h"

bool BitmapFileIconProvider::CanProvide(const std::wstring& iconSpec) const
{
    return HasExtensionIgnoreCase(iconSpec, L".bmp");
}

HBITMAP BitmapFileIconProvider::LoadIcon(
    const std::wstring& iconSpec,
    UINT dpi,
    HINSTANCE module)
{
    const std::wstring path = ResolveModuleRelativePath(module, iconSpec);
    const int iconSize = DpiProvider::ScaleForDpi(DpiProvider::kBaseMenuIconSize, dpi);
    HBITMAP bitmap = static_cast<HBITMAP>(LoadImageW(
        nullptr,
        path.c_str(),
        IMAGE_BITMAP,
        iconSize,
        iconSize,
        LR_LOADFROMFILE | LR_LOADTRANSPARENT));
    if (bitmap == nullptr)
    {
        ShellLog(L"BitmapFileIconProvider: failed to load %s", path.c_str());
    }

    return bitmap;
}