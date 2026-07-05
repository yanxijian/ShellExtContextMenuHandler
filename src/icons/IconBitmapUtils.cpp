#include "IconBitmapUtils.h"

HBITMAP CreateBitmapFromRgba(int width, int height, const unsigned char* rgba)
{
    if (width <= 0 || height <= 0 || rgba == nullptr)
    {
        return nullptr;
    }

    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr)
    {
        return nullptr;
    }

    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDc);
    if (bitmap == nullptr || bits == nullptr)
    {
        return nullptr;
    }

    auto* dest = static_cast<unsigned char*>(bits);
    const int pixelCount = width * height;
    for (int i = 0; i < pixelCount; ++i)
    {
        const int src = i * 4;
        dest[src + 0] = rgba[src + 2];
        dest[src + 1] = rgba[src + 1];
        dest[src + 2] = rgba[src + 0];
        dest[src + 3] = rgba[src + 3];
    }

    return bitmap;
}