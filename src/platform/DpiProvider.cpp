#include "DpiProvider.h"
#include "OsVersion.h"
#include "ShellLog.h"

namespace
{
    enum MonitorDpiType
    {
        MdtEffectiveDpi = 0,
        MdtAngularDpi = 1,
        MdtRawDpi = 2,
        MdtDefault = MdtEffectiveDpi
    };

    using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, MonitorDpiType, UINT*, UINT*);

    GetDpiForMonitorFn ResolveGetDpiForMonitor()
    {
        static GetDpiForMonitorFn cached = nullptr;
        static bool resolved = false;
        if (resolved)
        {
            return cached;
        }

        resolved = true;
        if (!IsWindows81OrGreater())
        {
            return nullptr;
        }

        HMODULE shcore = LoadLibraryW(L"Shcore.dll");
        if (shcore == nullptr)
        {
            ShellLog(L"DpiProvider: Shcore.dll not available, using system DPI.");
            return nullptr;
        }

        cached = reinterpret_cast<GetDpiForMonitorFn>(
            GetProcAddress(shcore, "GetDpiForMonitor"));
        if (cached == nullptr)
        {
            ShellLog(L"DpiProvider: GetDpiForMonitor not found, using system DPI.");
        }

        return cached;
    }

    UINT ReadSystemDpi()
    {
        HDC screenDc = GetDC(nullptr);
        if (screenDc == nullptr)
        {
            return DpiProvider::kDefaultDpi;
        }

        const UINT dpi = static_cast<UINT>(GetDeviceCaps(screenDc, LOGPIXELSX));
        ReleaseDC(nullptr, screenDc);
        return dpi == 0 ? DpiProvider::kDefaultDpi : dpi;
    }

    HBITMAP CopyBitmapHandle(HBITMAP source)
    {
        if (source == nullptr)
        {
            return nullptr;
        }

        BITMAP bitmap = {};
        if (GetObject(source, sizeof(bitmap), &bitmap) == 0)
        {
            return static_cast<HBITMAP>(CopyImage(
                source, IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG));
        }

        return static_cast<HBITMAP>(CopyImage(
            source,
            IMAGE_BITMAP,
            bitmap.bmWidth,
            bitmap.bmHeight,
            LR_COPYRETURNORG));
    }
}

namespace DpiProvider
{
    UINT GetSystemDpi()
    {
        static UINT cachedDpi = 0;
        if (cachedDpi == 0)
        {
            cachedDpi = ReadSystemDpi();
            ShellLog(L"DpiProvider: system DPI = %u", cachedDpi);
        }

        return cachedDpi;
    }

    UINT GetDpiForMonitor(HMONITOR monitor)
    {
        if (monitor == nullptr)
        {
            return GetSystemDpi();
        }

        GetDpiForMonitorFn getDpiForMonitor = ResolveGetDpiForMonitor();
        if (getDpiForMonitor != nullptr)
        {
            UINT dpiX = kDefaultDpi;
            UINT dpiY = kDefaultDpi;
            if (SUCCEEDED(getDpiForMonitor(monitor, MdtEffectiveDpi, &dpiX, &dpiY))
                && dpiX != 0)
            {
                return dpiX;
            }
        }

        MONITORINFOEXW monitorInfo = { sizeof(monitorInfo) };
        if (GetMonitorInfoW(monitor, &monitorInfo))
        {
            HDC monitorDc = CreateDCW(L"DISPLAY", monitorInfo.szDevice, nullptr, nullptr);
            if (monitorDc != nullptr)
            {
                const UINT dpi = static_cast<UINT>(GetDeviceCaps(monitorDc, LOGPIXELSX));
                DeleteDC(monitorDc);
                if (dpi != 0)
                {
                    return dpi;
                }
            }
        }

        return GetSystemDpi();
    }

    UINT GetDpiAtCursor()
    {
        POINT cursor = {};
        if (!GetCursorPos(&cursor))
        {
            return GetSystemDpi();
        }

        HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        return GetDpiForMonitor(monitor);
    }

    UINT GetDpiForWindow(HWND hwnd)
    {
        if (hwnd == nullptr)
        {
            return GetDpiAtCursor();
        }

        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        return GetDpiForMonitor(monitor);
    }

    int ScaleForDpi(int logicalPixels, UINT dpi)
    {
        if (dpi == 0)
        {
            dpi = kDefaultDpi;
        }

        return MulDiv(logicalPixels, static_cast<int>(dpi), static_cast<int>(kDefaultDpi));
    }

    HBITMAP LoadMenuBitmapScaled(HINSTANCE module, PCWSTR resourceName, UINT dpi)
    {
        if (module == nullptr || resourceName == nullptr)
        {
            return nullptr;
        }

        const int iconSize = ScaleForDpi(kBaseMenuIconSize, dpi);
        HBITMAP loaded = static_cast<HBITMAP>(LoadImageW(
            module,
            resourceName,
            IMAGE_BITMAP,
            iconSize,
            iconSize,
            LR_DEFAULTCOLOR | LR_LOADTRANSPARENT));
        if (loaded != nullptr)
        {
            return loaded;
        }

        HBITMAP fallback = static_cast<HBITMAP>(LoadImageW(
            module,
            resourceName,
            IMAGE_BITMAP,
            0,
            0,
            LR_DEFAULTSIZE | LR_LOADTRANSPARENT));
        HBITMAP copied = CopyBitmapHandle(fallback);
        if (fallback != nullptr)
        {
            DeleteObject(fallback);
        }
        return copied;
    }
}