#include "FileContextMenuExt.h"
#include "DpiProvider.h"
#include "resource.h"
#include <strsafe.h>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

extern HINSTANCE g_hInst;
extern long g_cDllRef;

FileContextMenuExt::FileContextMenuExt(void)
    : m_cRef(1),
    m_menuProvider(g_hInst),
    m_hMenuBmp(nullptr),
    m_cachedIconDpi(0)
{
    InterlockedIncrement(&g_cDllRef);
}

FileContextMenuExt::~FileContextMenuExt(void)
{
    if (m_hMenuBmp)
    {
        DeleteObject(m_hMenuBmp);
        m_hMenuBmp = nullptr;
    }

    InterlockedDecrement(&g_cDllRef);
}

void FileContextMenuExt::EnsureMenuBitmap(UINT dpi)
{
    if (m_hMenuBmp != nullptr && m_cachedIconDpi == dpi)
    {
        return;
    }

    if (m_hMenuBmp != nullptr)
    {
        DeleteObject(m_hMenuBmp);
        m_hMenuBmp = nullptr;
    }

    m_hMenuBmp = DpiProvider::LoadMenuBitmapScaled(
        g_hInst,
        MAKEINTRESOURCEW(IDB_OK),
        dpi);
    m_cachedIconDpi = dpi;
}

IFACEMETHODIMP FileContextMenuExt::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(FileContextMenuExt, IContextMenu),
        QITABENT(FileContextMenuExt, IShellExtInit),
        { 0 },
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

IFACEMETHODIMP FileContextMenuExt::Initialize(
    LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID)
{
    return m_menuProvider.Initialize(pidlFolder, pDataObj, hKeyProgID);
}

IFACEMETHODIMP FileContextMenuExt::QueryContextMenu(
    HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    if (CMF_DEFAULTONLY & uFlags)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    const UINT menuDpi = DpiProvider::GetDpiAtCursor();
    EnsureMenuBitmap(menuDpi);

    m_menuProvider.BuildInsertedItems(m_insertedItems);
    if (m_insertedItems.empty())
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    UINT menuOffset = 0;
    UINT insertIndex = indexMenu;
    for (size_t i = 0; i < m_insertedItems.size(); ++i)
    {
        if (idCmdFirst + menuOffset > idCmdLast)
        {
            break;
        }

        const InsertedMenuItem& inserted = m_insertedItems[i];
        MENUITEMINFOW menuInfo = { sizeof(menuInfo) };
        menuInfo.fMask = MIIM_BITMAP | MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
        menuInfo.wID = idCmdFirst + menuOffset;
        menuInfo.fType = MFT_STRING;
        menuInfo.dwTypeData = const_cast<PWSTR>(inserted.item.label.c_str());
        menuInfo.fState = inserted.state == MenuItemState::Disabled ? MFS_DISABLED : MFS_ENABLED;
        menuInfo.hbmpItem = static_cast<HBITMAP>(m_hMenuBmp);

        if (!InsertMenuItemW(hMenu, insertIndex, TRUE, &menuInfo))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        ++insertIndex;
        ++menuOffset;

        if (inserted.item.separatorAfter)
        {
            MENUITEMINFOW separator = { sizeof(separator) };
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
}

IFACEMETHODIMP FileContextMenuExt::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    if (pici == nullptr)
    {
        return E_INVALIDARG;
    }

    BOOL isUnicode = FALSE;
    if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX)
        && (pici->fMask & CMIC_MASK_UNICODE))
    {
        isUnicode = TRUE;
    }

    const InsertedMenuItem* item = nullptr;
    if (!isUnicode && HIWORD(pici->lpVerb))
    {
        if (!m_menuProvider.TryGetInsertedItemByVerb(
            reinterpret_cast<PCWSTR>(pici->lpVerb), &item))
        {
            return E_FAIL;
        }
    }
    else if (isUnicode && HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW))
    {
        if (!m_menuProvider.TryGetInsertedItemByVerb(
            ((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW, &item))
        {
            return E_FAIL;
        }
    }
    else
    {
        if (!m_menuProvider.TryGetInsertedItemByCommandOffset(LOWORD(pici->lpVerb), &item))
        {
            return E_FAIL;
        }
    }

    m_menuProvider.ExecuteItem(*item, pici->hwnd);
    return S_OK;
}

IFACEMETHODIMP FileContextMenuExt::GetCommandString(UINT_PTR idCommand,
    UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax)
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

    default:
        return S_OK;
    }
}