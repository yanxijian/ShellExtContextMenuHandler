#include "PathHelpers.h"

#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

std::wstring QuotePath(const std::wstring& path)
{
	if (path.empty())
	{
		return L"\"\"";
	}

	if (path.find(L' ') == std::wstring::npos && path.find(L'\t') == std::wstring::npos)
	{
		return path;
	}

	return L"\"" + path + L"\"";
}

std::wstring JoinQuotedPaths(const std::vector<std::wstring>& paths)
{
	std::wstring joined;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		if (i > 0)
		{
			joined.append(L" ");
		}
		joined.append(QuotePath(paths[i]));
	}
	return joined;
}

std::wstring GetParentDirectory(const std::wstring& path)
{
	if (path.empty())
	{
		return L"";
	}

	// Keep a drive root (for example "C:\") as its own parent instead of
	// returning an empty string.
	if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
	{
		bool onlyRoot = true;
		for (size_t i = 3; i < path.size(); ++i)
		{
			if (path[i] != L'\\' && path[i] != L'/')
			{
				onlyRoot = false;
				break;
			}
		}
		if (onlyRoot)
		{
			return path.substr(0, 3);
		}
	}

	size_t end = path.size();
	while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/'))
	{
		--end;
	}

	if (end == 0)
	{
		return L"";
	}

	const size_t separator = path.find_last_of(L"\\/", end - 1);
	if (separator == std::wstring::npos)
	{
		return L"";
	}

	if (separator == 2 && path.size() > 2 && path[1] == L':')
	{
		return path.substr(0, 3);
	}

	if (separator == 0)
	{
		return path.substr(0, 1);
	}

	return path.substr(0, separator);
}

std::wstring GetPathName(const std::wstring& path)
{
	if (path.empty())
	{
		return L"";
	}

	size_t end = path.size();
	while (end > 3 && (path[end - 1] == L'\\' || path[end - 1] == L'/'))
	{
		--end;
	}

	const size_t separator = path.find_last_of(L"\\/", end - 1);
	if (separator == std::wstring::npos || separator + 1 >= end)
	{
		return path.substr(0, end);
	}

	return path.substr(separator + 1, end - separator - 1);
}

std::wstring GetModuleFilePath(HINSTANCE module)
{
	if (module == nullptr)
	{
		return L"";
	}

	std::vector<wchar_t> buffer(MAX_PATH);
	for (;;)
	{
		const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (length == 0)
		{
			return L"";
		}

		if (length < buffer.size())
		{
			return std::wstring(buffer.data(), length);
		}

		// GetModuleFileNameW returns the buffer size when it truncates. Grow
		// instead of silently registering or loading a truncated path.
		if (buffer.size() >= 32768)
		{
			return L"";
		}
		buffer.resize(buffer.size() * 2);
	}
}

std::wstring ResolveModuleRelativePath(HINSTANCE module, const std::wstring& relativePath)
{
	if (relativePath.empty())
	{
		return L"";
	}

	if (PathIsRelativeW(relativePath.c_str()) == FALSE)
	{
		return relativePath;
	}

	const std::wstring modulePath = GetModuleFilePath(module);
	if (modulePath.empty())
	{
		return relativePath;
	}

	const size_t separator = modulePath.find_last_of(L"\\/");
	if (separator == std::wstring::npos)
	{
		return relativePath;
	}

	std::wstring combined = modulePath.substr(0, separator);
	if (!combined.empty() && combined.back() != L'\\' && combined.back() != L'/')
	{
		combined += L'\\';
	}
	combined += relativePath;
	return combined;
}

bool HasExtensionIgnoreCase(const std::wstring& path, const std::wstring& extension)
{
	if (path.empty() || extension.empty())
	{
		return false;
	}

	const size_t pathLength = path.size();
	const size_t extensionLength = extension.size();
	if (pathLength < extensionLength)
	{
		return false;
	}

	return _wcsicmp(path.c_str() + (pathLength - extensionLength), extension.c_str()) == 0;
}