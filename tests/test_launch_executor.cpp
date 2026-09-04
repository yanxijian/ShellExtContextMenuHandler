// Hijack regression test for LaunchExecutor.
// Compiles the REAL LaunchExecutor.cpp / ActionPlaceholders.cpp / PathHelpers.cpp.
// A fake marker.exe is planted in the current directory while a legitimate copy
// sits in a directory on PATH; the launch must resolve to the PATH copy.
#include "../src/actions/LaunchExecutor.cpp"
#include "../src/actions/ActionPlaceholders.cpp"
#include "../src/menu/PathHelpers.cpp"

#include <cstdio>
#include <filesystem>
#include <string>

#ifndef MARKER_HELPER_PATH
#define MARKER_HELPER_PATH L"shellext_marker_helper.exe"
#endif

void ShellLog(PCWSTR, ...) {}

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                                                                                \
	do                                                                                                                             \
	{                                                                                                                              \
		++g_total;                                                                                                                 \
		if (!(cond))                                                                                                               \
		{                                                                                                                          \
			++g_failed;                                                                                                            \
			printf("  FAIL(line %d): %s\n", __LINE__, #cond);                                                                       \
		}                                                                                                                          \
	} while (0)

static std::wstring g_baseDir;
static std::wstring g_fakeDir;
static std::wstring g_safeDir;

static std::wstring ReadFirstLine(const std::wstring& path)
{
	FILE* f = nullptr;
	if (_wfopen_s(&f, path.c_str(), L"r") != 0 || f == nullptr)
	{
		return L"";
	}
	wchar_t buffer[1024] = {};
	fgetws(buffer, 1024, f);
	fclose(f);
	std::wstring line(buffer);
	while (!line.empty() && (line.back() == L'\n' || line.back() == L'\r'))
	{
		line.pop_back();
	}
	return line;
}

static bool WaitForFile(const std::wstring& path, int iterations)
{
	for (int i = 0; i < iterations; ++i)
	{
		if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			Sleep(50); // allow the writer to finish
			return true;
		}
		Sleep(50);
	}
	return false;
}

static MenuContext SingleSelectionContext(const std::wstring& path)
{
	MenuContext context;
	SelectedItem item;
	item.path = path;
	context.selected.push_back(item);
	context.hasFiles = true;
	return context;
}

int main()
{
	wchar_t tempDir[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, tempDir);
	g_baseDir = std::wstring(tempDir) + L"shellext-tests\\launch_executor";
	g_fakeDir = g_baseDir + L"\\fake";
	g_safeDir = g_baseDir + L"\\safe";
	std::error_code directoryError;
	std::filesystem::create_directories(g_fakeDir, directoryError);
	std::filesystem::create_directories(g_safeDir, directoryError);

	const std::wstring markerSource = MARKER_HELPER_PATH;
	CopyFileW(markerSource.c_str(), (g_fakeDir + L"\\marker.exe").c_str(), FALSE);
	CopyFileW(markerSource.c_str(), (g_safeDir + L"\\marker.exe").c_str(), FALSE);

	// "my tools\my app.exe" for the spaced-path cases.
	const std::wstring spacedDir = g_baseDir + L"\\my tools";
	std::filesystem::create_directories(spacedDir, directoryError);
	CopyFileW(markerSource.c_str(), (spacedDir + L"\\my app.exe").c_str(), FALSE);

	// Prepend the safe directory to PATH and make the fake directory current.
	wchar_t originalPath[8192] = {};
	GetEnvironmentVariableW(L"PATH", originalPath, 8192);
	const std::wstring testPath = g_safeDir + L";" + originalPath;
	SetEnvironmentVariableW(L"PATH", testPath.c_str());
	SetCurrentDirectoryW(g_fakeDir.c_str());

	printf("== H4-1: bare name resolution skips the current directory ==\n");
	{
		std::wstring resolved;
		CHECK(TryResolveBareExecutable(L"marker.exe", resolved));
		printf("   resolved: [%ls]\n", resolved.c_str());
		CHECK(resolved == g_safeDir + L"\\marker.exe");

		std::wstring appName;
		CHECK(TryResolveApplicationName(L"marker.exe --flag arg", appName));
		CHECK(appName == g_safeDir + L"\\marker.exe");
	}

	printf("== H4-2: end-to-end Execute must launch the PATH copy, not the CWD fake ==\n");
	{
		const std::wstring outFile = g_baseDir + L"\\out_bare.txt";
		DeleteFileW(outFile.c_str());

		MenuContext context = SingleSelectionContext(outFile);
		MenuAction action;
		action.type = MenuActionType::Launch;
		action.command = L"marker.exe %1";

		LaunchExecutor executor;
		CHECK(executor.Execute(context, action, nullptr));
		CHECK(WaitForFile(outFile, 100));
		const std::wstring selfPath = ReadFirstLine(outFile);
		printf("   launched: [%ls]\n", selfPath.c_str());
		CHECK(selfPath == g_safeDir + L"\\marker.exe");
	}

	printf("== H4-3: unquoted application path containing spaces ==\n");
	{
		std::wstring resolved;
		CHECK(TryResolveApplicationName(spacedDir + L"\\my app.exe --flag x", resolved));
		CHECK(resolved == spacedDir + L"\\my app.exe");
		printf("   resolved: [%ls]\n", resolved.c_str());
	}

	printf("== H4-4: quoted application path containing spaces ==\n");
	{
		std::wstring resolved;
		CHECK(TryResolveApplicationName(L"\"" + spacedDir + L"\\my app.exe\" --flag x", resolved));
		CHECK(resolved == spacedDir + L"\\my app.exe");
	}

	printf("== H4-5: quoted bare name still resolves safely ==\n");
	{
		std::wstring resolved;
		CHECK(TryResolveApplicationName(L"\"marker.exe\" x", resolved));
		CHECK(resolved == g_safeDir + L"\\marker.exe");
	}

	printf("== H4-6: unresolvable name fails instead of falling back to CWD ==\n");
	{
		MenuContext context;
		MenuAction action;
		action.type = MenuActionType::Launch;
		action.command = L"definitely_missing_exe_xyz123.exe";
		LaunchExecutor executor;
		CHECK(!executor.Execute(context, action, nullptr));
	}

	SetEnvironmentVariableW(L"PATH", originalPath);
	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
