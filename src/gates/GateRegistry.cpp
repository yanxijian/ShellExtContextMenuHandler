#include "GateRegistry.h"
#include "JsonFilterGate.h"
#include <windows.h>

namespace
{
    class PassthroughExtensionGate : public IExtensionGate
    {
    public:
        bool ShouldActivate(const MenuContext& context) override
        {
            UNREFERENCED_PARAMETER(context);
            return true;
        }
    };

    class PassthroughPresentationGate : public IMenuItemPresentationGate
    {
    public:
        MenuItemState Evaluate(const MenuContext& context, const MenuItemDef& item) override
        {
            UNREFERENCED_PARAMETER(context);
            UNREFERENCED_PARAMETER(item);
            return MenuItemState::Enabled;
        }
    };
}

GateRegistry& GateRegistry::Instance()
{
    static GateRegistry registry;
    static bool gatesRegistered = false;
    if (!gatesRegistered)
    {
        registry.RegisterBuiltInGates();
        gatesRegistered = true;
    }
    return registry;
}

void GateRegistry::RegisterExtensionGate(const std::wstring& name, std::unique_ptr<IExtensionGate> gate)
{
    m_extensionGates[name] = std::move(gate);
}

void GateRegistry::RegisterItemGate(const std::wstring& name, std::unique_ptr<IMenuItemGate> gate)
{
    m_itemGates[name] = std::move(gate);
}

void GateRegistry::RegisterPresentationGate(
    const std::wstring& name,
    std::unique_ptr<IMenuItemPresentationGate> gate)
{
    m_presentationGates[name] = std::move(gate);
}

IExtensionGate* GateRegistry::GetExtensionGate(const std::wstring& name) const
{
    const auto it = m_extensionGates.find(name);
    return it == m_extensionGates.end() ? nullptr : it->second.get();
}

IMenuItemGate* GateRegistry::GetItemGate(const std::wstring& name) const
{
    const auto it = m_itemGates.find(name);
    return it == m_itemGates.end() ? nullptr : it->second.get();
}

IMenuItemPresentationGate* GateRegistry::GetPresentationGate(const std::wstring& name) const
{
    const auto it = m_presentationGates.find(name);
    return it == m_presentationGates.end() ? nullptr : it->second.get();
}

IExtensionGate* GateRegistry::DefaultExtensionGate() const
{
    return GetExtensionGate(L"extensionPass");
}

IMenuItemGate* GateRegistry::DefaultItemGate() const
{
    return GetItemGate(L"jsonFilter");
}

IMenuItemPresentationGate* GateRegistry::DefaultPresentationGate() const
{
    return GetPresentationGate(L"presentationPass");
}

void GateRegistry::RegisterBuiltInGates()
{
    RegisterExtensionGate(L"extensionPass", std::unique_ptr<IExtensionGate>(new PassthroughExtensionGate()));
    RegisterItemGate(L"jsonFilter", std::unique_ptr<IMenuItemGate>(new JsonFilterGate()));
    RegisterPresentationGate(
        L"presentationPass",
        std::unique_ptr<IMenuItemPresentationGate>(new PassthroughPresentationGate()));
}
