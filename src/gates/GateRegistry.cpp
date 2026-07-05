#include "GateRegistry.h"
#include "DemoHideTempItemGate.h"
#include "DemoReadOnlyPresentationGate.h"
#include "JsonFilterGate.h"
#include "ShellLog.h"
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

std::wstring GateRegistry::ResolveGateName(const std::wstring& spec)
{
    const std::wstring customPrefix = L"custom:";
    if (spec.size() > customPrefix.size()
        && _wcsnicmp(spec.c_str(), customPrefix.c_str(), static_cast<int>(customPrefix.size())) == 0)
    {
        return spec.substr(customPrefix.size());
    }

    return spec;
}

void GateRegistry::RegisterExtensionGate(
    const std::wstring& name,
    std::unique_ptr<IExtensionGate> gate)
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

bool GateRegistry::EvaluateExtensionChain(
    const MenuContext& context,
    const std::vector<std::wstring>& gateNames) const
{
    if (gateNames.empty())
    {
        IExtensionGate* gate = DefaultExtensionGate();
        return gate != nullptr && gate->ShouldActivate(context);
    }

    for (const auto& name : gateNames)
    {
        IExtensionGate* gate = GetExtensionGate(ResolveGateName(name));
        if (gate == nullptr)
        {
            ShellLog(L"Extension gate not found: %s", name.c_str());
            continue;
        }

        if (!gate->ShouldActivate(context))
        {
            return false;
        }
    }

    return true;
}

bool GateRegistry::EvaluateItemChain(
    const MenuContext& context,
    const MenuItemDef& item,
    const std::vector<std::wstring>& gateNames) const
{
    if (gateNames.empty())
    {
        IMenuItemGate* gate = DefaultItemGate();
        return gate != nullptr && gate->ShouldShow(context, item);
    }

    for (const auto& name : gateNames)
    {
        IMenuItemGate* gate = GetItemGate(ResolveGateName(name));
        if (gate == nullptr)
        {
            ShellLog(L"Item gate not found: %s", name.c_str());
            continue;
        }

        if (!gate->ShouldShow(context, item))
        {
            return false;
        }
    }

    return true;
}

MenuItemState GateRegistry::EvaluatePresentationChain(
    const MenuContext& context,
    const MenuItemDef& item,
    const std::vector<std::wstring>& gateNames) const
{
    if (gateNames.empty())
    {
        IMenuItemPresentationGate* gate = DefaultPresentationGate();
        if (gate == nullptr)
        {
            return MenuItemState::Enabled;
        }

        return gate->Evaluate(context, item);
    }

    MenuItemState state = MenuItemState::Enabled;
    for (const auto& name : gateNames)
    {
        IMenuItemPresentationGate* gate = GetPresentationGate(ResolveGateName(name));
        if (gate == nullptr)
        {
            ShellLog(L"Presentation gate not found: %s", name.c_str());
            continue;
        }

        const MenuItemState gateState = gate->Evaluate(context, item);
        if (gateState == MenuItemState::Hidden)
        {
            return MenuItemState::Hidden;
        }

        if (gateState == MenuItemState::Disabled)
        {
            state = MenuItemState::Disabled;
        }
    }

    return state;
}

void GateRegistry::RegisterBuiltInGates()
{
    RegisterExtensionGate(
        L"extensionPass",
        std::unique_ptr<IExtensionGate>(new PassthroughExtensionGate()));
    RegisterItemGate(L"jsonFilter", std::unique_ptr<IMenuItemGate>(new JsonFilterGate()));
    RegisterPresentationGate(
        L"presentationPass",
        std::unique_ptr<IMenuItemPresentationGate>(new PassthroughPresentationGate()));
    RegisterItemGate(
        L"demo:hideTemp",
        std::unique_ptr<IMenuItemGate>(new DemoHideTempItemGate()));
    RegisterPresentationGate(
        L"demo:readOnlyDisable",
        std::unique_ptr<IMenuItemPresentationGate>(new DemoReadOnlyPresentationGate()));
}
