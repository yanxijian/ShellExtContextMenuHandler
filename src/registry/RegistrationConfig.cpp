#include "RegistrationConfig.h"

#include "ShellLog.h"

#include <algorithm>
#include <cstdio>
#include <vector>
#include <windows.h>

namespace
{
	bool ReadUtf8File(const std::wstring& path, std::wstring& content)
	{
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || file == nullptr)
		{
			return false;
		}

		if (fseek(file, 0, SEEK_END) != 0)
		{
			fclose(file);
			return false;
		}

		const long fileSize = ftell(file);
		if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0)
		{
			fclose(file);
			return false;
		}

		std::vector<char> bytes(static_cast<size_t>(fileSize));
		if (fileSize > 0 && fread(bytes.data(), 1, static_cast<size_t>(fileSize), file) != static_cast<size_t>(fileSize))
		{
			fclose(file);
			return false;
		}

		fclose(file);
		if (bytes.empty())
		{
			content.clear();
			return true;
		}

		const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
		if (wideLength <= 0)
		{
			return false;
		}

		std::vector<wchar_t> wideBuffer(static_cast<size_t>(wideLength));
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), wideBuffer.data(), wideLength)
			<= 0)
		{
			return false;
		}

		content.assign(wideBuffer.begin(), wideBuffer.end());
		return true;
	}

	bool ExtractStringArray(const std::wstring& json, PCWSTR key, std::vector<std::wstring>& values)
	{
		const std::wstring keyPattern = std::wstring(L"\"") + key + L"\"";
		const size_t keyPosition = json.find(keyPattern);
		if (keyPosition == std::wstring::npos)
		{
			return false;
		}

		const size_t colonPosition = json.find(L':', keyPosition + keyPattern.size());
		const size_t arrayStart = json.find(L'[', colonPosition);
		const size_t arrayEnd = json.find(L']', arrayStart);
		if (colonPosition == std::wstring::npos || arrayStart == std::wstring::npos || arrayEnd == std::wstring::npos)
		{
			return false;
		}

		const std::wstring arrayBody = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
		size_t position = 0;
		while (position < arrayBody.size())
		{
			const size_t startQuote = arrayBody.find(L'"', position);
			if (startQuote == std::wstring::npos)
			{
				break;
			}

			const size_t endQuote = arrayBody.find(L'"', startQuote + 1);
			if (endQuote == std::wstring::npos)
			{
				return false;
			}

			values.push_back(arrayBody.substr(startQuote + 1, endQuote - startQuote - 1));
			position = endQuote + 1;
		}

		return true;
	}

	bool ExtractUIntValue(const std::wstring& json, PCWSTR key, UINT& value)
	{
		const std::wstring keyPattern = std::wstring(L"\"") + key + L"\"";
		const size_t keyPosition = json.find(keyPattern);
		if (keyPosition == std::wstring::npos)
		{
			return false;
		}

		const size_t colonPosition = json.find(L':', keyPosition + keyPattern.size());
		if (colonPosition == std::wstring::npos)
		{
			return false;
		}

		const size_t valueStart = json.find_first_not_of(L" \t\r\n", colonPosition + 1);
		if (valueStart == std::wstring::npos)
		{
			return false;
		}

		wchar_t* end = nullptr;
		const unsigned long parsed = wcstoul(json.c_str() + valueStart, &end, 10);
		if (end == json.c_str() + valueStart || parsed > UINT_MAX)
		{
			return false;
		}

		value = static_cast<UINT>(parsed);
		return true;
	}

	void SetDefaultRegistrations(std::vector<ShellTargetType>& registrations)
	{
		registrations = {ShellTargetType::File};
	}
} // namespace

ShellRegistrationConfigStatus LoadShellRegistrationConfig(const std::wstring& configPath, std::vector<ShellTargetType>& registrations)
{
	registrations.clear();

	std::wstring json;
	std::vector<std::wstring> configuredTargets;
	if (!ReadUtf8File(configPath, json))
	{
		SetDefaultRegistrations(registrations);
		ShellLog(L"Using default shell registration target: file");
		return ShellRegistrationConfigStatus::Missing;
	}

	UINT schemaVersion = 0;
	if (!ExtractUIntValue(json, L"schemaVersion", schemaVersion) || schemaVersion != 1
		|| !ExtractStringArray(json, L"shellRegistrations", configuredTargets) || configuredTargets.empty())
	{
		ShellLog(L"Invalid shell registration configuration: %s", configPath.c_str());
		return ShellRegistrationConfigStatus::Invalid;
	}

	for (const auto& configuredTarget : configuredTargets)
	{
		ShellTargetType targetType;
		if (!ParseShellTargetType(configuredTarget, targetType))
		{
			registrations.clear();
			ShellLog(L"Unknown shell registration target: %s", configuredTarget.c_str());
			return ShellRegistrationConfigStatus::Invalid;
		}

		if (std::find(registrations.begin(), registrations.end(), targetType) != registrations.end())
		{
			registrations.clear();
			ShellLog(L"Duplicate shell registration target: %s", configuredTarget.c_str());
			return ShellRegistrationConfigStatus::Invalid;
		}

		registrations.push_back(targetType);
	}

	return ShellRegistrationConfigStatus::Loaded;
}