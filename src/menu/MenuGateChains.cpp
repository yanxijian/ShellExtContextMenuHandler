#include "MenuGateChains.h"

void ApplyDefaultMenuGateChains(MenuGateChains& chains)
{
    if (chains.extensionGates.empty())
    {
        chains.extensionGates = { L"extensionPass" };
    }

    if (chains.itemGates.empty())
    {
        chains.itemGates = { L"jsonFilter" };
    }

    if (chains.presentationGates.empty())
    {
        chains.presentationGates = { L"presentationPass" };
    }

    if (chains.executors.empty())
    {
        chains.executors = { L"messageBox", L"launch" };
    }
}
