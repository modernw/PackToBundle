#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include <tlhelp32.h>
#include "strcmp.h"

// 用于传递搜索参数的结构体
struct WindowSearchData
{
	LPCWSTR targetTitle;
	std::vector <HWND> *foundHandles;
};

// 精确匹配回调函数
BOOL CALLBACK EnumWindowsProc_FindWindowsByTitleW (HWND hwnd, LPARAM lParam)
{
	WindowSearchData* data = reinterpret_cast <WindowSearchData *> (lParam);
	int length = GetWindowTextLengthW (hwnd);
	if (length == 0) return TRUE;
	LPWSTR buffer = new wchar_t [length + 1];
	GetWindowTextW (hwnd, buffer, length + 1);
	if (wcscmp (buffer, data->targetTitle) == 0)
	{
		data->foundHandles->push_back (hwnd);
	}
	delete [] buffer;
	return TRUE;  
}

BOOL CALLBACK EnumWindowsProc_FindWindowsByTitleBlurW (HWND hwnd, LPARAM lParam) 
{
	WindowSearchData* data = reinterpret_cast <WindowSearchData *> (lParam);
	int length = GetWindowTextLengthW (hwnd);
	if (length == 0) return TRUE;
	wchar_t* buffer = new wchar_t [length + 1];
	GetWindowTextW (hwnd, buffer, length + 1);
	if (wcsstr (buffer, data->targetTitle) != nullptr) 
	{
		data->foundHandles->push_back (hwnd);
	}
	delete [] buffer;
	return TRUE; 
}

std::vector <HWND> FindWindowsByTitleW (const LPCWSTR title) 
{
	WindowSearchData data;
	data.targetTitle = title;
	std::vector <HWND> handles;
	data.foundHandles = &handles;
	EnumWindows (EnumWindowsProc_FindWindowsByTitleW, reinterpret_cast <LPARAM> (&data));
	return handles;
}

std::vector <HWND> FindWindowsByTitleBlurW (const LPCWSTR title)
{
	WindowSearchData data;
	data.targetTitle = title;
	std::vector<HWND> handles;
	data.foundHandles = &handles;
	EnumWindows (EnumWindowsProc_FindWindowsByTitleBlurW, reinterpret_cast <LPARAM> (&data));
	return handles;
}


LONG SetWindowOwner (HWND child, HWND owner) 
{
	return SetWindowLongW (child, GWLP_HWNDPARENT, (LONG_PTR)owner);
}

HANDLE GetThreadHandleFromHwnd (HWND hwnd)
{
	DWORD threadId = 0;
	DWORD processId = 0;
	threadId = GetWindowThreadProcessId (hwnd, &processId);
	if (threadId == 0) return nullptr;
	HANDLE hThread = OpenThread (
		THREAD_QUERY_INFORMATION, 
		FALSE, 
		threadId 
	);
	return hThread;
}

HANDLE GetProcessHandleFromHwnd (HWND hwnd, DWORD desiredAccess = PROCESS_QUERY_INFORMATION)
{
	DWORD processId = 0;
	DWORD threadId = GetWindowThreadProcessId (hwnd, &processId);
	if (processId == 0) return nullptr;
	HANDLE hProcess = OpenProcess (desiredAccess, FALSE, processId);
	return hProcess;
}

void RefreshWindow (HWND hwnd)
{
	SetWindowPos (hwnd, HWND_TOP, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	WINDOWPOS wp = {0};
	wp.hwnd = hwnd;
	wp.flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER;
	SendMessage (hwnd, WM_WINDOWPOSCHANGED, 0, (LPARAM)&wp);
	UpdateWindow (hwnd);
	InvalidateRect (hwnd, NULL, TRUE);
}

bool SetWindowState (HWND hwnd, int nCmdShow = SW_RESTORE, bool bFore = true)
{
	if (!IsWindow (hwnd)) return false;
	ShowWindow (hwnd, SW_RESTORE);
	bool res = ShowWindow (hwnd, nCmdShow);
	RefreshWindow (hwnd);
	return SetForegroundWindow (hwnd) && res;
}

BOOL IsWindowOwner (HWND hWnd, HWND hOwner) 
{
	return (GetWindow (hWnd, GW_OWNER) == hOwner);
}

#define SetWindowTopMost(_HWND_hWnd_, _bool_bTopMost_) SetWindowPos (_HWND_hWnd_, (_bool_bTopMost_ ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)
#define IsWindowTopMost(_HWND_hWnd_) (GetWindow(_HWND_hWnd_, GW_HWNDNEXT) == HWND_TOPMOST)

std::string WStringToUTF8 (const std::wstring &wstr)
{
	if (wstr.empty ()) return "";

	int size_needed = WideCharToMultiByte (
		CP_UTF8,
		0,
		wstr.c_str (),
		(int)wstr.size (),
		nullptr,
		0,
		nullptr, nullptr
	);
	if (size_needed <= 0) return "";
	std::string str (size_needed, 0);
	WideCharToMultiByte (
		CP_UTF8, 0,
		wstr.c_str (), (int)wstr.size (),
		&str [0], size_needed,
		nullptr, nullptr
	);
	return str;
}
std::string WStringToUTF8 (const std::string &str) { return str; }
std::string WStringToANSI (const std::wstring &wstr)
{
	if (wstr.empty ()) return "";
	int size_needed = WideCharToMultiByte (
		CP_ACP,
		0,
		wstr.c_str (), (int)wstr.size (),
		nullptr, 0,
		nullptr, nullptr
	);
	if (size_needed <= 0) return "";
	std::string str (size_needed, 0);
	WideCharToMultiByte (
		CP_ACP, 0,
		wstr.c_str (), (int)wstr.size (),
		&str [0], size_needed,
		nullptr, nullptr
	);
	return str;
}
std::string WStringToANSI (const std::string &str) { return str; }
std::string GetProcessNameByIdA (DWORD dwProcessId)
{
	std::string name;
	HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof (PROCESSENTRY32);
		if (Process32First (hSnapshot, &pe))
		{
			do
			{
				if (pe.th32ProcessID == dwProcessId)
				{
					name = WStringToANSI (pe.szExeFile);
					break;
				}
			} while (Process32Next (hSnapshot, &pe));
		}
		CloseHandle (hSnapshot);
	}
	return name;
}
// W版本
std::wstring GetProcessNameByIdW (DWORD dwProcessId)
{
	std::wstring name;
	HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32W pe;
		pe.dwSize = sizeof (PROCESSENTRY32W);
		if (Process32FirstW (hSnapshot, &pe))
		{
			do
			{
				if (pe.th32ProcessID == dwProcessId)
				{
					name = pe.szExeFile;
					break;
				}
			} while (Process32NextW (hSnapshot, &pe));
		}
		CloseHandle (hSnapshot);
	}
	return name;
}
// 进程_ID取窗口句柄
// A版本
HWND FindWindowByProcessIdA (
	DWORD dwPid,
	LPCSTR lpWindowTitle = NULL,
	LPCSTR lpClassName = NULL,
	DWORD dwTimeout = INFINITE,
	BOOL bVisibleOnly = TRUE)
{
	DWORD dwStart = GetTickCount ();
	HWND hWnd = NULL;
	while ((GetTickCount () - dwStart) < dwTimeout)
	{
		hWnd = FindWindowExA (NULL, hWnd, NULL, NULL);
		if (!hWnd) break;

		if (bVisibleOnly && !IsWindowVisible (hWnd)) continue;

		DWORD dwProcessId = 0;
		GetWindowThreadProcessId (hWnd, &dwProcessId);
		if (dwProcessId == dwPid && GetParent (hWnd) == NULL)
		{
			CHAR szClass [256] = {0};
			CHAR szTitle [256] = {0};
			GetClassNameA (hWnd, szClass, 256);
			GetWindowTextA (hWnd, szTitle, 256);

			BOOL bClassMatch = !lpClassName || strstr (szClass, lpClassName);
			BOOL bTitleMatch = !lpWindowTitle || strstr (szTitle, lpWindowTitle);

			if (bClassMatch && bTitleMatch) return hWnd;
		}
	}
	return NULL;
}
// W版本
HWND FindWindowByProcessIdW (
	DWORD dwPid,
	LPCWSTR lpWindowTitle = NULL,
	LPCWSTR lpClassName = NULL,
	DWORD dwTimeout = INFINITE,
	BOOL bVisibleOnly = TRUE)
{
	DWORD dwStart = GetTickCount ();
	HWND hWnd = NULL;
	while ((GetTickCount () - dwStart) < dwTimeout)
	{
		hWnd = FindWindowExW (NULL, hWnd, NULL, NULL);
		if (!hWnd) break;
		if (bVisibleOnly && !IsWindowVisible (hWnd)) continue;
		DWORD dwProcessId = 0;
		GetWindowThreadProcessId (hWnd, &dwProcessId);
		if (dwProcessId == dwPid && GetParent (hWnd) == NULL)
		{
			WCHAR szClass [256] = {0};
			WCHAR szTitle [256] = {0};
			GetClassNameW (hWnd, szClass, 256);
			GetWindowTextW (hWnd, szTitle, 256);
			BOOL bClassMatch = !lpClassName || wcsstr (szClass, lpClassName);
			BOOL bTitleMatch = !lpWindowTitle || wcsstr (szTitle, lpWindowTitle);

			if (bClassMatch && bTitleMatch) return hWnd;
		}
	}
	return NULL;
}
std::wstring StringToWString (const std::string &str, UINT codePage = CP_ACP)
{
	if (str.empty ()) return L"";
	int requiredSize = MultiByteToWideChar (
		codePage,
		0,
		str.data (),
		static_cast <int> (str.size ()),
		nullptr,
		0
	);

	if (requiredSize == 0) {
		return L"";
	}
	std::wstring wstr;
	wstr.resize (requiredSize);
	int convertedSize = MultiByteToWideChar (
		codePage,
		0,
		str.data (),
		static_cast <int> (str.size ()),
		&wstr [0],
		requiredSize
	);
	if (convertedSize == 0) return L"";
	return wstr;
}
std::wstring StringToWString (const std::wstring &wstr) { return wstr; }
DWORD GetProcessIdByName (LPCSTR lpProcessName, BOOL bCaseSensitive = FALSE)
{
	DWORD dwPid = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
	PROCESSENTRY32 pe;
	pe.dwSize = sizeof (PROCESSENTRY32);
	if (Process32First (hSnapshot, &pe))
	{
		do {
			bool res = false;
			if (bCaseSensitive) res = LabelEqual (pe.szExeFile, StringToWString (lpProcessName));
			else res = (lstrcmp (pe.szExeFile, StringToWString (lpProcessName).c_str ()) == 0);
			if (res) {
				dwPid = pe.th32ProcessID;
				break;
			}
		} while (Process32Next (hSnapshot, &pe));
	}
	CloseHandle (hSnapshot);
	return dwPid;
}
// W版本 (Unicode)
DWORD GetProcessIdByName (LPCWSTR lpProcessName, BOOL bCaseSensitive = FALSE)
{
	DWORD dwPid = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return 0;
	PROCESSENTRY32W pe;
	pe.dwSize = sizeof (PROCESSENTRY32W);
	if (Process32FirstW (hSnapshot, &pe))
	{
		do {
			int (*compare)(const wchar_t *, const wchar_t *) = bCaseSensitive ?
				&wcscmp : &_wcsicmp;
			if (compare (pe.szExeFile, lpProcessName) == 0)
			{
				dwPid = pe.th32ProcessID;
				break;
			}
		} while (Process32NextW (hSnapshot, &pe));
	}
	CloseHandle (hSnapshot);
	return dwPid;
}
HWND GetWindowFromProcessName (const std::string &pn)
{
	DWORD pid = GetProcessIdByName (pn.c_str ());
	if (pid) return FindWindowByProcessIdA (pid, NULL, NULL, 100, TRUE);
	return NULL;
}
HWND GetWindowFromProcessName (const std::wstring &pn)
{
	DWORD pid = GetProcessIdByName (pn.c_str ());
	if (pid) return FindWindowByProcessIdW (pid, NULL, NULL, 100, TRUE);
	return NULL;
}
bool SetWindowActived (HWND hWnd)
{
	DWORD currentThreadId = GetCurrentThreadId ();
	DWORD targetThreadId = GetWindowThreadProcessId (hWnd, NULL);
	AttachThreadInput (targetThreadId, currentThreadId, TRUE);
	bool res = SetActiveWindow (hWnd);
	AttachThreadInput (targetThreadId, currentThreadId, FALSE);
	SetForegroundWindow (hWnd);
	return res;
}
#define IsWindowActived(_HWND_hWnd_) (GetActiveWindow () == (_HWND_hWnd_))
HWND SetWindowParent (HWND hChild, HWND hParent)
{
	HWND res = SetParent (hChild, hParent);
	SetWindowLongPtrW (hChild, GWL_EXSTYLE, GetWindowLongPtrW (hChild, GWL_EXSTYLE) | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
	return res;
}
#define IsWindowParent(_HWND_hChild_, _HWND_hParent_) (GetParent (_HWND_hChild_) == (_HWND_hParent_))