#include "LaunchExecutor.h"
#include "ActionPlaceholders.h"
#include "ShellLog.h"
#include <vector>

bool LaunchExecutor::CanExecute(const MenuAction& action) const
{
    return action.type == MenuActionType::Launch;
}

bool LaunchExecutor::Execute(const MenuContext& context, const MenuAction& action, HWND hwnd)
{
    UNREFERENCED_PARAMETER(hwnd);

    const std::wstring commandLine = ExpandActionPlaceholders(action.command, context);
    if (commandLine.empty())
    {
        ShellLog(L"Launch action skipped: empty command.");
        return false;
    }

    STARTUPINFOW startupInfo = { sizeof(startupInfo) };
    PROCESS_INFORMATION processInfo = {};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    if (!CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        action.showWindow ? 0 : CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo))
    {
        ShellLog(L"CreateProcess failed: %s", commandLine.c_str());
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}