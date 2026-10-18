#include <windows.h>
#include <cstdio>

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		return 2;
	}

	wchar_t self[1024] = {};
	if (GetModuleFileNameW(nullptr, self, 1024) == 0)
	{
		return 3;
	}

	FILE* f = nullptr;
	if (_wfopen_s(&f, argv[1], L"w") != 0 || f == nullptr)
	{
		return 4;
	}

	fwprintf(f, L"%s\n", self);
	fwprintf(f, L"argc=%d\n", argc);
	for (int i = 0; i < argc; ++i)
	{
		fwprintf(f, L"argv[%d]=%s\n", i, argv[i]);
	}
	fclose(f);
	return 0;
}
