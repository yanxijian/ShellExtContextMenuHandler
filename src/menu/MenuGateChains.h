#pragma once

#include <string>
#include <vector>

struct MenuGateChains
{
    std::vector<std::wstring> extensionGates;
    std::vector<std::wstring> itemGates;
    std::vector<std::wstring> presentationGates;
    std::vector<std::wstring> executors;
};

void ApplyDefaultMenuGateChains(MenuGateChains& chains);
