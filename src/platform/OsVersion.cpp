#include "OsVersion.h"

namespace
{
    using RtlGetVersionFn = LONG (WINAPI*)(void* versionInfo);

    struct RtlOsVersionInfo
    {
        ULONG osVersionInfoSize;
        ULONG majorVersion;
        ULONG minorVersion;
        ULONG buildNumber;
        ULONG platformId;
        WCHAR servicePack[128];
    };

    const OsVersionInfo& QueryOsVersion()
    {
        static OsVersionInfo cached;
        static bool initialized = false;
        if (initialized)
        {
            return cached;
        }

        initialized = true;
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll == nullptr)
        {
            return cached;
        }

        const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (rtlGetVersion == nullptr)
        {
            return cached;
        }

        RtlOsVersionInfo version = {};
        version.osVersionInfoSize = sizeof(version);
        if (rtlGetVersion(&version) != 0)
        {
            return cached;
        }

        cached.major = version.majorVersion;
        cached.minor = version.minorVersion;
        cached.build = version.buildNumber;
        return cached;
    }
}

const OsVersionInfo& GetOsVersion()
{
    return QueryOsVersion();
}

bool IsWindows7OrGreater()
{
    const OsVersionInfo& version = GetOsVersion();
    return version.major > 6
        || (version.major == 6 && version.minor >= 1);
}

bool IsWindows81OrGreater()
{
    const OsVersionInfo& version = GetOsVersion();
    return version.major > 6
        || (version.major == 6 && version.minor >= 3);
}

bool IsWindows10OrGreater()
{
    const OsVersionInfo& version = GetOsVersion();
    return version.major >= 10;
}