#include "DemoActionLogExecutor.h"
#include "ShellLog.h"

bool DemoActionLogExecutor::CanExecute(const MenuAction& action) const
{
    UNREFERENCED_PARAMETER(action);
    return true;
}

bool DemoActionLogExecutor::Execute(
    const MenuContext& context,
    const MenuAction& action,
    HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    const wchar_t* actionName = action.type == MenuActionType::Launch ? L"launch" : L"messageBox";
    ShellLog(
        L"DemoActionLogExecutor: action=%s selection=%u",
        actionName,
        context.GetSelectionCount());
    return false;
}
