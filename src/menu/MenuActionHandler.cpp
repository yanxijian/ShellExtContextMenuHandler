#include "MenuActionHandler.h"
#include "ExecutorRegistry.h"

void ExecuteMenuAction(const MenuAction& action, const MenuContext& context, HWND hwnd)
{
    ExecutorRegistry::Instance().ExecuteChain(context, action, hwnd);
}