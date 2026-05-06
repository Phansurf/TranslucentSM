// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include "VisualTreeWatcher.h"
#include "Helpers.h"

static HMODULE g_hModule = nullptr;

LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static DWORD WINAPI SelfInitThread(LPVOID)
{
	Sleep(200);

	HMODULE wux = LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!wux)
	{
		HANDLE h = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
		if (h) { SetEvent(h); CloseHandle(h); }
		return 1;
	}

	typedef HRESULT(*PFN_InitXamlDiag)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, CLSID, LPCWSTR);
	auto pfn = reinterpret_cast<PFN_InitXamlDiag>(GetProcAddress(wux, "InitializeXamlDiagnosticsEx"));
	if (!pfn)
	{
		HANDLE h = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
		if (h) { SetEvent(h); CloseHandle(h); }
		return 2;
	}

	wchar_t dllPath[MAX_PATH];
	GetModuleFileNameW(g_hModule, dllPath, MAX_PATH);

	DWORD pid = GetCurrentProcessId();
	static constexpr GUID tapClsid = { 0x36162bd3, 0x3531, 0x4131, { 0x9b, 0x8b, 0x7f, 0xb1, 0xa9, 0x91, 0xef, 0x51 } };

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	HRESULT hr = E_FAIL;
	for (int attempt = 0; attempt < 60 && FAILED(hr); ++attempt)
	{
		if (attempt > 0) Sleep(500);

		wchar_t connName[64];
		swprintf_s(connName, L"VisualDiagConnection%d", attempt + 1);

		hr = pfn(connName, pid, nullptr, dllPath, tapClsid, nullptr);
	}

	HANDLE hReady = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
	if (hReady)
	{
		SetEvent(hReady);
		CloseHandle(hReady);
	}
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		g_hModule = hModule;

		HANDLE hStart = OpenEventW(EVENT_ALL_ACCESS, FALSE, L"TSM_ExplorerDiag_Start");
		if (hStart)
		{
			CloseHandle(hStart);

			wchar_t hostPath[MAX_PATH];
			if (GetModuleFileNameW(NULL, hostPath, MAX_PATH))
			{
				std::wstring path(hostPath);
				for (auto& c : path) c = towlower(c);
				if (path.find(L"explorer.exe") != std::wstring::npos)
				{
					HANDLE hThread = CreateThread(nullptr, 0, SelfInitThread, nullptr, 0, nullptr);
					if (hThread) CloseHandle(hThread);
				}
			}
		}
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

struct StartTAP : winrt::implements<StartTAP, IObjectWithSite>
{
	HRESULT STDMETHODCALLTYPE SetSite(IUnknown* pUnkSite) noexcept override
	{
		site.copy_from(pUnkSite);
		com_ptr<IXamlDiagnostics> diag = site.as<IXamlDiagnostics>();
		com_ptr<VisualTreeWatcher> treeWatcher = winrt::make_self<VisualTreeWatcher>(site);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetSite(REFIID riid, void** ppvSite) noexcept override
	{
		return site.as(riid, ppvSite);
	}

private:
	winrt::com_ptr<IUnknown> site;
};
struct TAPFactory : winrt::implements<TAPFactory, IClassFactory>
{
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject) override try
	{
		*ppvObject = nullptr;

		if (pUnkOuter)
		{
			return CLASS_E_NOAGGREGATION;
		}

		return winrt::make<StartTAP>().as(riid, ppvObject);
	}
	catch (...)
	{
		return winrt::to_hresult();
	}

	HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override
	{
		return S_OK;
	}
};
_Use_decl_annotations_ STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) try
{
	*ppv = nullptr;

	// TapFactory
	// {36162BD3-3531-4131-9B8B-7FB1A991EF51}
	static constexpr GUID tapFactory =
	{ 0x36162bd3, 0x3531, 0x4131, { 0x9b, 0x8b, 0x7f, 0xb1, 0xa9, 0x91, 0xef, 0x51 } };

	// {F3454DD1-B68F-4196-8571-2260F107A47B}
	static const GUID shellExt =
	{ 0xf3454dd1, 0xb68f, 0x4196, { 0x85, 0x71, 0x22, 0x60, 0xf1, 0x7, 0xa4, 0x7b } };

	if (rclsid == tapFactory)
	{
		return winrt::make<TAPFactory>().as(riid, ppv);
	}
	else if (rclsid == shellExt)
	{

	}
	else
	{
		return CLASS_E_CLASSNOTAVAILABLE;
	}
}
catch (...)
{
	return winrt::to_hresult();
}
_Use_decl_annotations_ STDAPI DllCanUnloadNow(void)
{
	if (winrt::get_module_lock())
	{
		return S_FALSE;
	}
	else
	{
		return S_OK;
	}
}
