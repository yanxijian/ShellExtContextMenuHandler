#include "MessageBoxExecutor.h"
#include "ActionPlaceholders.h"

bool MessageBoxExecutor::CanExecute(const MenuAction& action) const
{
    return action.type == MenuActionType::ShowMessage;
}

bool MessageBoxExecutor::Execute(const MenuContext& context, const MenuAction& action, HWND hwnd)
{
    const std::wstring message = ExpandActionPlaceholders(action.templateText, context);
    const std::wstring title = action.title.empty() ? L"Shell Extension" : action.title;
    MessageBoxW(hwnd, message.c_str(), title.c_str(), MB_OK);
    return true;
}