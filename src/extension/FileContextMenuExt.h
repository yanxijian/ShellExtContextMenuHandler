#pragma once

#include <windows.h>
#include <shlobj.h>
#include <vector>
#include "MenuProvider.h"

class FileContextMenuExt : public IShellExtInit, public IContextMenu
{
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID);
    IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici);
    IFACEMETHODIMP GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT *pwReserved, LPSTR pszName, UINT cchMax);

    FileContextMenuExt(void);

protected:
    ~FileContextMenuExt(void);

private:
    long m_cRef;
    MenuProvider m_menuProvider;
    std::vector<InsertedMenuItem> m_insertedItems;
    HANDLE m_hMenuBmp;
};
