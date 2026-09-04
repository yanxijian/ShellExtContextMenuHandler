#include "LaunchExecutor.h"

#include "ActionPlaceholders.h"
#include "ShellLog.h"

#include <cwctype>
#include <vector>

namespace
{
	// Collects the whitespace-delimited tokens of a command line.
	std::vector<std::wstring> SplitCommandTokens(const std::wstring& commandLine)
	{
		std::vector<std::wstring> tokens;
		size_t position = 0;
		while (position < commandLine.size())
		{
			while (position < commandLine.size() && iswspace(commandLine[position]))
			{
				++position;
			}
			if (position >= commandLine.size())
			{
				break;
			}

			size_t end = position;
			while (end < commandLine.size() && !iswspace(commandLine[end]))
			{
				++end;
			}
			tokens.push_back(commandLine.substr(position, end - position));
			position = end;
		}
		return tokens;
	}

	bool IsBareExecutableName(const std::wstring& token)
	{
		return token.find(L'\\') == std::wstring::npos && token.find(L'/') == std::wstring::npos && token.find(L':') == std::wstring::npos;
	}

	bool IsAbsoluteEntry(const std::wstring& entry)
	{
		if (entry.size() >= 3 && (entry[2] == L'\\' || entry[2] == L'/'))
		{
			const wchar_t drive = towupper(entry[0]);
			return entry[1] == L':' && drive >= L'A' && drive <= L'Z';
		}
		return entry.size() >= 2 && entry[0] == L'\\' && entry[1] == L'\\';
	}

	// Resolves a bare executable name against the system directories and the
	// absolute entries of PATH. The current directory is deliberately excluded,
	// so a planted executable in whatever folder the host process happens to
	// use cannot hijack the launch.
	bool TryResolveBareExecutable(const std::wstring& name, std::wstring& resolvedPath)
	{
		std::wstring searchPath;

		wchar_t systemDirectory[MAX_PATH] = {};
		const UINT systemLength = GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory));
		if (systemLength > 0 && systemLength < ARRAYSIZE(systemDirectory))
		{
			searchPath += systemDirectory;
			searchPath += L';';
		}

		wchar_t windowsDirectory[MAX_PATH] = {};
		const UINT windowsLength = GetWindowsDirectoryW(windowsDirectory, ARRAYSIZE(windowsDirectory));
		if (windowsLength > 0 && windowsLength < ARRAYSIZE(windowsDirectory))
		{
			searchPath += windowsDirectory;
			searchPath += L';';
		}

		std::vector<wchar_t> pathBuffer(4096);
		const DWORD pathLength = GetEnvironmentVariableW(L"PATH", pathBuffer.data(), static_cast<DWORD>(pathBuffer.size()));
		if (pathLength > 0 && pathLength < pathBuffer.size())
		{
			const std::wstring pathValue(pathBuffer.data());
			size_t start = 0;
			while (start < pathValue.size())
			{
				size_t end = pathValue.find(L';', start);
				if (end == std::wstring::npos)
				{
					end = pathValue.size();
				}
				const std::wstring entry = pathValue.substr(start, end - start);
				if (!entry.empty() && IsAbsoluteEntry(entry))
				{
					searchPath += entry;
					searchPath += L';';
				}
				start = end + 1;
			}
		}

		std::vector<wchar_t> resolved(1024);
		PWSTR filePart = nullptr;
		DWORD length = SearchPathW(searchPath.c_str(), name.c_str(), nullptr, static_cast<DWORD>(resolved.size()), resolved.data(), &filePart);
		if (length == 0)
		{
			return false;
		}
		if (length >= resolved.size())
		{
			resolved.resize(length + 1);
			length = SearchPathW(searchPath.c_str(), name.c_str(), nullptr, static_cast<DWORD>(resolved.size()), resolved.data(), &filePart);
			if (length == 0 || length >= resolved.size())
			{
				return false;
			}
		}

		resolvedPath.assign(resolved.data(), length);
		return true;
	}

	// Mirrors the documented CreateProcess application-name search for an
	// unquoted application path that contains spaces: successively longer
	// prefixes are tried until an existing file is found.
	bool TryResolveUnquotedPathedExecutable(const std::vector<std::wstring>& tokens, std::wstring& resolvedPath)
	{
		if (tokens.empty())
		{
			return false;
		}

		std::wstring candidate = tokens[0];
		for (size_t index = 0; index < tokens.size(); ++index)
		{
			if (index > 0)
			{
				candidate += L' ';
				candidate += tokens[index];
			}

			if (candidate.find(L'\\') == std::wstring::npos && candidate.find(L'/') == std::wstring::npos)
			{
				continue;
			}

			const DWORD attributes = GetFileAttributesW(candidate.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				resolvedPath = candidate;
				return true;
			}
		}

		return false;
	}

	// Determines the executable to launch from the expanded command line
	// without ever consulting the current directory.
	bool TryResolveApplicationName(const std::wstring& commandLine, std::wstring& applicationName)
	{
		size_t start = 0;
		while (start < commandLine.size() && iswspace(commandLine[start]))
		{
			++start;
		}
		if (start >= commandLine.size())
		{
			return false;
		}

		if (commandLine[start] == L'"')
		{
			const size_t endQuote = commandLine.find(L'"', start + 1);
			if (endQuote == std::wstring::npos)
			{
				return false;
			}

			const std::wstring token = commandLine.substr(start + 1, endQuote - start - 1);
			if (token.empty() || IsBareExecutableName(token))
			{
				return !token.empty() && TryResolveBareExecutable(token, applicationName);
			}

			applicationName = token;
			return true;
		}

		const std::vector<std::wstring> tokens = SplitCommandTokens(commandLine);
		if (tokens.empty())
		{
			return false;
		}

		if (IsBareExecutableName(tokens[0]))
		{
			return TryResolveBareExecutable(tokens[0], applicationName);
		}

		return TryResolveUnquotedPathedExecutable(tokens, applicationName);
	}
} // namespace

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

	// Resolve the application explicitly. Passing lpApplicationName = nullptr
	// makes CreateProcessW search the caller's current directory first, which
	// allowed a planted executable to hijack bare command names.
	std::wstring applicationName;
	if (!TryResolveApplicationName(commandLine, applicationName))
	{
		ShellLog(L"Launch action skipped: could not resolve application from command: %s", commandLine.c_str());
		return false;
	}

	STARTUPINFOW startupInfo = {sizeof(startupInfo)};
	PROCESS_INFORMATION processInfo = {};
	std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
	mutableCommand.push_back(L'\0');

	if (!CreateProcessW(applicationName.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE, action.showWindow ? 0 : CREATE_NO_WINDOW, nullptr,
				nullptr, &startupInfo, &processInfo))
	{
		ShellLog(L"CreateProcess failed (%s): %s", applicationName.c_str(), commandLine.c_str());
		return false;
	}

	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	return true;
}
