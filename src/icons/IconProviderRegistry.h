#pragma once

#include <windows.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class IIconProvider;

class IconProviderRegistry
{
public:
    static IconProviderRegistry& Instance();

    void RegisterProvider(const std::wstring& name, std::unique_ptr<IIconProvider> provider);
    HBITMAP LoadMenuItemIcon(const std::wstring& iconSpec, UINT dpi, HINSTANCE module) const;

    void RegisterBuiltInProviders();

private:
    IconProviderRegistry() = default;

    std::vector<std::wstring> m_chainOrder;
    std::unordered_map<std::wstring, std::unique_ptr<IIconProvider>> m_providers;
};