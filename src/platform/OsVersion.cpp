#include "OsVersion.h"

namespace
{
	using RtlGetVersionFn = LONG(WINAPI*)(void* versionInfo);

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
		static const OsVersionInfo cached = []()
		{
			OsVersionInfo info;
			HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
			if (ntdll == nullptr)
			{
				return info;
			}

			const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
			if (rtlGetVersion == nullptr)
			{
				return info;
			}

			RtlOsVersionInfo version = {};
			version.osVersionInfoSize = sizeof(version);
			if (rtlGetVersion(&version) != 0)
			{
				return info;
			}

			info.major = version.majorVersion;
			info.minor = version.minorVersion;
			info.build = version.buildNumber;
			return info;
		}();

		return cached;
	}
} // namespace

const OsVersionInfo& GetOsVersion()
{
	return QueryOsVersion();
}

bool IsWindows7OrGreater()
{
	const OsVersionInfo& version = GetOsVersion();
	return version.major > 6 || (version.major == 6 && version.minor >= 1);
}

bool IsWindows81OrGreater()
{
	const OsVersionInfo& version = GetOsVersion();
	return version.major > 6 || (version.major == 6 && version.minor >= 3);
}

bool IsWindows10OrGreater()
{
	const OsVersionInfo& version = GetOsVersion();
	return version.major >= 10;
}