#include "MenuActionHandler.h"
#include "ExecutorRegistry.h"
#include "MenuGateChains.h"

void ExecuteMenuAction(const MenuAction& action, const MenuContext& context, HWND hwnd)
{
    MenuGateChains chains;
    ApplyDefaultMenuGateChains(chains);
    ExecutorRegistry::Instance().ExecuteChain(context, action, hwnd, chains.executors);
}