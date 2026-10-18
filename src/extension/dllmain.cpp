/****************************** Module Header ******************************\
Module Name:  dllmain.cpp
Project:      CppShellExtContextMenuHandler
Copyright (c) Microsoft Corporation.

The file implements DllMain, and the DllGetClassObject, DllCanUnloadNow, 
DllRegisterServer, DllUnregisterServer functions that are necessary for a COM 
DLL. 

DllGetClassObject invokes the class factory defined in ClassFactory.h/cpp and 
queries to the specific interface.

DllCanUnloadNow checks if we can unload the component from the memory.

DllRegisterServer registers the COM server and the context menu handler in 
the registry by invoking the helper functions defined in Reg.h/cpp. The 
context menu handler is associated with the .cpp file class.

DllUnregisterServer unregisters the COM server and the context menu handler. 

This source is subject to the Microsoft Public License.
See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
All other rights reserved.

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND, 
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\***************************************************************************/

#include <windows.h>
#include <Guiddef.h>
#include <ShlObj.h>
#include <algorithm>
#include <new>
#include <string>
#include <vector>
#include "ClassFactory.h" // For the class factory
#include "PathHelpers.h"
#include "Reg.h"
#include "RegistrationConfig.h"
#include "ShellTarget.h"
#include "ShellLog.h"
#include "common.h"

// {BFD98515-CD74-48A4-98E2-13D209E3EE4F}
// When you write your own handler, you must create a new CLSID by using the
// "Create GUID" tool in the Tools menu, and specify the CLSID value here.
const CLSID CLSID_FileContextMenuExt = {0xBFD98515, 0xCD74, 0x48A4, {0x98, 0xE2, 0x13, 0xD2, 0x09, 0xE3, 0xEE, 0x4F}};

HINSTANCE g_hInst = NULL;
long g_cDllRef = 0;

namespace
{
	bool GetSiblingFilePath(PCWSTR fileName, std::wstring& path)
	{
		const std::wstring modulePath = GetModuleFilePath(g_hInst);
		if (modulePath.empty())
		{
			return false;
		}

		const size_t separator = modulePath.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
		{
			return false;
		}

		path = modulePath.substr(0, separator + 1);
		path += fileName;
		return true;
	}

	HRESULT RegisterShellTarget(ShellTargetType targetType)
	{
		const PCWSTR fileType = GetShellTargetRegistryFileType(targetType);
		return fileType == nullptr ? E_INVALIDARG
								   : RegisterShellExtContextMenuHandler(fileType, CLSID_FileContextMenuExt, L_Friendly_Menu_Name);
	}

	HRESULT UnregisterShellTarget(ShellTargetType targetType)
	{
		const PCWSTR fileType = GetShellTargetRegistryFileType(targetType);
		return fileType == nullptr ? E_INVALIDARG : UnregisterShellExtContextMenuHandler(fileType, CLSID_FileContextMenuExt);
	}

	void SetFirstFailure(HRESULT current, HRESULT& firstFailure)
	{
		if (FAILED(current) && SUCCEEDED(firstFailure))
		{
			firstFailure = current;
		}
	}
} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		// Hold the instance of this DLL module, we will use it to get the
		// path of the DLL to register the component.
		g_hInst = hModule;
		DisableThreadLibraryCalls(hModule);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

//
//   FUNCTION: DllGetClassObject
//
//   PURPOSE: Create the class factory and query to the specific interface.
//
//   PARAMETERS:
//   * rclsid - The CLSID that will associate the correct data and code.
//   * riid - A reference to the identifier of the interface that the caller
//     is to use to communicate with the class object.
//   * ppv - The address of a pointer variable that receives the interface
//     pointer requested in riid. Upon successful return, *ppv contains the
//     requested interface pointer. If an error occurs, the interface pointer
//     is NULL.
//
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
	HRESULT hr = CLASS_E_CLASSNOTAVAILABLE;

	if (IsEqualCLSID(CLSID_FileContextMenuExt, rclsid))
	{
		hr = E_OUTOFMEMORY;

		try
		{
			ClassFactory* pClassFactory = new (std::nothrow) ClassFactory();
			if (pClassFactory)
			{
				hr = pClassFactory->QueryInterface(riid, ppv);
				pClassFactory->Release();
			}
		}
		catch (...)
		{
			hr = E_FAIL;
		}
	}

	return hr;
}

//
//   FUNCTION: DllCanUnloadNow
//
//   PURPOSE: Check if we can unload the component from the memory.
//
//   NOTE: The component can be unloaded from the memory when its reference
//   count is zero (i.e. nobody is still using the component).
//
STDAPI DllCanUnloadNow(void)
{
	return g_cDllRef > 0 ? S_FALSE : S_OK;
}

//
//   FUNCTION: DllRegisterServer
//
//   PURPOSE: Register the COM server and the context menu handler.
//
STDAPI DllRegisterServer(void)
{
	std::vector<ShellTargetType> registeredTargets;
	bool inprocRegistered = false;

	try
	{
		const std::wstring modulePath = GetModuleFilePath(g_hInst);
		if (modulePath.empty())
		{
			const DWORD lastError = GetLastError();
			return HRESULT_FROM_WIN32(lastError != ERROR_SUCCESS ? lastError : ERROR_INSUFFICIENT_BUFFER);
		}

		// Validate the registration configuration before touching the registry.
		// A bad edit must not disable an existing, working installation.
		std::wstring configPath;
		std::vector<ShellTargetType> registrations;
		if (!GetSiblingFilePath(L"registration.json", configPath))
		{
			registrations = {ShellTargetType::File};
		}
		else
		{
			const ShellRegistrationConfigStatus configStatus = LoadShellRegistrationConfig(configPath, registrations);
			if (configStatus == ShellRegistrationConfigStatus::Invalid)
			{
				ShellLog(L"Invalid shell registration configuration; registry left unchanged: %s", configPath.c_str());
				return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
			}
		}

		HRESULT hr = RegisterInprocServer(modulePath.c_str(), CLSID_FileContextMenuExt, L_Friendly_Class_Name, L"Apartment");
		if (FAILED(hr))
		{
			return hr;
		}
		inprocRegistered = true;

		for (const ShellTargetType targetType : registrations)
		{
			// Track before writing so an exception between a successful
			// RegisterShellTarget call and the next loop iteration still gets
			// rolled back by the catch handler.
			registeredTargets.push_back(targetType);
			hr = RegisterShellTarget(targetType);
			if (FAILED(hr))
			{
				for (const ShellTargetType registeredTarget : registeredTargets)
				{
					UnregisterShellTarget(registeredTarget);
				}
				UnregisterInprocServer(CLSID_FileContextMenuExt);
				return hr;
			}
		}

		const ShellTargetType knownTargets[] = {ShellTargetType::File, ShellTargetType::Directory, ShellTargetType::DirectoryBackground,
												ShellTargetType::Drive, ShellTargetType::FileSystemObject};
		for (const ShellTargetType knownTarget : knownTargets)
		{
			if (std::find(registrations.begin(), registrations.end(), knownTarget) == registrations.end())
			{
				hr = UnregisterShellTarget(knownTarget);
				if (FAILED(hr))
				{
					ShellLog(L"Failed to remove legacy shell registration target; "
							 L"keeping the current registration: %08X",
							 hr);
				}
			}
		}

		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
		return S_OK;
	}
	catch (...)
	{
		// Best-effort rollback if an allocation or unexpected exception occurs
		// after the registry was modified.
		for (const ShellTargetType registeredTarget : registeredTargets)
		{
			UnregisterShellTarget(registeredTarget);
		}
		if (inprocRegistered)
		{
			UnregisterInprocServer(CLSID_FileContextMenuExt);
		}
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
		OutputDebugStringW(L"[ShellExt] Unhandled exception in DllRegisterServer\n");
		return E_FAIL;
	}
}

//
//   FUNCTION: DllUnregisterServer
//
//   PURPOSE: Unregister the COM server and the context menu handler.
//
STDAPI DllUnregisterServer(void)
{
	try
	{
		HRESULT firstFailure = S_OK;
		const ShellTargetType knownTargets[] = {ShellTargetType::File, ShellTargetType::Directory, ShellTargetType::DirectoryBackground,
												ShellTargetType::Drive, ShellTargetType::FileSystemObject};

		for (const ShellTargetType targetType : knownTargets)
		{
			SetFirstFailure(UnregisterShellTarget(targetType), firstFailure);
		}

		SetFirstFailure(UnregisterInprocServer(CLSID_FileContextMenuExt), firstFailure);
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

		return firstFailure;
	}
	catch (...)
	{
		OutputDebugStringW(L"[ShellExt] Unhandled exception in DllUnregisterServer\n");
		return E_FAIL;
	}
}