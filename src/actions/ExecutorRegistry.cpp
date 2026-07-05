#include "ExecutorRegistry.h"
#include "DemoActionLogExecutor.h"
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
    return ExecuteChain(context, action, hwnd, m_chainOrder);
}

bool ExecutorRegistry::ExecuteChain(
    const MenuContext& context,
    const MenuAction& action,
    HWND hwnd,
    const std::vector<std::wstring>& executorNames) const
{
    const std::vector<std::wstring>& chain = executorNames.empty() ? m_chainOrder : executorNames;

    for (const auto& name : chain)
    {
        IActionExecutor* executor = GetExecutor(name);
        if (executor == nullptr)
        {
            ShellLog(L"Executor not found: %s", name.c_str());
            continue;
        }

        if (!executor->CanExecute(action))
        {
            continue;
        }

        if (executor->Execute(context, action, hwnd))
        {
            return true;
        }
    }

    ShellLog(L"No executor handled action.");
    return false;
}

void ExecutorRegistry::RegisterBuiltInExecutors()
{
    RegisterExecutor(
        L"demo:actionLog",
        std::unique_ptr<IActionExecutor>(new DemoActionLogExecutor()));
    RegisterExecutor(L"messageBox", std::unique_ptr<IActionExecutor>(new MessageBoxExecutor()));
    RegisterExecutor(L"launch", std::unique_ptr<IActionExecutor>(new LaunchExecutor()));
}
