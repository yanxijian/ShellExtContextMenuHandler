#include "ShellLog.h"
#include <cstdarg>
#include <vector>

void ShellLog(PCWSTR format, ...)
{
    va_list args;
    va_start(args, format);

    int requiredLength = _vscwprintf(format, args);
    if (requiredLength < 0)
    {
        va_end(args);
        return;
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(requiredLength) + 1);
    vswprintf_s(buffer.data(), buffer.size(), format, args);
    va_end(args);

    OutputDebugStringW(L"[ShellExt] ");
    OutputDebugStringW(buffer.data());
    OutputDebugStringW(L"\n");
}
