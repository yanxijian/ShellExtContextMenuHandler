#include "IconProviderRegistry.h"
#include "BitmapFileIconProvider.h"
#include "DefaultResourceIconProvider.h"
#include "IIconProvider.h"
#include "SvgIconProvider.h"
#include "ShellLog.h"

IconProviderRegistry& IconProviderRegistry::Instance()
{
    static IconProviderRegistry registry;
    static bool providersRegistered = false;
    if (!providersRegistered)
    {
        registry.RegisterBuiltInProviders();
        providersRegistered = true;
    }
    return registry;
}

void IconProviderRegistry::RegisterProvider(
    const std::wstring& name,
    std::unique_ptr<IIconProvider> provider)
{
    if (provider == nullptr)
    {
        return;
    }

    if (m_providers.find(name) == m_providers.end())
    {
        m_chainOrder.push_back(name);
    }

    m_providers[name] = std::move(provider);
}

HBITMAP IconProviderRegistry::LoadMenuItemIcon(
    const std::wstring& iconSpec,
    UINT dpi,
    HINSTANCE module) const
{
    for (const auto& name : m_chainOrder)
    {
        const auto it = m_providers.find(name);
        if (it == m_providers.end() || it->second == nullptr)
        {
            continue;
        }

        IIconProvider* provider = it->second.get();
        if (!provider->CanProvide(iconSpec))
        {
            continue;
        }

        HBITMAP bitmap = provider->LoadIcon(iconSpec, dpi, module);
        if (bitmap != nullptr)
        {
            return bitmap;
        }

        ShellLog(L"Icon provider '%s' failed for icon spec.", name.c_str());
    }

    DefaultResourceIconProvider fallbackProvider;
    return fallbackProvider.LoadIcon(L"", dpi, module);
}

void IconProviderRegistry::RegisterBuiltInProviders()
{
    RegisterProvider(L"svg", std::unique_ptr<IIconProvider>(new SvgIconProvider()));
    RegisterProvider(L"bmp", std::unique_ptr<IIconProvider>(new BitmapFileIconProvider()));
    RegisterProvider(L"default", std::unique_ptr<IIconProvider>(new DefaultResourceIconProvider()));
}