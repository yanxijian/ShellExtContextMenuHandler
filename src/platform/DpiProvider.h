#pragma once

#include <windows.h>

namespace DpiProvider
{
    constexpr UINT kDefaultDpi = 96;
    constexpr int kBaseMenuIconSize = 16;

    UINT GetSystemDpi();
    UINT GetDpiForMonitor(HMONITOR monitor);
    UINT GetDpiAtCursor();
    UINT GetDpiForWindow(HWND hwnd);

    int ScaleForDpi(int logicalPixels, UINT dpi);
    HBITMAP LoadMenuBitmapScaled(HINSTANCE module, PCWSTR resourceName, UINT dpi);
}