#include "ExecutorRegistry.h"
#include "LaunchExecutor.h"
#include "MessageBoxExecutor.h"
#include "ShellLog.h"

ExecutorRegistry& ExecutorRegistry::Instance()
{
    static ExecutorRegistry registry;
    static bool executorsRegistered = false;
    if (!executorsRegistered)
    {
        registry.RegisterBuiltInExecutors();
        executorsRegistered = true;
    }
    return registry;
}

void ExecutorRegistry::RegisterExecutor(
    const std::wstring& name,
    std::unique_ptr<IActionExecutor> executor)
{
    if (executor == nullptr)
    {
        return;
    }

    const auto it = m_executors.find(name);
    if (it == m_executors.end())
    {
        m_chainOrder.push_back(name);
    }

    m_executors[name] = std::move(executor);
}

IActionExecutor* ExecutorRegistry::GetExecutor(const std::wstring& name) const
{
    const auto it = m_executors.find(name);
    return it == m_executors.end() ? nullptr : it->second.get();
}

bool ExecutorRegistry::ExecuteChain(
    const MenuContext& context,
    const MenuAction& action,
    HWND hwnd) const
{
    for (const auto& name : m_chainOrder)
    {
        IActionExecutor* executor = GetExecutor(name);
        if (executor == nullptr || !executor->CanExecute(action))
        {
            continue;
        }

        if (executor->Execute(context, action, hwnd))
        {
            return true;
        }

        ShellLog(L"Executor '%s' declined action after CanExecute matched.", name.c_str());
    }

    ShellLog(L"No executor handled action.");
    return false;
}

void ExecutorRegistry::RegisterBuiltInExecutors()
{
    RegisterExecutor(L"messageBox", std::unique_ptr<IActionExecutor>(new MessageBoxExecutor()));
    RegisterExecutor(L"launch", std::unique_ptr<IActionExecutor>(new LaunchExecutor()));
}