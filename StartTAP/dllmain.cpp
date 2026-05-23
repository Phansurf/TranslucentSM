// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include "VisualTreeWatcher.h"
#include "Helpers.h"
#include <atomic>

static HMODULE g_hModule = nullptr;
static std::atomic<bool> g_initStarted(false);
static std::atomic<bool> g_initDone(false);

// Called by the WH_CALLWNDPROC hook in the TARGET process.
// This runs AFTER DllMain has returned and the loader lock is released,
// so it's safe to create threads and load additional DLLs.
LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (g_initDone.load())
	{
		HANDLE hReady = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
		if (hReady)
		{
			SetEvent(hReady);
			CloseHandle(hReady);
		}
		return CallNextHookEx(nullptr, nCode, wParam, lParam);
	}

	bool expected = false;
	if (g_initStarted.compare_exchange_strong(expected, true))
	{
		wchar_t hostPath[MAX_PATH];
		GetModuleFileNameW(NULL, hostPath, MAX_PATH);
		std::wstring path(hostPath);
		for (auto& c : path) c = towlower(c);

		bool isExplorer = (path.find(L"explorer.exe") != std::wstring::npos);
		bool isShellExperience = (path.find(L"shellexperiencehost.exe") != std::wstring::npos);

		if (isExplorer || isShellExperience)
		{
			HANDLE hThread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
				Sleep(200);

				HMODULE wux = LoadLibraryExW(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
				if (!wux)
				{
					g_initStarted.store(false);
					HANDLE h = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
					if (h) { SetEvent(h); CloseHandle(h); }
					return 1;
				}

				typedef HRESULT(*PFN_InitXamlDiag)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, CLSID, LPCWSTR);
				auto pfn = reinterpret_cast<PFN_InitXamlDiag>(GetProcAddress(wux, "InitializeXamlDiagnosticsEx"));
				if (!pfn)
				{
					g_initStarted.store(false);
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
					if (SUCCEEDED(hr))
					{
						g_initDone.store(true);
					}
				}

				if (FAILED(hr))
				{
					g_initStarted.store(false);
				}

				HANDLE hReady = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"TSM_ExplorerDiag_Ready");
				if (hReady)
				{
					SetEvent(hReady);
					CloseHandle(hReady);
				}
				return 0;
			}, nullptr, 0, nullptr);

			if (hThread) {
				CloseHandle(hThread);
			} else {
				g_initStarted.store(false);
			}
		}
		else
		{
			g_initStarted.store(false);
		}
	}

	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		g_hModule = hModule;
		break;
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
		if (pUnkOuter) return CLASS_E_NOAGGREGATION;
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
	static constexpr GUID tapFactory =
	{ 0x36162bd3, 0x3531, 0x4131, { 0x9b, 0x8b, 0x7f, 0xb1, 0xa9, 0x91, 0xef, 0x51 } };

	if (rclsid == tapFactory)
	{
		return winrt::make<TAPFactory>().as(riid, ppv);
	}
	return CLASS_E_CLASSNOTAVAILABLE;
}
catch (...)
{
	return winrt::to_hresult();
}
_Use_decl_annotations_ STDAPI DllCanUnloadNow(void)
{
	if (winrt::get_module_lock())
		return S_FALSE;
	else
		return S_OK;
}
