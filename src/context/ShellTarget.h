#pragma once
#include <string>
#include <windows.h>

enum class ShellTargetType
{
	File,
	Directory,
	DirectoryBackground,
	Drive,
	FileSystemObject
};

bool ParseShellTargetType(const std::wstring& value, ShellTargetType& targetType);
bool IsShellTargetMatch(ShellTargetType configuredTarget, ShellTargetType actualTarget);
PCWSTR GetShellTargetRegistryFileType(ShellTargetType targetType);