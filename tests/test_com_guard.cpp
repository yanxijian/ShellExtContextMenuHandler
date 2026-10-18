// Verifies that COM entry-point guards stop C++ exceptions at the boundary.
#include "../src/extension/ComGuard.h"

#include <cstdio>
#include <stdexcept>

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

int main()
{
	CHECK(GuardCom(L"success", []() { return S_OK; }) == S_OK);
	CHECK(GuardCom(L"std exception", []() -> HRESULT { throw std::runtime_error("boom"); }) == E_FAIL);
	CHECK(GuardCom(L"foreign exception", []() -> HRESULT { throw 42; }) == E_FAIL);

	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
