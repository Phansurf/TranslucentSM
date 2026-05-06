// start.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
#include <sddl.h>
#include <aclapi.h>
#include <Shlobj.h>
#include <filesystem>
#include "resource.h"

PSECURITY_DESCRIPTOR sd = nullptr, sdFile = nullptr;

bool RegistryGrantAll(HKEY hKey)
{
	bool bResult = false;

	DWORD ok = 0;
	const TCHAR* szSD =
		TEXT("D:")                          // Discretionary ACL
		TEXT("(A;OICI;KR;;;S-1-5-12)")      // NT AUTHORITY\RESTRICTED
		TEXT("(A;OICI;KA;;;S-1-5-18)")      // NT AUTHORITY\SYSTEM
		TEXT("(A;OICI;KA;;;S-1-5-32-545)")  // BUILTIN\Users
		TEXT("(A;OICI;KA;;;S-1-5-32-544)")  // BUILTIN\Administrators
		TEXT("(A;OICI;KA;;;S-1-15-2-1)");   // APPLICATION PACKAGE AUTHORITY\ALL APPLICATION PACKAGES

	if (ConvertStringSecurityDescriptorToSecurityDescriptor((LPCTSTR)szSD, SDDL_REVISION_1, &sd, 0))
	{
		auto result = RegSetKeySecurity(hKey, DACL_SECURITY_INFORMATION, sd);
		if (ERROR_SUCCESS == result)
			bResult = true;
		else
			SetLastError(result);
		LocalFree(sd);
	}
	return bResult;
}

int FileGrantAll(LPCWSTR file)
{
	const TCHAR* szSD =
		TEXT("D:AI")                          // Discretionary ACL
		TEXT("(A;OICI;0x1200a9;;;AC)")
		TEXT("(A;OICIID;FA;;;BA)")
		TEXT("(A;OICIID;FA;;;SY)")
		TEXT("(A;OICIID;0x1200a9;;;BU)")
		TEXT("(A;ID;0x1301bf;;;AU)")
		TEXT("(A;OICIIOID;SDGXGWGR;;;AU)");
	if (ConvertStringSecurityDescriptorToSecurityDescriptor((LPCTSTR)szSD, SDDL_REVISION_1, &sdFile, 0))
	{
		SetFileSecurity(file, DACL_SECURITY_INFORMATION, sdFile);
	}
	LocalFree(sdFile);
	return 1;
}

int CreateDwords(HKEY subKey, LPCWSTR value, DWORD defVal)
{
	DWORD dwSize = sizeof(DWORD);
	if (ERROR_FILE_NOT_FOUND == RegGetValue(HKEY_CURRENT_USER, L"Software\\TranslucentSM", value, RRF_RT_DWORD, NULL, NULL, &dwSize))
	{
		return RegSetValueEx(subKey, value, 0, REG_DWORD, (const BYTE*)&defVal, sizeof(defVal));
	}
	return -1;
}

int main(int argc, char* argv[])
{
	std::cout << "Initializing...\n--------------------\n";
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	std::wstring ok = L"StartMenuExperienceHost.exe";

	if (argc > 3)
	{
		std::string arg = argv[3];
		for (auto& x : arg) {
			x = tolower(x);
		}
		if (arg == "/process")
		{
			std::wstring ws(argv[4], argv[4] + strlen(argv[4]));
			ok = ws;
		}
	}
	typedef HRESULT(*InitializeXamlDiagnosticsExProto)(_In_ LPCWSTR endPointName, _In_ DWORD pid, _In_opt_ LPCWSTR wszDllXamlDiagnostics, _In_ LPCWSTR wszTAPDllName, _In_opt_ CLSID tapClsid, _In_ LPCWSTR wszInitializationData);
	InitializeXamlDiagnosticsExProto InitializeXamlDiagnosticsExFn = (InitializeXamlDiagnosticsExProto)GetProcAddress(LoadLibraryEx(L"Windows.UI.Xaml.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32), "InitializeXamlDiagnosticsEx");

	HKEY subKey = nullptr;
	DWORD disposition;
	DWORD dwSize = sizeof(DWORD), dwInstalled = 0;
	RegCreateKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\TranslucentSM", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &subKey, &disposition);
	if (disposition == REG_CREATED_NEW_KEY)
	{
		std::cout << "\n- Created HKCU\\SOFTWARE\\TranslucentSM registry key.";
		RegistryGrantAll(subKey);
	}
	else
	{
		std::cout << "\n- Opened HKCU\\SOFTWARE\\TranslucentSM registry key.";
	}

	CreateDwords(subKey, L"HideSearch", 0);
	CreateDwords(subKey, L"HideBorder", 0);
	CreateDwords(subKey, L"EditButton", 1);
	CreateDwords(subKey, L"HideRecommended", 0);
	CreateDwords(subKey, L"TintOpacity", 30);
	CreateDwords(subKey, L"TintLuminosityOpacity", 30);

	RegCloseKey(subKey);

	wchar_t path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring pathStr = path;

	std::wstring dir = pathStr.substr(0, pathStr.find_last_of(L"\\"));

	std::wstring dllPath = dir + L"\\";
	std::wstring fn;

	if (argc > 1)
	{
		std::string arg = argv[1];
		for (auto& x : arg) {
			x = tolower(x);
		}
		if (arg == "/dllname")
		{
			std::wstring ws(argv[2], argv[2] + strlen(argv[2]));
			fn = ws;
			dllPath += ws;
		}
	}
	else
	{
		dllPath += L"StartTAP.dll";
		fn = L"StartTAP.dll";
	}

	const wchar_t* dllPathW = dllPath.c_str();

	if (!std::filesystem::exists(dllPathW))
	{
		std::wcout << "\n- " << dllPathW << " not found." << "\n\n";
		return 0;
	}
	auto nResult = FileGrantAll(dllPathW);
	if (nResult != 0)
	{
		std::cout << "\n- Changed ";
		std::wcout << fn.c_str();
		std::cout << " permissions.";
	}

	static constexpr GUID tapClsid = { 0x36162bd3, 0x3531, 0x4131, { 0x9b, 0x8b, 0x7f, 0xb1, 0xa9, 0x91, 0xef, 0x51 } };

	auto FindPid = [](const std::wstring& name) -> DWORD {
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);
		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		DWORD p = 0;
		if (Process32First(snap, &pe) == TRUE)
		{
			while (Process32Next(snap, &pe) == TRUE)
			{
				if (wcscmp(pe.szExeFile, name.c_str()) == 0)
				{
					p = pe.th32ProcessID;
				}
			}
		}
		CloseHandle(snap);
		return p;
	};

	auto InjectIntoProcess = [&](const std::wstring& procName) {
		DWORD pid = FindPid(procName);
		if (pid == 0)
		{
			std::wcout << L"\n- " << procName << L" is not running.";
			return;
		}
		std::wcout << L"\n- " << procName << L" PID: " << pid;

		HRESULT hr = InitializeXamlDiagnosticsExFn(L"VisualDiagConnection1", pid, NULL, dllPathW, tapClsid, L"");
		if (SUCCEEDED(hr))
			std::wcout << L"\n- Injected " << dllPathW << L" into " << procName;
		else
			std::wcout << L"\n- Failed to inject into " << procName << L" (HRESULT: 0x" << std::hex << hr << L")";
	};

	auto InjectIntoExplorerViaHook = [&](const wchar_t* dllPathW) {
		DWORD pid = FindPid(L"explorer.exe");
		if (pid == 0)
		{
			std::wcout << L"\n- explorer.exe is not running.";
			return;
		}
		std::wcout << L"\n- explorer.exe PID: " << pid;

		HWND hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
		if (!hTray)
		{
			std::wcout << L"\n- Could not find Shell_TrayWnd window.";
			return;
		}
		DWORD tid = GetWindowThreadProcessId(hTray, nullptr);
		std::wcout << L"\n- Taskbar thread ID: " << tid;

		HANDLE hStart = CreateEventW(nullptr, TRUE, FALSE, L"TSM_ExplorerDiag_Start");
		HANDLE hReady = CreateEventW(nullptr, TRUE, FALSE, L"TSM_ExplorerDiag_Ready");
		if (!hStart || !hReady)
		{
			std::wcout << L"\n- Failed to create signaling events.";
			if (hStart) CloseHandle(hStart);
			if (hReady) CloseHandle(hReady);
			return;
		}

		HMODULE hDll = LoadLibraryW(dllPathW);
		if (!hDll)
		{
			std::wcout << L"\n- Failed to load " << dllPathW << L" in current process (error " << GetLastError() << L").";
			CloseHandle(hStart);
			CloseHandle(hReady);
			return;
		}

		auto hookProc = (HOOKPROC)GetProcAddress(hDll, "CallWndProc");
		if (!hookProc)
		{
			std::wcout << L"\n- Failed to get CallWndProc address (error " << GetLastError() << L").";
			FreeLibrary(hDll);
			CloseHandle(hStart);
			CloseHandle(hReady);
			return;
		}

		std::wcout << L"\n- Injecting " << dllPathW << L" into explorer.exe via SetWindowsHookEx...";

		HHOOK hHook = SetWindowsHookExW(WH_CALLWNDPROC, hookProc, hDll, tid);
		if (!hHook)
		{
			std::wcout << L"\n- SetWindowsHookEx failed (error " << GetLastError() << L").";
			FreeLibrary(hDll);
			CloseHandle(hStart);
			CloseHandle(hReady);
			return;
		}

		PostMessageW(hTray, WM_NULL, 0, 0);

		std::wcout << L"\n- Waiting for DLL to initialize XAML Diagnostics (timeout 20s)...";

		DWORD waitResult = WaitForSingleObject(hReady, 20000);
		if (waitResult == WAIT_OBJECT_0)
		{
			std::wcout << L"\n- DLL initialized successfully!";
		}
		else if (waitResult == WAIT_TIMEOUT)
		{
			std::wcout << L"\n- Timed out waiting for DLL initialization.";
		}
		else
		{
			std::wcout << L"\n- Wait failed (error " << GetLastError() << L").";
		}

		UnhookWindowsHookEx(hHook);
		FreeLibrary(hDll);
		CloseHandle(hStart);
		CloseHandle(hReady);
	};

	if (wcscmp(ok.c_str(), L"StartMenuExperienceHost.exe") == 0)
	{
		InjectIntoProcess(L"StartMenuExperienceHost.exe");
		InjectIntoProcess(L"SearchHost.exe");
		InjectIntoProcess(L"ShellExperienceHost.exe");
		InjectIntoExplorerViaHook(dllPathW);
	}
	else if (ok == L"explorer.exe")
	{
		InjectIntoExplorerViaHook(dllPathW);
	}
	else
	{
		InjectIntoProcess(ok);
	}

	std::cout << "\n\n";
	return 0;
}
