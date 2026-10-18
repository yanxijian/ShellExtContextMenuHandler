#pragma once
#include <windows.h>

// COM methods must never let a C++ exception cross the ABI boundary. Explorer
// calls these methods without a C++ catch frame, so an escaping exception
// terminates the host process.
template <typename Callable>
HRESULT GuardCom(const wchar_t* operation, Callable&& callable) noexcept
{
	try
	{
		return callable();
	}
	catch (...)
	{
		// Do not allocate or format here: the process may already be out of
		// memory. OutputDebugStringW is the only safe diagnostic.
		OutputDebugStringW(L"[ShellExt] Unhandled exception in ");
		OutputDebugStringW(operation);
		OutputDebugStringW(L"\n");
		return E_FAIL;
	}
}
