#include "ActionPlaceholders.h"
#include "PathHelpers.h"

std::wstring ExpandActionPlaceholders(const std::wstring& text, const MenuContext& context)
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