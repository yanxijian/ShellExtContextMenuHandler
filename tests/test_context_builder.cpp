// Verification harness for DragQueryFileW long-path handling in ContextBuilder.
// Includes the REAL ContextBuilder.cpp and feeds it a synthetic CF_HDROP IDataObject.
#include "../src/context/ContextBuilder.cpp"

#include <cstdio>
#include <vector>

void ShellLog(PCWSTR, ...) {}

namespace DpiProvider
{
	UINT GetSystemDpi()
	{
		return 96;
	}
} // namespace DpiProvider

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                                                                                \
	do                                                                                                                             \
	{                                                                                                                              \
		++g_total;                                                                                                                 \
		if (!(cond))                                                                                                               \
		{                                                                                                                          \
			++g_failed;                                                                                                            \
			printf("  FAIL(line %d): %s\n", __LINE__, #cond);                                                                       \
		}                                                                                                                          \
	} while (0)

class CTestDropDataObject : public IDataObject
{
public:
	explicit CTestDropDataObject(std::vector<std::wstring> files)
		: m_files(std::move(files))
	{
	}

	// IUnknown
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return 2;
	}
	ULONG STDMETHODCALLTYPE Release() override
	{
		return 1;
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
	{
		if (riid == IID_IUnknown || riid == IID_IDataObject)
		{
			*ppv = static_cast<IDataObject*>(this);
			return S_OK;
		}
		*ppv = nullptr;
		return E_NOINTERFACE;
	}

	// IDataObject
	HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override
	{
		if (pformatetcIn == nullptr || pformatetcIn->cfFormat != CF_HDROP || pformatetcIn->tymed != TYMED_HGLOBAL)
		{
			return DV_E_FORMATETC;
		}

		size_t chars = 1;
		for (const auto& file : m_files)
		{
			chars += file.size() + 1;
		}
		const size_t bytes = sizeof(DROPFILES) + chars * sizeof(wchar_t);
		HGLOBAL global = GlobalAlloc(GHND, bytes);
		if (global == nullptr)
		{
			return E_OUTOFMEMORY;
		}

		auto* dropFiles = static_cast<DROPFILES*>(GlobalLock(global));
		dropFiles->pFiles = sizeof(DROPFILES);
		dropFiles->fWide = TRUE;
		wchar_t* cursor = reinterpret_cast<wchar_t*>(dropFiles + 1);
		for (const auto& file : m_files)
		{
			wmemcpy(cursor, file.c_str(), file.size());
			cursor += file.size();
			*cursor++ = L'\0';
		}
		*cursor = L'\0';
		GlobalUnlock(global);

		pmedium->tymed = TYMED_HGLOBAL;
		pmedium->hGlobal = global; // ownership passes to ReleaseStgMedium
		pmedium->pUnkForRelease = nullptr;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC*) override
	{
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC*) override
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC**) override
	{
		return E_NOTIMPL;
	}
	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}
	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}
	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override
	{
		return OLE_E_ADVISENOTSUPPORTED;
	}

private:
	std::vector<std::wstring> m_files;
};

static std::wstring MakeLongPath(const wchar_t* fileName)
{
	// Build a path comfortably longer than MAX_PATH.
	std::wstring path = L"C:\\";
	while (path.size() < 300)
	{
		path += L"sub_directory_with_a_long_name\\";
	}
	path += fileName;
	return path;
}

int main()
{
	printf("== D1: path longer than MAX_PATH is not truncated ==\n");
	{
		const std::wstring longPath = MakeLongPath(L"long file name.txt");
		CHECK(longPath.size() > MAX_PATH);
		CTestDropDataObject dataObject({longPath});
		MenuContext context;
		const bool ok = BuildMenuContext(nullptr, &dataObject, nullptr, context);
		CHECK(ok);
		CHECK(context.GetSelectionCount() == 1);
		printf("   input length:  %zu\n", longPath.size());
		printf("   parsed length: %zu\n", context.selected.empty() ? 0 : context.selected[0].path.size());
		CHECK(context.selected[0].path == longPath);
		CHECK(context.selected[0].fileName == L"long file name");
		CHECK(context.selected[0].extension == L".txt");
	}

	printf("== D2: multiple files, mixed lengths ==\n");
	{
		const std::wstring longPath = MakeLongPath(L"deep.txt");
		CTestDropDataObject dataObject({L"C:\\short\\a.txt", longPath, L"b.log"});
		MenuContext context;
		const bool ok = BuildMenuContext(nullptr, &dataObject, nullptr, context);
		CHECK(ok);
		CHECK(context.GetSelectionCount() == 3);
		CHECK(context.selected[0].path == L"C:\\short\\a.txt");
		CHECK(context.selected[1].path == longPath);
		CHECK(context.selected[2].fileName == L"b");
		CHECK(context.selected[2].extension == L".log");
		CHECK(context.hasFiles);
		CHECK(!context.hasFolders);
		CHECK(context.targetType == ShellTargetType::File);
	}

	printf("== D3: normal short paths still work ==\n");
	{
		CTestDropDataObject dataObject({L"C:\\dir\\plain.txt"});
		MenuContext context;
		const bool ok = BuildMenuContext(nullptr, &dataObject, nullptr, context);
		CHECK(ok);
		CHECK(context.selected[0].path == L"C:\\dir\\plain.txt");
		CHECK(context.selected[0].fileName == L"plain");
	}

	printf("\n%d/%d checks passed\n", g_total - g_failed, g_total);
	return g_failed == 0 ? 0 : 1;
}
