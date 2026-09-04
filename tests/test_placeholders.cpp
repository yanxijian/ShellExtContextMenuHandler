// Verification harness for single-pass placeholder expansion.
// Compiles the REAL ActionPlaceholders.cpp and PathHelpers.cpp.
#include "../src/actions/ActionPlaceholders.cpp"
#include "../src/menu/PathHelpers.cpp"

#include <cstdio>

static int g_failed = 0;
static int g_total = 0;

#define CHECK_EQ(actual, expected)                                                                                                  \
	do                                                                                                                              \
	{                                                                                                                               \
		++g_total;                                                                                                                  \
		if (actual != expected)                                                                                                     \
		{                                                                                                                           \
			++g_failed;                                                                                                             \
			printf("  FAIL(line %d):\n    got:      [%ls]\n    expected: [%ls]\n", __LINE__, actual.c_str(), expected.c_str());      \
		}                                                                                                                           \
	} while (0)

static MenuContext SingleFileContext(const wchar_t* path)
{
	MenuContext ctx;
	SelectedItem item;
	item.path = path;
	item.isDirectory = false;
	ctx.selected.push_back(item);
	ctx.hasFiles = true;
	return ctx;
}

int main()
{
	printf("== P1: file name contains literal %%N (contamination via %%1) ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\dir\\%N file.txt");
		CHECK_EQ(ExpandActionPlaceholders(L"open %1 | name=%N", ctx),
		         std::wstring(L"open \"C:\\dir\\%N file.txt\" | name=%N file.txt"));
	}

	printf("== P2: file path contains literal %%D (contamination via %%1) ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\x\\%Dy.txt");
		CHECK_EQ(ExpandActionPlaceholders(L"%1", ctx), std::wstring(L"C:\\x\\%Dy.txt"));
	}

	printf("== P3: file name contains literal %%1 (expanded via %%N) ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\dir\\100%1.txt");
		CHECK_EQ(ExpandActionPlaceholders(L"%N", ctx), std::wstring(L"100%1.txt"));
	}

	printf("== P4: sanity - normal file with spaces ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\my docs\\readme.txt");
		CHECK_EQ(ExpandActionPlaceholders(L"%1", ctx), std::wstring(L"\"C:\\my docs\\readme.txt\""));
		CHECK_EQ(ExpandActionPlaceholders(L"%N", ctx), std::wstring(L"readme.txt"));
		CHECK_EQ(ExpandActionPlaceholders(L"%D", ctx), std::wstring(L"\"C:\\my docs\""));
	}

	printf("== P5: real command template + path containing %%N ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\tools\\run %N.bat");
		CHECK_EQ(ExpandActionPlaceholders(L"cmd.exe /c %1 --log %N", ctx),
		         std::wstring(L"cmd.exe /c \"C:\\tools\\run %N.bat\" --log run %N.bat"));
	}

	printf("== P6: multiple selection %%* ==\n");
	{
		MenuContext ctx = SingleFileContext(L"C:\\a b\\one.txt");
		SelectedItem two;
		two.path = L"C:\\a b\\two.txt";
		ctx.selected.push_back(two);
		CHECK_EQ(ExpandActionPlaceholders(L"%*", ctx), std::wstring(L"\"C:\\a b\\one.txt\" \"C:\\a b\\two.txt\""));
	}

	printf("== P7: directory background context ==\n");
	{
		MenuContext ctx;
		ctx.folderPath = L"C:\\some dir";
		CHECK_EQ(ExpandActionPlaceholders(L"%1 %D %N", ctx), std::wstring(L"\"C:\\some dir\" \"C:\\some dir\" some dir"));
	}

	printf("== P8: no selection, no folder ==\n");
	{
		MenuContext ctx;
		CHECK_EQ(ExpandActionPlaceholders(L"a %1 b %D c %N d %* e", ctx), std::wstring(L"a %1 b %D c  d  e"));
	}

	printf("== P9: literals and edge cases ==\n");
	{
		const auto ctx = SingleFileContext(L"C:\\f.txt");
		CHECK_EQ(ExpandActionPlaceholders(L"plain text", ctx), std::wstring(L"plain text"));
		CHECK_EQ(ExpandActionPlaceholders(L"trailing %", ctx), std::wstring(L"trailing %"));
		CHECK_EQ(ExpandActionPlaceholders(L"unknown %X token", ctx), std::wstring(L"unknown %X token"));
		CHECK_EQ(ExpandActionPlaceholders(L"%%1", ctx), std::wstring(L"%C:\\f.txt"));
		CHECK_EQ(ExpandActionPlaceholders(L"%n", ctx), std::wstring(L"%n")); // tokens are case sensitive, as before
	}

	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
