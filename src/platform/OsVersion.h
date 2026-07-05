#pragma once

#include <windows.h>

struct OsVersionInfo
{
    DWORD major = 0;
    DWORD minor = 0;
    DWORD build = 0;
};

const OsVersionInfo& GetOsVersion();
bool IsWindows7OrGreater();
bool IsWindows81OrGreater();
bool IsWindows10OrGreater();