#pragma once

#include "IIconProvider.h"

class DefaultResourceIconProvider : public IIconProvider
{
public:
    bool CanProvide(const std::wstring& iconSpec) const override;
    HBITMAP LoadIcon(const std::wstring& iconSpec, UINT dpi, HINSTANCE module) override;
};