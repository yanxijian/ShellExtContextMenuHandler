#include "ContextBuilder.h"

#include "DpiProvider.h"

#include <ShlObj.h>
#include <Shlwapi.h>
#include <stdlib.h>

#pragma comment(lib, "shlwapi.lib")

namespace
{
	bool IsDrivePath(const std::wstring& path)
	{
		return path.size() == 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
	}

	void ParsePathComponents(const std::wstring& path, SelectedItem& item)
	{
		// Size the buffers from the path itself instead of MAX_PATH so long
		// paths are not silently truncated.
		const size_t bufferLength = path.size() + 1;
		std::vector<wchar_t> fileName(bufferLength, L'\0');
		std::vector<wchar_t> extension(bufferLength, L'\0');
		if (_wsplitpath_s(path.c_str(), nullptr, 0, nullptr, 0, fileName.data(), bufferLength, extension.data(), bufferLength) == 0)
		{
			item.fileName = fileName.data();
			item.extension = extension.data();
		}
	}

	bool TryReadProgId(HKEY hKeyProgID, std::wstring& progId)
	{
		if (hKeyProgID == nullptr)
		{
			return false;
		}

		wchar_t buffer[260] = {};
		LONG bufferChars = ARRAYSIZE(buffer);
		const LONG result = RegQueryValueW(hKeyProgID, nullptr, buffer, &bufferChars);
		if (result != ERROR_SUCCESS || buffer[0] == L'\0')
		{
			return false;
		}

		progId = buffer;
		return true;
	}

	bool TryPopulateFolderPath(LPCITEMIDLIST pidlFolder, MenuContext& context)
	{
		if (pidlFolder == nullptr)
		{
			return false;
		}

		wchar_t folderPath[MAX_PATH] = {};
		if (!SHGetPathFromIDListW(pidlFolder, folderPath))
		{
			return false;
		}

		context.folderPath = folderPath;
		return true;
	}

	bool TryPopulateFromDataObject(LPDATAOBJECT dataObject, MenuContext& context)
	{
		if (dataObject == nullptr)
		{
			return false;
		}

		FORMATETC format = {CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM storage = {};
		if (!SUCCEEDED(dataObject->GetData(&format, &storage)))
		{
			return false;
		}

		HDROP dropHandle = static_cast<HDROP>(GlobalLock(storage.hGlobal));
		if (dropHandle == nullptr)
		{
			ReleaseStgMedium(&storage);
			return false;
		}

		const UINT fileCount = DragQueryFileW(dropHandle, 0xFFFFFFFF, nullptr, 0);
		for (UINT index = 0; index < fileCount; ++index)
		{
			// Ask for the required buffer size (in characters, without the
			// terminating null) first, then allocate exactly that amount so
			// paths longer than MAX_PATH are not silently truncated.
			const UINT pathLength = DragQueryFileW(dropHandle, index, nullptr, 0);
			if (pathLength == 0)
			{
				continue;
			}

			std::vector<wchar_t> selectedPath(pathLength + 1, L'\0');
			if (DragQueryFileW(dropHandle, index, selectedPath.data(), pathLength + 1) != pathLength)
			{
				continue;
			}

			SelectedItem item;
			item.path = selectedPath.data();
			item.attributes = GetFileAttributesW(selectedPath.data());
			item.isDirectory = item.attributes != INVALID_FILE_ATTRIBUTES && (item.attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			ParsePathComponents(item.path, item);

			context.selected.push_back(item);
			if (item.isDirectory)
			{
				context.hasFolders = true;
			}
			else
			{
				context.hasFiles = true;
			}
		}

		GlobalUnlock(storage.hGlobal);
		ReleaseStgMedium(&storage);
		return !context.selected.empty();
	}
} // namespace

bool BuildMenuContext(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY hKeyProgID, MenuContext& context)
{
	context = MenuContext();
	TryReadProgId(hKeyProgID, context.progId);
	TryPopulateFolderPath(pidlFolder, context);
	TryPopulateFromDataObject(dataObject, context);
	if (context.GetSelectionCount() == 0 && !context.folderPath.empty())
	{
		context.targetType = ShellTargetType::DirectoryBackground;
	}
	else if (context.hasFolders && !context.hasFiles)
	{
		context.targetType = ShellTargetType::Directory;
		if (context.selected.size() == 1 && IsDrivePath(context.selected[0].path))
		{
			context.targetType = ShellTargetType::Drive;
		}
	}
	else if (context.hasFiles && context.hasFolders)
	{
		context.targetType = ShellTargetType::FileSystemObject;
	}
	else
	{
		context.targetType = ShellTargetType::File;
	}
	context.systemDpi = DpiProvider::GetSystemDpi();
	return context.GetSelectionCount() > 0 || !context.folderPath.empty();
}