#include "FileContextMenuExt.h"

#include "ComGuard.h"
#include "DpiProvider.h"
#include "IconProviderRegistry.h"

#include <Shlwapi.h>
#include <strsafe.h>

#include <string>

#pragma comment(lib, "shlwapi.lib")

extern HINSTANCE g_hInst;
extern long g_cDllRef;

namespace
{
	bool TryAnsiToWide(PCSTR text, std::wstring& wide)
	{
		if (text == nullptr || *text == '\0')
		{
			return false;
		}

		const int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
		if (length <= 1)
		{
			return false;
		}

		std::wstring buffer(static_cast<size_t>(length), L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, text, -1, buffer.data(), length) != length)
		{
			return false;
		}

		buffer.resize(static_cast<size_t>(length - 1));
		wide.swap(buffer);
		return true;
	}
} // namespace

FileContextMenuExt::FileContextMenuExt(void)
	: m_cRef(1)
	, m_menuProvider(g_hInst)
{
	InterlockedIncrement(&g_cDllRef);
}

FileContextMenuExt::~FileContextMenuExt(void)
{
	ReleaseItemBitmaps();
	InterlockedDecrement(&g_cDllRef);
}

void FileContextMenuExt::ReleaseItemBitmaps()
{
	for (HBITMAP bitmap : m_itemBitmaps)
	{
		if (bitmap != nullptr)
		{
			DeleteObject(bitmap);
		}
	}

	m_itemBitmaps.clear();
}

HBITMAP FileContextMenuExt::LoadItemBitmap(const std::wstring& iconSpec, UINT dpi)
{
	HBITMAP bitmap = IconProviderRegistry::Instance().LoadMenuItemIcon(iconSpec, dpi, g_hInst);
	return bitmap;
}

IFACEMETHODIMP FileContextMenuExt::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[] = {
		QITABENT(FileContextMenuExt, IContextMenu),
		QITABENT(FileContextMenuExt, IShellExtInit),
		{0},
	};
	return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) FileContextMenuExt::AddRef()
{
	return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) FileContextMenuExt::Release()
{
	ULONG cRef = InterlockedDecrement(&m_cRef);
	if (0 == cRef)
	{
		delete this;
	}

	return cRef;
}

IFACEMETHODIMP FileContextMenuExt::Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID)
{
	return GuardCom(L"Initialize",
					[&]() { return m_menuProvider.Initialize(pidlFolder, pDataObj, hKeyProgID); });
}

IFACEMETHODIMP FileContextMenuExt::QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
	return GuardCom(L"QueryContextMenu",
					[&]() -> HRESULT
					{
						if (CMF_DEFAULTONLY & uFlags)
						{
							return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
						}

						const UINT menuDpi = DpiProvider::GetDpiAtCursor();
						ReleaseItemBitmaps();

						m_menuProvider.BuildInsertedItems(m_insertedItems);
						if (m_insertedItems.empty())
						{
							return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
						}

						m_itemBitmaps.reserve(m_insertedItems.size());

						UINT menuOffset = 0;
						UINT insertIndex = indexMenu;
						for (size_t i = 0; i < m_insertedItems.size(); ++i)
						{
							if (idCmdFirst + menuOffset > idCmdLast)
							{
								break;
							}

							const InsertedMenuItem& inserted = m_insertedItems[i];
							HBITMAP itemBitmap = LoadItemBitmap(inserted.item.icon, menuDpi);
							m_itemBitmaps.push_back(itemBitmap);

							MENUITEMINFOW menuInfo = {sizeof(menuInfo)};
							menuInfo.fMask = MIIM_BITMAP | MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
							menuInfo.wID = idCmdFirst + menuOffset;
							menuInfo.fType = MFT_STRING;
							menuInfo.dwTypeData = const_cast<PWSTR>(inserted.item.label.c_str());
							menuInfo.fState = inserted.state == MenuItemState::Disabled ? MFS_DISABLED : MFS_ENABLED;
							menuInfo.hbmpItem = itemBitmap;

							if (!InsertMenuItemW(hMenu, insertIndex, TRUE, &menuInfo))
							{
								return HRESULT_FROM_WIN32(GetLastError());
							}

							++insertIndex;
							++menuOffset;

							if (inserted.item.separatorAfter && i + 1 < m_insertedItems.size())
							{
								MENUITEMINFOW separator = {sizeof(separator)};
								separator.fMask = MIIM_TYPE;
								separator.fType = MFT_SEPARATOR;
								if (!InsertMenuItemW(hMenu, insertIndex, TRUE, &separator))
								{
									return HRESULT_FROM_WIN32(GetLastError());
								}
								++insertIndex;
							}
						}

						return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(menuOffset));
					});
}

IFACEMETHODIMP FileContextMenuExt::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
	return GuardCom(L"InvokeCommand",
					[&]() -> HRESULT
					{
						if (pici == nullptr)
						{
							return E_INVALIDARG;
						}

						const bool isUnicode =
							pici->cbSize >= sizeof(CMINVOKECOMMANDINFOEX) && (pici->fMask & CMIC_MASK_UNICODE) != 0;
						const auto* piciEx = reinterpret_cast<const CMINVOKECOMMANDINFOEX*>(pici);

						const InsertedMenuItem* item = nullptr;
						if (isUnicode && piciEx->lpVerbW == nullptr)
						{
							return E_INVALIDARG;
						}
						if (!isUnicode && pici->lpVerb == nullptr)
						{
							return E_INVALIDARG;
						}

						if (isUnicode && HIWORD(piciEx->lpVerbW) != 0)
						{
							if (!m_menuProvider.TryGetInsertedItemByVerb(piciEx->lpVerbW, &item))
							{
								return E_FAIL;
							}
						}
						else if (!isUnicode && HIWORD(pici->lpVerb) != 0)
						{
							std::wstring verb;
							if (!TryAnsiToWide(pici->lpVerb, verb) || !m_menuProvider.TryGetInsertedItemByVerb(verb.c_str(), &item))
							{
								return E_FAIL;
							}
						}
						else
						{
							const UINT commandOffset = isUnicode ? LOWORD(piciEx->lpVerbW) : LOWORD(pici->lpVerb);
							if (!m_menuProvider.TryGetInsertedItemByCommandOffset(commandOffset, &item))
							{
								return E_FAIL;
							}
						}

						m_menuProvider.ExecuteItem(*item, pici->hwnd);
						return S_OK;
					});
}

IFACEMETHODIMP FileContextMenuExt::GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
	return GuardCom(L"GetCommandString",
					[&]() -> HRESULT
					{
						UNREFERENCED_PARAMETER(pwReserved);

						const InsertedMenuItem* item = nullptr;
						if (!m_menuProvider.TryGetInsertedItemByCommandOffset(static_cast<UINT>(idCommand), &item))
						{
							return E_INVALIDARG;
						}

						switch (uFlags)
						{
						case GCS_HELPTEXTW:
							return StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax, item->item.helpText.c_str());

						case GCS_VERBW:
							return StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax, item->item.canonicalName.c_str());

						case GCS_HELPTEXTA:
						{
							const int written =
								WideCharToMultiByte(CP_ACP, 0, item->item.helpText.c_str(), -1, pszName, cchMax, nullptr, nullptr);
							return written > 0 ? S_OK : HRESULT_FROM_WIN32(GetLastError());
						}

						case GCS_VERBA:
						{
							const int written = WideCharToMultiByte(CP_ACP, 0, item->item.canonicalName.c_str(), -1, pszName, cchMax,
																	nullptr, nullptr);
							return written > 0 ? S_OK : HRESULT_FROM_WIN32(GetLastError());
						}

						default:
							return S_OK;
						}
					});
}
