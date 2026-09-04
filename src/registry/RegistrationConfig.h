#pragma once

#include "ShellTarget.h"

#include <string>
#include <vector>

enum class ShellRegistrationConfigStatus
{
	Loaded,
	Missing,
	Invalid
};

ShellRegistrationConfigStatus LoadShellRegistrationConfig(
	const std::wstring& configPath,
	std::vector<ShellTargetType>& registrations);
