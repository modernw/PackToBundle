#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

enum class EncodingType
{
	Unknown,
	UTF8,
	UTF16LE,
	ANSI
};

EncodingType DetectEncoding (const BYTE* data, size_t length)
{
	// 检查BOM标记
	if (length >= 3 && data [0] == 0xEF && data [1] == 0xBB && data [2] == 0xBF)
	{
		return EncodingType::UTF8;
	}
	if (length >= 2 && data [0] == 0xFF && data [1] == 0xFE)
	{
		return EncodingType::UTF16LE;
	}
	// 统计零字节分布
	size_t zeroCount = 0;
	size_t consecutiveZero = 0;
	bool evenZero = true;
	bool oddZero = true;
	for (size_t i = 0; i < length; ++ i) 
	{
		if (data [i] == 0) 
		{
			++zeroCount;
			++consecutiveZero;
			if (consecutiveZero > 2) 
			{
				evenZero = oddZero = false;
			}
			// 检查奇偶位置
			if ((i % 2) == 0) evenZero = false;
			else oddZero = false;
		}
		else
		{
			consecutiveZero = 0;
		}
	}
	// 启发式判断
	if (zeroCount > length / 4)
	{
		if (evenZero || oddZero) return EncodingType::UTF16LE;
		if (consecutiveZero >= 2) return EncodingType::UTF16LE;
	}
	// 检查UTF-8有效性
	bool validUTF8 = true;
	for (size_t i = 0; i < length; )
	{
		if ((data [i] & 0x80) == 0) 
		{ // 单字节字符
			++i;
		}
		else if ((data [i] & 0xE0) == 0xC0) 
		{ // 两字节
			if (i + 1 >= length || (data [i + 1] & 0xC0) != 0x80) 
			{
				validUTF8 = false;
				break;
			}
			i += 2;
		}
		else if ((data [i] & 0xF0) == 0xE0)
		{ // 三字节
			if (i + 2 >= length || (data [i + 1] & 0xC0) != 0x80 || (data [i + 2] & 0xC0) != 0x80)
			{
				validUTF8 = false;
				break;
			}
			i += 3;
		}
		else 
		{
			validUTF8 = false;
			break;
		}
	}
	if (validUTF8) return EncodingType::UTF8;
	return EncodingType::ANSI;
}

std::wstring ConvertBufferToWide (const BYTE* data, size_t length, EncodingType encoding) 
{
	std::wstring result;
	switch (encoding) 
	{
		case EncodingType::UTF16LE: 
		{
			size_t wchars = length / sizeof (wchar_t);
			result.assign (reinterpret_cast <const wchar_t *> (data), wchars);
			break;
		}
		case EncodingType::UTF8:
		{
			int required = MultiByteToWideChar (CP_UTF8, 0,
				reinterpret_cast <LPCCH> (data), static_cast <int> (length),
				nullptr, 0);
			if (required > 0) 
			{
				result.resize (required);
				MultiByteToWideChar (CP_UTF8, 0,
					reinterpret_cast <LPCCH> (data), static_cast <int> (length),
					&result [0], required);
			}
			break;
		}
		case EncodingType::ANSI:
		{
			int required = MultiByteToWideChar (CP_ACP, 0,
				reinterpret_cast <LPCCH> (data), static_cast <int> (length),
				nullptr, 0);
			if (required > 0) 
			{
				result.resize (required);
				MultiByteToWideChar (CP_ACP, 0,
					reinterpret_cast <LPCCH> (data), static_cast <int> (length),
					&result [0], required);
			}
			break;
		}
		default:
			if (length % 2 == 0)
			{
				EncodingType retry = DetectEncoding (data, std::min<size_t> (length, 1024));
				if (retry != EncodingType::Unknown) return ConvertBufferToWide (data, length, retry);
			}
			return ConvertBufferToWide (data, length, EncodingType::ANSI);
	}
	return result;
}

struct WindowData
{
	DWORD targetPid;
	std::vector <HWND> windowHandles;
};

BOOL CALLBACK EnumWindowsProc_GetWindowHandlesByProcess (HWND hwnd, LPARAM lParam)
{
	WindowData* data = reinterpret_cast <WindowData *> (lParam);
	DWORD windowPid = 0;
	// 获取窗口所属进程的PID
	GetWindowThreadProcessId (hwnd, &windowPid);
	if (windowPid == data->targetPid) 
	{
		data->windowHandles.push_back (hwnd);
	}
	return TRUE; // 继续枚举
}

// 随机返回一个窗口句柄
std::vector <HWND> GetWindowHandlesByProcess (HANDLE hProcess)
{
	DWORD processId = GetProcessId (hProcess);
	std::vector <HWND> vecnull;
	if (processId == 0) return vecnull;
	WindowData data;
	data.targetPid = processId;
	EnumWindows (EnumWindowsProc_GetWindowHandlesByProcess, reinterpret_cast <LPARAM> (&data));
	if (data.windowHandles.size () > 0) return data.windowHandles;
	else return vecnull;
}

// 随机返回一个窗口句柄
HWND GetWindowHandleByProcess (HANDLE hProcess)
{
	std::vector <HWND> hds = GetWindowHandlesByProcess (hProcess);
	if (hds.size ()) return hds [0];
	else return NULL;
}

class ConsolePipe
{
	public:
	ConsolePipe ()
	{
		ZeroMemory (&processInfo, sizeof (PROCESS_INFORMATION));
		hStdOutRead = hStdOutWrite = hStdInRead = hStdInWrite = NULL;
		InitializeCriticalSection (&cs);
		current_pos = 0;
	}
	~ConsolePipe ()
	{
		CloseHandles ();
		DeleteCriticalSection (&cs);
	}

	std::vector <HWND> GetHWndsFromCurrentProcess ()
	{
		return GetWindowHandlesByProcess (processInfo.hProcess);
	}

	HWND GetRandomHWndFromCurrentProcess ()
	{
		std::vector <HWND> hds = GetHWndsFromCurrentProcess ();
		if (hds.size ()) return hds [0];
		else return NULL;
	}

	LONG SetCurrentProgressWndOwner (HWND owner)
	{
		if (!owner) return NULL;
		HWND hw = GetRandomHWndFromCurrentProcess ();
		if (!hw) return 0;
		return SetWindowLongW (hw, GWLP_HWNDPARENT, (LONG_PTR)owner);
	}

	bool Execute (LPCSTR command, HWND owner = NULL)
	{
		EnterCriticalSection (&cs);
		CloseHandles ();
		SECURITY_ATTRIBUTES sa;
		ZeroMemory (&sa, sizeof (SECURITY_ATTRIBUTES));
		sa.nLength = sizeof (SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;
		if (!CreatePipe (&hStdOutRead, &hStdOutWrite, &sa, 0) ||
			!SetHandleInformation (hStdOutRead, HANDLE_FLAG_INHERIT, 0) ||
			!CreatePipe (&hStdInRead, &hStdInWrite, &sa, 0) ||
			!SetHandleInformation (hStdInWrite, HANDLE_FLAG_INHERIT, 0))
		{
			LeaveCriticalSection (&cs);
			return false;
		}
		STARTUPINFOA si;
		ZeroMemory (&si, sizeof (STARTUPINFOA));
		si.cb = sizeof (STARTUPINFOA);
		si.hStdError = hStdOutWrite;
		si.hStdOutput = hStdOutWrite;
		si.hStdInput = hStdInRead;
		si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		if (!CreateProcessA (NULL, (LPSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &processInfo))
		{
			CloseHandles ();
			LeaveCriticalSection (&cs);
			return false;
		}
		if (owner)
		{
			SetCurrentProgressWndOwner (owner);
		}
		LeaveCriticalSection (&cs);
		return true;
	}

	bool Execute (LPCWSTR command, HWND owner = NULL)
	{
		EnterCriticalSection (&cs);
		CloseHandles ();
		SECURITY_ATTRIBUTES sa;
		ZeroMemory (&sa, sizeof (SECURITY_ATTRIBUTES));
		sa.nLength = sizeof (SECURITY_ATTRIBUTES);
		sa.bInheritHandle = TRUE;
		sa.lpSecurityDescriptor = NULL;
		if (!CreatePipe (&hStdOutRead, &hStdOutWrite, &sa, 0) ||
			!SetHandleInformation (hStdOutRead, HANDLE_FLAG_INHERIT, 0) ||
			!CreatePipe (&hStdInRead, &hStdInWrite, &sa, 0) ||
			!SetHandleInformation (hStdInWrite, HANDLE_FLAG_INHERIT, 0))
		{
			LeaveCriticalSection (&cs);
			return false;
		}
		STARTUPINFOW si;
		ZeroMemory (&si, sizeof (STARTUPINFOA));
		si.cb = sizeof (STARTUPINFOA);
		si.hStdError = hStdOutWrite;
		si.hStdOutput = hStdOutWrite;
		si.hStdInput = hStdInRead;
		si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;;
		si.wShowWindow = SW_HIDE;
		if (!CreateProcessW (NULL, (LPWSTR)command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &processInfo))
		{
			CloseHandles ();
			LeaveCriticalSection (&cs);
			return false;
		}
		if (owner)
		{
			SetCurrentProgressWndOwner (owner);
		}
		LeaveCriticalSection (&cs);
		return true;
	}

	bool IsProcessRunning () const
	{
		EnterCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		if (processInfo.hProcess == NULL)
		{
			LeaveCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
			return false;
		}
		DWORD exitCode;
		if (!GetExitCodeProcess (processInfo.hProcess, &exitCode))
		{
			LeaveCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
			return false;
		}
		LeaveCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		return exitCode == STILL_ACTIVE;
	}

	std::vector <BYTE> GetOutputBytes (size_t numBytes)
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();
		size_t available = buffer.size () - current_pos;
		size_t bytesToRead = min (numBytes, available);
		std::vector<BYTE> result (buffer.begin () + current_pos, buffer.begin () + current_pos + bytesToRead);
		current_pos += bytesToRead;
		LeaveCriticalSection (&cs);
		return result;
	}

	std::string GetOutputText (size_t numChars)
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();
		size_t available = buffer.size () - current_pos;
		size_t bytesToRead = min (numChars, available);
		std::string result (reinterpret_cast<const char*>(buffer.data () + current_pos), bytesToRead);
		current_pos += bytesToRead;
		LeaveCriticalSection (&cs);
		return result;
	}

	std::wstring GetOutputTextW (size_t numChars)
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();
		size_t available_bytes = buffer.size () - current_pos;
		const BYTE* start_ptr = buffer.data () + current_pos;
		EncodingType encoding = DetectEncoding (start_ptr, std::min<size_t> (available_bytes, 1024));
		size_t bytes_to_read = available_bytes;
		if (encoding == EncodingType::UTF16LE)
		{
			bytes_to_read = (available_bytes / sizeof (wchar_t)) * sizeof (wchar_t);
		}
		bytes_to_read = min (bytes_to_read, available_bytes);
		std::wstring result = ConvertBufferToWide (start_ptr, bytes_to_read, encoding);
		current_pos += bytes_to_read;
		LeaveCriticalSection (&cs);
		return result;
	}

	std::string GetOutputLine ()
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();

		size_t start = current_pos;
		size_t end = buffer.size ();
		bool found = false;
		for (size_t i = start; i < buffer.size (); ++i)
		{
			if (buffer [i] == '\n')
			{
				end = i + 1;
				found = true;
				break;
			}
			else if (buffer [i] == '\r' && i + 1 < buffer.size () && buffer [i + 1] == '\n')
			{
				end = i + 2;
				found = true;
				break;
			}
		}

		std::string line;
		if (found || start < end)
		{
			line.assign (reinterpret_cast<const char*>(buffer.data () + start), end - start);
			current_pos = end;
		}

		TrimBuffer ();
		LeaveCriticalSection (&cs);
		return line;
	}

	std::wstring GetOutputLineW ()
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();
		const BYTE* start_ptr = buffer.data () + current_pos;
		size_t available_bytes = buffer.size () - current_pos;
		EncodingType encoding = DetectEncoding (start_ptr, std::min <size_t> (available_bytes, 1024));
		std::wstring converted = ConvertBufferToWide (start_ptr, available_bytes, encoding);
		size_t lineEnd = converted.find (L'\n');
		if (lineEnd != std::wstring::npos)
		{
			if (lineEnd > 0 && converted [lineEnd - 1] == L'\r') -- lineEnd;
			std::wstring line = converted.substr (0, lineEnd);
			size_t bytesConsumed = 0;
			switch (encoding)
			{
				case EncodingType::UTF16LE:
					bytesConsumed = (lineEnd + 1) * sizeof (wchar_t);
					break;
				case EncodingType::UTF8: 
				{
					std::string temp (line.begin (), line.end ());
					bytesConsumed = temp.size () + 1;
					break;
				}
				default:
					bytesConsumed = line.size () + 1;
			}
			current_pos += bytesConsumed;
			TrimBuffer ();
			LeaveCriticalSection (&cs);
			return line;
		}
		LeaveCriticalSection (&cs);
		return L"";
	}

	std::vector<BYTE> GetAllOutputBytes ()
	{
		EnterCriticalSection (&cs);
		ReadPipeToBuffer ();
		std::vector<BYTE> result (buffer.begin (), buffer.end ()); 
		LeaveCriticalSection (&cs);
		return result;
	}

	std::string GetAllOutputA ()
	{
		auto bytes = GetAllOutputBytes ();
		return std::string (reinterpret_cast<const char*>(bytes.data ()), bytes.size ());
	}

	std::wstring GetAllOutputW ()
	{
		auto bytes = GetAllOutputBytes ();
		if (bytes.empty ()) return L"";
		EncodingType encoding = DetectEncoding (bytes.data (), bytes.size ());
		return ConvertBufferToWide (bytes.data (), bytes.size (), encoding);
	}

	void InputText (LPCSTR text)
	{
		EnterCriticalSection (&cs);
		DWORD bytesWritten;
		WriteFile (hStdInWrite, text, strlen (text), &bytesWritten, NULL);
		LeaveCriticalSection (&cs);
	}

	void InputTextW (LPCWSTR text)
	{
		EnterCriticalSection (&cs);
		DWORD bytesWritten;
		WriteFile (hStdInWrite, text, wcslen (text) * sizeof (WCHAR), &bytesWritten, NULL);
		LeaveCriticalSection (&cs);
	}

	void InputBytes (const std::vector<BYTE>& bytes)
	{
		EnterCriticalSection (&cs);
		DWORD bytesWritten;
		WriteFile (hStdInWrite, bytes.data (), bytes.size (), &bytesWritten, NULL);
		LeaveCriticalSection (&cs);
	}

	HANDLE GetProcessHandle () const
	{
		EnterCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		HANDLE handle = processInfo.hProcess;
		LeaveCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		return handle;
	}

	HANDLE GetThreadHandle () const
	{
		EnterCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		HANDLE handle = processInfo.hThread;
		LeaveCriticalSection (const_cast<LPCRITICAL_SECTION>(&cs));
		return handle;
	}

	private:
	PROCESS_INFORMATION processInfo;
	HANDLE hStdOutRead;
	HANDLE hStdOutWrite;
	HANDLE hStdInRead;
	HANDLE hStdInWrite;
	CRITICAL_SECTION cs;
	std::vector<BYTE> buffer;
	size_t current_pos;

	void CloseHandles ()
	{
		EnterCriticalSection (&cs);
		if (processInfo.hProcess != NULL)
		{
			CloseHandle (processInfo.hProcess);
			processInfo.hProcess = NULL;
		}
		if (processInfo.hThread != NULL)
		{
			CloseHandle (processInfo.hThread);
			processInfo.hThread = NULL;
		}
		if (hStdOutRead != NULL)
		{
			CloseHandle (hStdOutRead);
			hStdOutRead = NULL;
		}
		if (hStdOutWrite != NULL)
		{
			CloseHandle (hStdOutWrite);
			hStdOutWrite = NULL;
		}
		if (hStdInRead != NULL)
		{
			CloseHandle (hStdInRead);
			hStdInRead = NULL;
		}
		if (hStdInWrite != NULL)
		{
			CloseHandle (hStdInWrite);
			hStdInWrite = NULL;
		}
		buffer.clear ();
		current_pos = 0;
		LeaveCriticalSection (&cs);
	}

	void ReadPipeToBuffer ()
	{
		DWORD bytesAvailable = 0;
		if (!PeekNamedPipe (hStdOutRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) || bytesAvailable == 0)
		{
			return;
		}

		BYTE tempBuffer [256];
		DWORD bytesRead = 0;
		if (ReadFile (hStdOutRead, tempBuffer, sizeof (tempBuffer), &bytesRead, nullptr) && bytesRead > 0)
		{
			buffer.insert (buffer.end (), tempBuffer, tempBuffer + bytesRead);
		}
	}

	void TrimBuffer ()
	{
		return;
		if (current_pos > buffer.size () / 2)
		{
			buffer.erase (buffer.begin (), buffer.begin () + current_pos);
			current_pos = 0;
		}
	}
};