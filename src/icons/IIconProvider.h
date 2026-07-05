#pragma once

#include <windows.h>
#include <string>

class IIconProvider
{
public:
    virtual ~IIconProvider() = default;

    virtual bool CanProvide(const std::wstring& iconSpec) const = 0;
    virtual HBITMAP LoadIcon(const std::wstring& iconSpec, UINT dpi, HINSTANCE module) = 0;
};