#include "ShellTarget.h"

bool ParseShellTargetType(const std::wstring& value, ShellTargetType& targetType)
{
	if (value == L"file")
	{
		targetType = ShellTargetType::File;
		return true;
	}
	if (value == L"directory")
	{
		targetType = ShellTargetType::Directory;
		return true;
	}
	if (value == L"directoryBackground")
	{
		targetType = ShellTargetType::DirectoryBackground;
		return true;
	}
	if (value == L"drive")
	{
		targetType = ShellTargetType::Drive;
		return true;
	}
	if (value == L"fileSystemObject")
	{
		targetType = ShellTargetType::FileSystemObject;
		return true;
	}

	return false;
}

bool IsShellTargetMatch(ShellTargetType configuredTarget, ShellTargetType actualTarget)
{
	if (configuredTarget == ShellTargetType::FileSystemObject)
	{
		return actualTarget == ShellTargetType::FileSystemObject;
	}

	return configuredTarget == actualTarget;
}

PCWSTR GetShellTargetRegistryFileType(ShellTargetType targetType)
{
	switch (targetType)
	{
	case ShellTargetType::File:
		return L"*";
	case ShellTargetType::Directory:
		return L"Directory";
	case ShellTargetType::DirectoryBackground:
		return L"Directory\\Background";
	case ShellTargetType::Drive:
		return L"Drive";
	case ShellTargetType::FileSystemObject:
		return L"AllFilesystemObjects";
	default:
		return nullptr;
	}
}
