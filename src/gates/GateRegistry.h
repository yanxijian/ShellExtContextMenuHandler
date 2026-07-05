#pragma once

#include "GateTypes.h"
#include "IExtensionGate.h"
#include "IMenuItemGate.h"
#include "IMenuItemPresentationGate.h"
#include "MenuItem.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GateRegistry
{
public:
    static GateRegistry& Instance();

    void RegisterExtensionGate(const std::wstring& name, std::unique_ptr<IExtensionGate> gate);
    void RegisterItemGate(const std::wstring& name, std::unique_ptr<IMenuItemGate> gate);
    void RegisterPresentationGate(const std::wstring& name, std::unique_ptr<IMenuItemPresentationGate> gate);

    IExtensionGate* GetExtensionGate(const std::wstring& name) const;
    IMenuItemGate* GetItemGate(const std::wstring& name) const;
    IMenuItemPresentationGate* GetPresentationGate(const std::wstring& name) const;

    IExtensionGate* DefaultExtensionGate() const;
    IMenuItemGate* DefaultItemGate() const;
    IMenuItemPresentationGate* DefaultPresentationGate() const;

    bool EvaluateExtensionChain(
        const MenuContext& context,
        const std::vector<std::wstring>& gateNames) const;
    bool EvaluateItemChain(
        const MenuContext& context,
        const MenuItemDef& item,
        const std::vector<std::wstring>& gateNames) const;
    MenuItemState EvaluatePresentationChain(
        const MenuContext& context,
        const MenuItemDef& item,
        const std::vector<std::wstring>& gateNames) const;

    void RegisterBuiltInGates();

private:
    GateRegistry() = default;

    static std::wstring ResolveGateName(const std::wstring& spec);

    std::unordered_map<std::wstring, std::unique_ptr<IExtensionGate>> m_extensionGates;
    std::unordered_map<std::wstring, std::unique_ptr<IMenuItemGate>> m_itemGates;
    std::unordered_map<std::wstring, std::unique_ptr<IMenuItemPresentationGate>> m_presentationGates;
};
