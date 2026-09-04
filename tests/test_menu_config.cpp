// Verification harness for the nlohmann/json-based MenuConfig parser.
// Includes the REAL MenuConfig.cpp and drives it through its public API.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <windows.h>

#include "../src/menu/MenuConfig.cpp"

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

static std::wstring WriteTempJson(const wchar_t* name, const char* json)
{
	static const std::wstring directory = []() {
		wchar_t tempDir[MAX_PATH] = {};
		GetTempPathW(MAX_PATH, tempDir);
		const std::filesystem::path path = std::wstring(tempDir) + L"shellext-tests/menu_config";
		std::error_code error;
		std::filesystem::create_directories(path, error);
		return path.wstring();
	}();

	std::wstring path = directory + L"\\" + name;
	FILE* f = nullptr;
	_wfopen_s(&f, path.c_str(), L"wb");
	if (f)
	{
		fwrite(json, 1, strlen(json), f);
		fclose(f);
	}
	return path;
}

static void PrintVec(const char* name, const std::vector<std::wstring>& v)
{
	printf("   %s = [", name);
	for (size_t i = 0; i < v.size(); ++i)
	{
		printf("%ls%ls", v[i].c_str(), i + 1 < v.size() ? L", " : L"");
	}
	printf("]\n");
}

int main()
{
	printf("== C1: escaped quotes inside a string value (T1) ==\n");
	{
		const auto path = WriteTempJson(L"c1.json", R"({"menuItems":[{"id":"a","label":"say \"hi\" done","verb":"va"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items.size() == 1);
		CHECK(doc.items[0].label == L"say \"hi\" done");
	}

	printf("== C2: earlier string value containing a later key name, valid JSON (T2b) ==\n");
	{
		const auto path = WriteTempJson(
			L"c2.json",
			R"({"menuItems":[{"id":"a","label":"A","verb":"realverb","helpText":"broken \"verb\" here"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items[0].verb == L"realverb");
		CHECK(doc.items[0].helpText == L"broken \"verb\" here");
	}

	printf("== C3: menuItems before root keys; item-level itemGates must not pollute root (T3) ==\n");
	{
		const auto path = WriteTempJson(
			L"c3.json",
			R"({"menuItems":[{"id":"a","label":"A","verb":"va","itemGates":["demo:hideTemp"]}],"itemGates":["customGate"]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		PrintVec("root itemGates", doc.globalChains.itemGates);
		CHECK(doc.globalChains.itemGates.size() == 1); // config fully replaces default jsonFilter
		CHECK(doc.globalChains.itemGates[0] == L"customGate");
		CHECK(doc.items[0].itemGates.size() == 1);
		CHECK(doc.items[0].itemGates[0] == L"demo:hideTemp");
	}

	printf("== C4: braces inside item string values (T4) ==\n");
	{
		const auto path = WriteTempJson(
			L"c4.json",
			R"({"menuItems":[{"id":"a","label":"A","verb":"va","helpText":"use } brace {"},{"id":"b","label":"B","verb":"vb"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items.size() == 2);
		CHECK(doc.items[0].helpText == L"use } brace {");
		CHECK(doc.items[1].id == L"b");
	}

	printf("== C5: ']' inside extension array element (T5) ==\n");
	{
		const auto path = WriteTempJson(
			L"c5.json",
			R"({"menuItems":[{"id":"a","label":"A","verb":"va","extensions":[".c]pp",".txt"]}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items[0].filter.extensions.size() == 2);
		CHECK(doc.items[0].filter.extensions[0] == L".c]pp");
		CHECK(doc.items[0].filter.extensions[1] == L".txt");
	}

	printf("== C6: minSelection validation (T6) ==\n");
	{
		const auto p1 = WriteTempJson(L"c6a.json", R"({"menuItems":[{"id":"a","label":"A","verb":"va","minSelection":"abc"}]})");
		const auto p2 = WriteTempJson(L"c6b.json", R"({"menuItems":[{"id":"a","label":"A","verb":"va","minSelection":-5}]})");
		const auto p3 = WriteTempJson(L"c6c.json", R"({"menuItems":[{"id":"a","label":"A","verb":"va","minSelection":5000000000}]})");
		const auto p4 = WriteTempJson(L"c6d.json", R"({"menuItems":[{"id":"a","label":"A","verb":"va","minSelection":3}]})");
		MenuConfigDocument doc;
		CHECK(LoadMenuConfigDocument(p1, doc) && doc.items[0].filter.minSelection == 1);
		CHECK(LoadMenuConfigDocument(p2, doc) && doc.items[0].filter.minSelection == 1);
		CHECK(LoadMenuConfigDocument(p3, doc) && doc.items[0].filter.minSelection == 1);
		CHECK(LoadMenuConfigDocument(p4, doc) && doc.items[0].filter.minSelection == 3);
	}

	printf("== C7: invalid JSON document -> builtin fallback ==\n");
	{
		const auto path = WriteTempJson(L"c7.json", "{ nope");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(!ok);
		CHECK(doc.items.size() == 1); // builtin item
	}

	printf("== C8: config file not found -> builtin fallback ==\n");
	{
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(L"C:\\definitely\\missing\\menu.json", doc);
		CHECK(!ok);
		CHECK(doc.items.size() == 1);
	}

	printf("== C9: empty menuItems array -> builtin fallback ==\n");
	{
		const auto path = WriteTempJson(L"c9.json", R"({"menuItems":[]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(!ok);
		CHECK(doc.items.size() == 1);
	}

	printf("== C10: shipped config/menu.json (T7 fixed: no duplicated chains) ==\n");
	{
		MenuConfigDocument doc;
		// menu.json is copied next to the test binary by the build.
		const bool ok = LoadMenuConfigDocument(L"menu.json", doc);
		CHECK(ok);
		CHECK(doc.items.size() == 5);
		PrintVec("root itemGates", doc.globalChains.itemGates);
		PrintVec("root executors", doc.globalChains.executors);
		CHECK(doc.globalChains.itemGates.size() == 1); // exactly the config value
		CHECK(doc.globalChains.itemGates[0] == L"jsonFilter");
		CHECK(doc.globalChains.executors.size() == 1);
		CHECK(doc.globalChains.executors[0] == L"messageBox");
	}

	printf("== C11: defaults fill in for unspecified fields ==\n");
	{
		const auto path = WriteTempJson(L"c11.json", R"({"menuItems":[{"id":"a","label":"MyLabel","verb":"myverb","targets":["file"]}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items[0].helpText == L"MyLabel");     // helpText defaults to label
		CHECK(doc.items[0].canonicalName == L"myverb"); // canonicalName defaults to verb
		CHECK(doc.items[0].targets.size() == 1);
	}

	printf("== C12: item with unknown target name is skipped ==\n");
	{
		const auto path = WriteTempJson(
			L"c12.json",
			R"({"menuItems":[{"id":"a","label":"A","verb":"va","targets":["bogus"]},{"id":"b","label":"B","verb":"vb"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.items.size() == 1);
		CHECK(doc.items[0].id == L"b");
	}

	printf("== C13: config executors replace defaults (H2) ==\n");
	{
		const auto path = WriteTempJson(
			L"c13.json",
			R"({"executors":["launch"],"menuItems":[{"id":"a","label":"A","verb":"va"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		PrintVec("root executors", doc.globalChains.executors);
		CHECK(doc.globalChains.executors.size() == 1);
		CHECK(doc.globalChains.executors[0] == L"launch");
		CHECK(doc.globalChains.itemGates.size() == 1); // unspecified chain still gets its default
		CHECK(doc.globalChains.itemGates[0] == L"jsonFilter");
	}

	printf("== C14: legacy 'gates' root alias still works ==\n");
	{
		const auto path = WriteTempJson(
			L"c14.json",
			R"({"gates":["legacyGate"],"menuItems":[{"id":"a","label":"A","verb":"va"}]})");
		MenuConfigDocument doc;
		const bool ok = LoadMenuConfigDocument(path, doc);
		CHECK(ok);
		CHECK(doc.globalChains.itemGates.size() == 1);
		CHECK(doc.globalChains.itemGates[0] == L"legacyGate");
	}

	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
