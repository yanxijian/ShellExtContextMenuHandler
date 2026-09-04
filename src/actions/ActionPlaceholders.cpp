#include "ActionPlaceholders.h"

#include "PathHelpers.h"

std::wstring ExpandActionPlaceholders(const std::wstring& text, const MenuContext& context)
{
	const bool hasSelection = !context.selected.empty();
	const bool hasSelectionOrFolder = hasSelection || !context.folderPath.empty();

	std::wstring selectionValue;
	std::wstring directoryValue;
	if (hasSelection)
	{
		selectionValue = QuotePath(context.selected.front().path);
		directoryValue = QuotePath(GetParentDirectory(context.selected.front().path));
	}
	else if (!context.folderPath.empty())
	{
		selectionValue = QuotePath(context.folderPath);
		directoryValue = QuotePath(context.folderPath);
	}

	const std::wstring nameValue = GetPathName(hasSelection ? context.selected.front().path : context.folderPath);
	const std::wstring allPathsValue = JoinQuotedPaths(context.GetSelectedPaths());

	// Expand every token from the original text in a single pass. Substitution
	// values are appended directly to the result and are never re-scanned, so a
	// selected file whose name contains a token (e.g. a file named "%N") can no
	// longer corrupt other placeholders.
	std::wstring result;
	result.reserve(text.size());
	for (size_t position = 0; position < text.size();)
	{
		const wchar_t current = text[position];
		if (current != L'%' || position + 1 >= text.size())
		{
			result.push_back(current);
			++position;
			continue;
		}

		const wchar_t token = text[position + 1];
		if (token == L'1' && hasSelectionOrFolder)
		{
			result += selectionValue;
			position += 2;
		}
		else if (token == L'D' && hasSelectionOrFolder)
		{
			result += directoryValue;
			position += 2;
		}
		else if (token == L'N')
		{
			result += nameValue;
			position += 2;
		}
		else if (token == L'*')
		{
			result += allPathsValue;
			position += 2;
		}
		else
		{
			result.push_back(current);
			++position;
		}
	}

	return result;
}
