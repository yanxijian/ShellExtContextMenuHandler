#include "MenuActionHandler.h"
#include "PathHelpers.h"
#include "ShellLog.h"
#include <windows.h>
#include <strsafe.h>

namespace
{
    std::wstring ExpandPlaceholders(const std::wstring& text, const MenuContext& context)
    {
        std::wstring result = text;
        const auto replaceAll = [&result](const std::wstring& token, const std::wstring& value)
        {
            size_t position = 0;
            while ((position = result.find(token, position)) != std::wstring::npos)
            {
                result.replace(position, token.size(), value);
                position += value.size();
            }
        };

        if (!context.selected.empty())
        {
            replaceAll(L"%1", QuotePath(context.selected.front().path));
            replaceAll(L"%D", QuotePath(GetParentDirectory(context.selected.front().path)));
        }
        else if (!context.folderPath.empty())
        {
            replaceAll(L"%1", QuotePath(context.folderPath));
            replaceAll(L"%D", QuotePath(context.folderPath));
        }

        replaceAll(L"%*", JoinQuotedPaths(context.GetSelectedPaths()));

        return result;
    }

    void ShowMessageAction(const MenuAction& action, const MenuContext& context, HWND hwnd)
    {
        const std::wstring message = ExpandPlaceholders(action.templateText, context);
        const std::wstring title = action.title.empty() ? L"Shell Extension" : action.title;
        MessageBoxW(hwnd, message.c_str(), title.c_str(), MB_OK);
    }

    void LaunchProcessAction(const MenuAction& action, const MenuContext& context)
    {
        const std::wstring commandLine = ExpandPlaceholders(action.command, context);
        if (commandLine.empty())
        {
            ShellLog(L"Launch action skipped: empty command.");
            return;
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
            return;
        }

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
    }
}

void ExecuteMenuAction(const MenuAction& action, const MenuContext& context, HWND hwnd)
{
    switch (action.type)
    {
    case MenuActionType::ShowMessage:
        ShowMessageAction(action, context, hwnd);
        break;
    case MenuActionType::Launch:
        LaunchProcessAction(action, context);
        break;
    }
}
