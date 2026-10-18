#include "RegistrationConfig.h"

#include "ShellLog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>

namespace
{
	using Json = nlohmann::json;

	bool ReadUtf8File(const std::wstring& path, std::string& content)
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
		content.assign(bytes.begin(), bytes.end());

		if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF
			&& static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF)
		{
			content.erase(0, 3);
		}

		return true;
	}

	bool Utf8ToWide(const std::string& value, std::wstring& wide)
	{
		if (value.empty())
		{
			wide.clear();
			return true;
		}

		const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (length <= 0)
		{
			return false;
		}

		wide.resize(static_cast<size_t>(length));
		return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), length)
			   == length;
	}

	void SetDefaultRegistrations(std::vector<ShellTargetType>& registrations)
	{
		registrations = {ShellTargetType::File};
	}
} // namespace

ShellRegistrationConfigStatus LoadShellRegistrationConfig(const std::wstring& configPath, std::vector<ShellTargetType>& registrations)
{
	registrations.clear();

	std::string jsonText;
	if (!ReadUtf8File(configPath, jsonText))
	{
		SetDefaultRegistrations(registrations);
		ShellLog(L"Using default shell registration target: file");
		return ShellRegistrationConfigStatus::Missing;
	}

	const Json root = Json::parse(jsonText, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		ShellLog(L"Invalid shell registration configuration: %s", configPath.c_str());
		return ShellRegistrationConfigStatus::Invalid;
	}

	const auto schemaVersion = root.find("schemaVersion");
	if (schemaVersion == root.end() || !schemaVersion->is_number_unsigned() || schemaVersion->get<std::uint64_t>() != 1)
	{
		ShellLog(L"Unsupported shell registration schema version: %s", configPath.c_str());
		return ShellRegistrationConfigStatus::Invalid;
	}

	const auto configuredTargets = root.find("shellRegistrations");
	if (configuredTargets == root.end() || !configuredTargets->is_array() || configuredTargets->empty())
	{
		ShellLog(L"Invalid shell registration target list: %s", configPath.c_str());
		return ShellRegistrationConfigStatus::Invalid;
	}

	for (const auto& element : *configuredTargets)
	{
		if (!element.is_string() || element.get_ref<const std::string&>().empty())
		{
			ShellLog(L"Invalid shell registration target entry: %s", configPath.c_str());
			registrations.clear();
			return ShellRegistrationConfigStatus::Invalid;
		}

		std::wstring targetName;
		if (!Utf8ToWide(element.get_ref<const std::string&>(), targetName))
		{
			ShellLog(L"Invalid UTF-8 in shell registration target: %s", configPath.c_str());
			registrations.clear();
			return ShellRegistrationConfigStatus::Invalid;
		}

		ShellTargetType targetType;
		if (!ParseShellTargetType(targetName, targetType))
		{
			ShellLog(L"Unknown shell registration target: %s", targetName.c_str());
			registrations.clear();
			return ShellRegistrationConfigStatus::Invalid;
		}

		if (std::find(registrations.begin(), registrations.end(), targetType) != registrations.end())
		{
			ShellLog(L"Duplicate shell registration target: %s", targetName.c_str());
			registrations.clear();
			return ShellRegistrationConfigStatus::Invalid;
		}

		registrations.push_back(targetType);
	}

	return ShellRegistrationConfigStatus::Loaded;
}
