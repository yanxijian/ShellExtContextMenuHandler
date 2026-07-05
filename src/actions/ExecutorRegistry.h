#pragma once

#include "IActionExecutor.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ExecutorRegistry
{
public:
    static ExecutorRegistry& Instance();

    void RegisterExecutor(const std::wstring& name, std::unique_ptr<IActionExecutor> executor);
    IActionExecutor* GetExecutor(const std::wstring& name) const;
    bool ExecuteChain(const MenuContext& context, const MenuAction& action, HWND hwnd) const;
    bool ExecuteChain(
        const MenuContext& context,
        const MenuAction& action,
        HWND hwnd,
        const std::vector<std::wstring>& executorNames) const;

    void RegisterBuiltInExecutors();

private:
    ExecutorRegistry() = default;

    std::vector<std::wstring> m_chainOrder;
    std::unordered_map<std::wstring, std::unique_ptr<IActionExecutor>> m_executors;
};
