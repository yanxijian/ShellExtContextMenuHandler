// Verification for the nlohmann/json-based registration parser.
// Compiles the REAL RegistrationConfig.cpp so shipped parsing is exercised.
#include "../src/registry/RegistrationConfig.cpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

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
	wchar_t tempDir[MAX_PATH] = {};
	GetTempPathW(MAX_PATH, tempDir);
	const std::filesystem::path dir = std::wstring(tempDir) + L"shellext-tests/registration_config";
	std::error_code error;
	std::filesystem::create_directories(dir, error);

	const std::wstring path = (dir / name).wstring();
	FILE* file = nullptr;
	if (_wfopen_s(&file, path.c_str(), L"wb") == 0 && file != nullptr)
	{
		fwrite(json, 1, strlen(json), file);
		fclose(file);
	}
	return path;
}

int main()
{
	printf("== R1: valid registration config ==\n");
	{
		std::vector<ShellTargetType> registrations;
		const auto status = LoadShellRegistrationConfig(
			WriteTempJson(L"normal.json", R"({"schemaVersion":1,"shellRegistrations":["file","directory"]})"), registrations);
		CHECK(status == ShellRegistrationConfigStatus::Loaded);
		CHECK(registrations.size() == 2);
		CHECK(registrations[0] == ShellTargetType::File);
		CHECK(registrations[1] == ShellTargetType::Directory);
	}

	printf("== R2: malformed JSON is rejected ==\n");
	{
		std::vector<ShellTargetType> registrations;
		const auto status = LoadShellRegistrationConfig(WriteTempJson(L"broken.json", "{ nope"), registrations);
		CHECK(status == ShellRegistrationConfigStatus::Invalid);
		CHECK(registrations.empty());
	}

	printf("== R3: non-numeric schemaVersion is rejected ==\n");
	{
		std::vector<ShellTargetType> registrations;
		const auto status = LoadShellRegistrationConfig(
			WriteTempJson(L"trailing.json", R"({"schemaVersion":1abc,"shellRegistrations":["file"]})"), registrations);
		CHECK(status == ShellRegistrationConfigStatus::Invalid);
	}

	printf("== R4: unknown and duplicate targets are rejected ==\n");
	{
		std::vector<ShellTargetType> registrations;
		const auto unknown = LoadShellRegistrationConfig(
			WriteTempJson(L"unknown.json", R"({"schemaVersion":1,"shellRegistrations":["bogus"]})"), registrations);
		CHECK(unknown == ShellRegistrationConfigStatus::Invalid);

		const auto duplicate = LoadShellRegistrationConfig(
			WriteTempJson(L"duplicate.json", R"({"schemaVersion":1,"shellRegistrations":["file","file"]})"), registrations);
		CHECK(duplicate == ShellRegistrationConfigStatus::Invalid);
	}

	printf("== R5: missing file falls back to file target ==\n");
	{
		std::vector<ShellTargetType> registrations;
		const auto status =
			LoadShellRegistrationConfig(L"C:\\definitely\\missing\\registration.json", registrations);
		CHECK(status == ShellRegistrationConfigStatus::Missing);
		CHECK(registrations.size() == 1);
		CHECK(registrations[0] == ShellTargetType::File);
	}

	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
