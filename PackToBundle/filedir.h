#pragma once
#include <Windows.h>
#include <string>
#include <algorithm>
#include <shlwapi.h>
#include <vector>
#include <iomanip>
#include <functional>
#include <sstream>
#include "strcmp.h"
#include "version.h"
#include "module.h"

template <typename CharT> std::basic_string <CharT> replace_substring
(
	const std::basic_string <CharT> &str,
	const std::basic_string <CharT> &from,
	const std::basic_string <CharT> &to
)
{
	if (from.empty ()) return str;
	std::basic_string  <CharT> result;
	size_t pos = 0;
	size_t start_pos;
	while ((start_pos = str.find (from, pos)) != std::basic_string<CharT>::npos)
	{
		result.append (str, pos, start_pos - pos);
		result.append (to);
		pos = start_pos + from.length ();
	}
	result.append (str, pos, str.length () - pos);
	return result;
}
std::string GetProgramRootDirectoryA (HMODULE hModule hModule_DefaultParam)
{
	char path [MAX_PATH];
	if (GetModuleFileNameA (hModule, path, MAX_PATH))
	{
		std::string dir (path);
		size_t pos = dir.find_last_of ("\\/");
		if (pos != std::string::npos)
		{
			dir = dir.substr (0, pos);
		}
		return dir;
	}
	return "";
}
std::wstring GetProgramRootDirectoryW (HMODULE hModule hModule_DefaultParam)
{
	wchar_t path [MAX_PATH];
	if (GetModuleFileNameW (hModule, path, MAX_PATH))
	{
		std::wstring dir (path);
		size_t pos = dir.find_last_of (L"\\/");
		if (pos != std::wstring::npos)
		{
			dir = dir.substr (0, pos);
		}
		return dir;
	}
	return L"";
}
std::string EnsureTrailingSlash (const std::string &path)
{
	if (path.empty ()) return path;  // 空路径直接返回

	char lastChar = path.back ();
	if (lastChar == '\\' || lastChar == '/')
		return path;  // 已有分隔符，直接返回
					  // 根据系统或原路径格式添加适当的分隔符
	char separator = (path.find ('/') != std::string::npos) ? '/' : '\\';
	return path + separator;
}
std::wstring EnsureTrailingSlash (const std::wstring &path)
{
	if (path.empty ()) return path;

	wchar_t lastChar = path.back ();
	if (lastChar == L'\\' || lastChar == L'/')
		return path;

	wchar_t separator = (path.find (L'/') != std::wstring::npos) ? L'/' : L'\\';
	return path + separator;
}
extern "C" bool IsFileExistsW (LPCWSTR filename)
{
	DWORD dwAttrib = GetFileAttributesW (filename);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
extern "C" bool IsFileExistsA (LPCSTR filename)
{
	DWORD dwAttrib = GetFileAttributesA (filename);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
bool IsFileExists (LPWSTR filePath) { return IsFileExistsW (filePath); }
bool IsFileExists (LPCSTR filePath) { return IsFileExistsA (filePath); }
bool IsFileExists (std::string filePath) { return IsFileExistsA (filePath.c_str ()); }
bool IsFileExists (std::wstring filePath) { return IsFileExistsW (filePath.c_str ()); }
bool IsDirectoryExistsA (LPCSTR path)
{
	DWORD attributes = GetFileAttributesA (path);
	return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}
bool IsDirectoryExistsW (LPCWSTR path)
{
	DWORD attributes = GetFileAttributesW (path);
	return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
}
bool IsDirectoryExists (const std::string path) { return IsDirectoryExistsA (path.c_str ()); }
bool IsDirectoryExists (const std::wstring path) { return IsDirectoryExistsW (path.c_str ()); }
bool IsDirectoryExists (LPCSTR path) { return IsDirectoryExistsA (path); }
bool IsDirectoryExists (LPCWSTR path) { return IsDirectoryExistsW (path); }
std::string NormalizePath (const std::string &path)
{
	if (!path.empty () && path.back () == '\\')
		return path.substr (0, path.size () - 1);
	return path;
}
std::wstring NormalizePath (const std::wstring &path)
{
	if (!path.empty () && path.back () == L'\\')
		return path.substr (0, path.size () - 1);
	return path;
}
std::vector <std::string> EnumSubdirectories (const std::string &directory, bool includeParentPath)
{
	std::vector<std::string> subdirs;
	std::string normPath = NormalizePath (directory);
	std::string searchPath = normPath + "\\*";
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA (searchPath.c_str (), &findData);
	if (hFind == INVALID_HANDLE_VALUE) return subdirs;
	do
	{
		// 过滤 "." 和 ".."
		if (strcmp (findData.cFileName, ".") == 0 || strcmp (findData.cFileName, "..") == 0)
			continue;
		// 判断是否为目录
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (includeParentPath)
				subdirs.push_back (normPath + "\\" + findData.cFileName);
			else
				subdirs.push_back (findData.cFileName);
		}
	} while (FindNextFileA (hFind, &findData));
	FindClose (hFind);
	return subdirs;
}
std::vector <std::wstring> EnumSubdirectories (const std::wstring &directory, bool includeParentPath)
{
	std::vector<std::wstring> subdirs;
	std::wstring normPath = NormalizePath (directory);
	std::wstring searchPath = normPath + L"\\*";
	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW (searchPath.c_str (), &findData);
	if (hFind == INVALID_HANDLE_VALUE) return subdirs;
	do
	{
		if (wcscmp (findData.cFileName, L".") == 0 || wcscmp (findData.cFileName, L"..") == 0)
			continue;
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (includeParentPath)
				subdirs.push_back (normPath + L"\\" + findData.cFileName);
			else
				subdirs.push_back (findData.cFileName);
		}
	} while (FindNextFileW (hFind, &findData));
	FindClose (hFind);
	return subdirs;
}
std::string GetCurrentProgramPathA (HMODULE hModule hModule_DefaultParam)
{
	char buf [MAX_PATH + 1] = {0};
	GetModuleFileNameA (hModule, buf, MAX_PATH);
	std::string str ("");
	str += buf;
	return str;
}
std::wstring GetCurrentProgramPathW (HMODULE hModule hModule_DefaultParam)
{
	WCHAR buf [MAX_PATH + 1] = {0};
	GetModuleFileNameW (hModule, buf, MAX_PATH);
	std::wstring str (L"");
	str += buf;
	return str;
}
version GetExeFileVersion (LPCSTR lpszFilePath)
{
	version ver (0);
	DWORD dummy;
	DWORD size = GetFileVersionInfoSizeA (lpszFilePath, &dummy);
	BYTE* pVersionInfo = new BYTE [size];
	if (!GetFileVersionInfoA (lpszFilePath, 0, size, pVersionInfo)) 
	{
		delete [] pVersionInfo;
		return ver;
	}
	VS_FIXEDFILEINFO* pFileInfo = nullptr;
	UINT len = 0;
	if (!VerQueryValueA (pVersionInfo, "\\", (LPVOID *)&pFileInfo, &len)) 
	{
		delete [] pVersionInfo;
		return ver;
	}
	if (len == 0 || pFileInfo == nullptr)
	{
		delete [] pVersionInfo;
		return ver;
	}
	ver = version (
		HIWORD (pFileInfo->dwFileVersionMS),
		LOWORD (pFileInfo->dwFileVersionMS),
		HIWORD (pFileInfo->dwFileVersionLS),
		LOWORD (pFileInfo->dwFileVersionLS)
	);
	delete [] pVersionInfo;
	return ver;
}
version GetExeFileVersion (LPCWSTR lpswFilePath)
{
	version ver (0);
	DWORD dummy;
	DWORD size = GetFileVersionInfoSizeW (lpswFilePath, &dummy);
	BYTE* pVersionInfo = new BYTE [size];
	if (!GetFileVersionInfoW (lpswFilePath, 0, size, pVersionInfo))
	{
		delete [] pVersionInfo;
		return ver;
	}
	VS_FIXEDFILEINFO* pFileInfo = nullptr;
	UINT len = 0;
	if (!VerQueryValueA (pVersionInfo, "\\", (LPVOID *)&pFileInfo, &len))
	{
		delete [] pVersionInfo;
		return ver;
	}
	if (len == 0 || pFileInfo == nullptr)
	{
		delete [] pVersionInfo;
		return ver;
	}
	ver = version (
		HIWORD (pFileInfo->dwFileVersionMS),
		LOWORD (pFileInfo->dwFileVersionMS),
		HIWORD (pFileInfo->dwFileVersionLS),
		LOWORD (pFileInfo->dwFileVersionLS)
	);
	delete [] pVersionInfo;
	return ver;
}
version GetExeFileVersion (std::wstring objswFilePath)
{
	return GetExeFileVersion (objswFilePath.c_str ());
}
version GetExeFileVersion (std::string objszFilePath)
{
	return GetExeFileVersion (objszFilePath.c_str ());
}
// 设置当前进程的环境变量RunPath和ProgramPath
void SetupInstanceEnvironment (HMODULE hModule hModule_DefaultParam)
{
	// 设置RunPath为当前工作目录（无结尾反斜杠）
	wchar_t currentDir [MAX_PATH];
	DWORD len = GetCurrentDirectoryW (MAX_PATH, currentDir);
	if (len > 0) 
	{
		std::wstring runPath (currentDir);
		if (!runPath.empty () && (runPath.back () == L'\\' || runPath.back () == L'/')) 
		{
			runPath.pop_back ();
		}
		SetEnvironmentVariableW (L"RunPath", runPath.c_str ());
	}
	// 设置ProgramPath为程序所在目录（无结尾反斜杠）
	wchar_t modulePath [MAX_PATH];
	len = GetModuleFileNameW (hModule, modulePath, MAX_PATH);
	if (len > 0 && len < MAX_PATH)
	{
		wchar_t* lastSlash = wcsrchr (modulePath, L'\\');
		if (!lastSlash) lastSlash = wcsrchr (modulePath, L'/');
		if (lastSlash) *lastSlash = L'\0';
		std::wstring programPath (modulePath);
		if (!programPath.empty () && (programPath.back () == L'\\' || programPath.back () == L'/'))
		{
			programPath.pop_back ();
		}
		SetEnvironmentVariableW (L"ProgramPath", programPath.c_str ());
	}
}
// 处理宽字符串环境变量展开
std::wstring ProcessEnvVars (const std::wstring &input) 
{
	DWORD requiredSize = ExpandEnvironmentStringsW (input.c_str (), nullptr, 0);
	if (requiredSize == 0) return input;
	std::wstring buffer (requiredSize, L'\0');
	if (!ExpandEnvironmentStringsW (input.c_str (), &buffer [0], requiredSize)) 
	{
		return input;
	}
	buffer.resize (requiredSize - 1); // 去除终止空字符
	return buffer;
}
std::wstring ProcessEnvVars (LPCWSTR input) 
{
	return ProcessEnvVars (std::wstring (input));
}
// 处理ANSI字符串环境变量展开
std::string ProcessEnvVars (const std::string &input) 
{
	DWORD requiredSize = ExpandEnvironmentStringsA (input.c_str (), nullptr, 0);
	if (requiredSize == 0) return input;
	std::string buffer (requiredSize, '\0');
	if (!ExpandEnvironmentStringsA (input.c_str (), &buffer [0], requiredSize))
	{
		return input;
	}
	buffer.resize (requiredSize - 1); // 去除终止空字符
	return buffer;
}
std::string ProcessEnvVars (LPCSTR input)
{
	return ProcessEnvVars (std::string (input));
}
std::string GetCurrentDirectoryA ()
{
	std::string str ("");
	char buf [MAX_PATH] = {0};
	GetCurrentDirectoryA (MAX_PATH, buf);
	str += buf;
	return str;
}
std::wstring GetCurrentDirectoryW ()
{
	std::wstring str (L"");
	WCHAR buf [MAX_PATH] = {0};
	GetCurrentDirectoryW (MAX_PATH, buf);
	str += buf;
	return str;
}
std::wstring GetFileDirectoryW (const std::wstring &filePath)
{
	std::wstring directory (L"");
	WCHAR *buf = (WCHAR *)calloc (filePath.capacity () + 1, sizeof (WCHAR));
	lstrcpyW (buf, filePath.c_str ());
	PathRemoveFileSpecW (buf);
	directory += buf;
	free (buf);
	return directory;
}
std::string GetFileDirectoryA (const std::string &filePath)
{
	std::string directory ("");
	char *buf = (char *)calloc (filePath.capacity () + 1, sizeof (char));
	lstrcpyA (buf, filePath.c_str ());
	PathRemoveFileSpecA (buf);
	directory += buf;
	free (buf);
	return directory;
}
size_t EnumerateFilesW (const std::wstring &directory, const std::wstring &filter,
	std::vector <std::wstring> &outFiles, bool recursive = false)
{
	std::wstring searchPath = directory;
	if (!searchPath.empty () && searchPath.back () != L'\\')
	{
		searchPath += L'\\';
	}
	searchPath += filter;
	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileW (searchPath.c_str (), &findData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do {
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				outFiles.push_back (directory + L"\\" + findData.cFileName);
			}
		} while (FindNextFileW (hFind, &findData));
		FindClose (hFind);
	}
	if (recursive) {
		std::wstring subDirSearchPath = directory + L"\\*";
		hFind = FindFirstFileW (subDirSearchPath.c_str (), &findData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do {
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
					wcscmp (findData.cFileName, L".") != 0 && wcscmp (findData.cFileName, L"..") != 0)
				{
					EnumerateFilesW (directory + L"\\" + findData.cFileName, filter, outFiles, true);
				}
			} while (FindNextFileW (hFind, &findData));
			FindClose (hFind);
		}
	}
	return outFiles.size ();
}
// 检查是否为 Windows 设备名（大小写不敏感）
bool IsReservedName (const std::wstring &name) 
{
	static const wchar_t* reserved [] = {
		L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
		L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
	};
	std::wstring upperName = StringToUpper (name);
	for (const auto& res : reserved)
	{
		if (upperName == res || (upperName.rfind (res, 0) == 0 && upperName.length () > wcslen (res) && upperName [wcslen (res)] == L'.')) 
		{
			return true;
		}
	}
	return false;
}
// Windows 文件命名规范检查 (Unicode)
bool IsValidWindowsNameW (LPCWSTR name) 
{
	if (!name || !*name) return false;
	std::wstring wname (name);
	if (wname.find_first_of (L"<>:\"/\\|?*") != std::wstring::npos) return false;
	if (IsReservedName (wname)) return false;
	if (wname.back () == L' ' || wname.back () == L'.') return false;
	return true;
}
// Windows 文件命名规范检查 (ANSI)
bool IsValidWindowsNameA (LPCSTR name) 
{
	if (!name || !*name) return false;
	std::string str (name);
	if (str.find_first_of ("<>:\"/\\|?*") != std::string::npos) return false;

	// 转换 ANSI 到宽字符
	int len = MultiByteToWideChar (CP_ACP, 0, name, -1, NULL, 0);
	if (len <= 0) return false;
	std::wstring wname (len - 1, L'\0');
	MultiByteToWideChar (CP_ACP, 0, name, -1, &wname [0], len);
	if (IsReservedName (wname)) return false;
	if (str.back () == ' ' || str.back () == '.') return false;
	return true;
}
bool IsValidWindowsName (LPCSTR name) { return IsValidWindowsNameA (name); }
bool IsValidWindowsName (LPCWSTR name) { return IsValidWindowsNameW (name); }
bool IsValidWindowsName (const std::wstring &name) { return IsValidWindowsName (name.c_str ()); }
bool IsValidWindowsName (const std::string &name) { return IsValidWindowsName (name.c_str ()); }
std::wstring GetRootFolderNameFromFilePath (const std::wstring &lpFilePath)
{
	WCHAR szPath [MAX_PATH + 1] = {0};
	if (!PathCanonicalizeW (szPath, lpFilePath.c_str ())) return L"";
	if (PathRemoveFileSpecW (szPath) == FALSE) return L"";
	LPCWSTR pszFolder = PathFindFileNameW (szPath);
	if (*pszFolder != L'\0') return std::wstring (pszFolder);
	WCHAR rootName [3] = {szPath [0], L':', L'\0'};
	return std::wstring (rootName);
}
std::wstring GetSafeTimestampForFilename () 
{
	::FILETIME ft;
	GetSystemTimeAsFileTime (&ft); 
	SYSTEMTIME st;
	FileTimeToSystemTime (&ft, &st); 
	std::wstringstream wss;
	wss << std::setfill (L'0')
		<< st.wYear
		<< std::setw (2) << st.wMonth
		<< std::setw (2) << st.wDay << L"_"
		<< std::setw (2) << st.wHour
		<< std::setw (2) << st.wMinute
		<< std::setw (2) << st.wSecond
		<< std::setw (3) << st.wMilliseconds;
	return wss.str ();
}
size_t EnumFiles (
	const std::wstring &lpDir,
	const std::wstring &lpFilter,
	std::vector <std::wstring> &aszOutput,
	bool bOutputWithPath = false,
	bool bSortByLetter = false,
	bool bIncludeSubDir = false
) {
	if (!bIncludeSubDir) aszOutput.clear ();
	std::vector<std::wstring> filters;
	size_t start = 0;
	while (start < lpFilter.length ())
	{
		size_t pos = lpFilter.find (L'\\', start);
		if (pos == std::wstring::npos) pos = lpFilter.length ();
		filters.emplace_back (lpFilter.substr (start, pos - start));
		start = pos + 1;
	}

	std::function <void (const std::wstring &, std::wstring)> enumDir;
	enumDir = [&] (const std::wstring &physicalPath, std::wstring relativePath) 
	{
		WIN32_FIND_DATAW ffd;
		HANDLE hFind = FindFirstFileW ((physicalPath + L"\\*").c_str (), &ffd);
		if (hFind == INVALID_HANDLE_VALUE) return;
		do {
			if (wcscmp (ffd.cFileName, L".") == 0 ||
				wcscmp (ffd.cFileName, L"..") == 0) continue;
			const bool isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
			const std::wstring newPhysical = physicalPath + L"\\" + ffd.cFileName;
			std::wstring newRelative = relativePath;
			if (isDir) {
				if (bIncludeSubDir) {
					newRelative += ffd.cFileName;
					newRelative += L"\\";
					enumDir (newPhysical, newRelative); 
				}
			}
			else
			{
				for (const auto &filter : filters) 
				{
					if (PathMatchSpecW (ffd.cFileName, filter.c_str ())) 
					{
						aszOutput.push_back 
						(
							bOutputWithPath ? newPhysical : (relativePath + ffd.cFileName)
						);
						break;
					}
				}
			}
		} while (FindNextFileW (hFind, &ffd));
		FindClose (hFind);
	};
	enumDir (lpDir, L""); 
	if (bSortByLetter) std::sort (aszOutput.begin (), aszOutput.end ());
	return aszOutput.size ();
}
std::wstring GetRelativePath (
	const std::wstring &pszBaseDir,  
	const std::wstring &pszFullPath,
	DWORD cchRelative 
) {
	WCHAR szBase [MAX_PATH];
	wcscpy_s (szBase, MAX_PATH, pszBaseDir.c_str ());
	if (szBase [wcslen (szBase) - 1] != L'\\')
	{
		wcscat_s (szBase, MAX_PATH, L"\\");
	}
	WCHAR buf [MAX_PATH + 1] = {0};
	BOOL res = PathRelativePathToW (
		buf,
		szBase, 
		FILE_ATTRIBUTE_DIRECTORY, 
		pszFullPath.c_str (), 
		FILE_ATTRIBUTE_NORMAL 
	);
	if (res) return buf;
	else return L"";
}
size_t EnumDirectory (
	const std::wstring &lpDir,
	std::vector<std::wstring> &aszOutput,
	bool bOutputWithPath = false,
	bool bSortByLetter = false,
	bool bIncludeSubDir = false
) {
	aszOutput.clear ();
	std::function <void (const std::wstring &, const std::wstring &)> enumDir;
	enumDir = [&] (const std::wstring &physicalPath, const std::wstring &relativePath) {
		WIN32_FIND_DATAW ffd;
		HANDLE hFind = FindFirstFileW ((physicalPath + L"\\*").c_str (), &ffd);
		if (hFind == INVALID_HANDLE_VALUE) return;
		do
		{
			const std::wstring name = ffd.cFileName;
			if (name == L"." || name == L"..") continue;
			const bool isDir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			std::wstring newPhysical = physicalPath + L"\\" + name;
			std::wstring newRelative = relativePath + name;
			if (isDir)
			{
				if (bIncludeSubDir) enumDir (newPhysical, newRelative + L"\\");
				if (bOutputWithPath) aszOutput.push_back (newPhysical);
				else aszOutput.push_back (newRelative);
			}
		} while (FindNextFileW (hFind, &ffd));
		FindClose (hFind);
	};
	enumDir (lpDir, L"");
	if (bSortByLetter) std::sort (aszOutput.begin (), aszOutput.end ());
	return aszOutput.size ();
}

static DWORD CALLBACK ProgressRoutine (
	LARGE_INTEGER TotalFileSize,
	LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER /*StreamSize*/,
	LARGE_INTEGER /*StreamBytesTransferred*/,
	DWORD /*dwStreamNumber*/,
	DWORD /*dwCallbackReason*/,
	HANDLE /*hSourceFile*/,
	HANDLE /*hDestinationFile*/,
	LPVOID lpData
) {
	auto *pCallback = reinterpret_cast <std::function <void (int)> *> (lpData);
	if (pCallback && *pCallback)
	{
		int progress = static_cast <int> (
			(TotalBytesTransferred.QuadPart * 100) / TotalFileSize.QuadPart
			);
		(*pCallback) (progress);
	}
	return PROGRESS_CONTINUE;
}
bool RenameFileW (
	const std::wstring &lpSrcPath,
	const std::wstring &lpDestPath,
	std::function <void (int)> fProgress = nullptr
) {
	LPPROGRESS_ROUTINE pRoutine = nullptr;
	LPVOID pData = nullptr;
	if (fProgress) 
	{
		pRoutine = ProgressRoutine;
		pData = &fProgress;
	}
	DWORD flags = MOVEFILE_COPY_ALLOWED;
	BOOL ok = MoveFileWithProgressW (
		lpSrcPath.c_str (),
		lpDestPath.c_str (),
		pRoutine,
		pData,
		flags
	);
	return ok != FALSE;
}
bool RenameFileA (
	const std::string &lpSrcPath,
	const std::string &lpDestPath,
	std::function <void (int)> fProgress = nullptr
) {
	LPPROGRESS_ROUTINE pRoutine = nullptr;
	LPVOID pData = nullptr;
	if (fProgress)
	{
		pRoutine = ProgressRoutine;
		pData = &fProgress;
	}
	DWORD flags = MOVEFILE_COPY_ALLOWED;
	BOOL ok = MoveFileWithProgressA (
		lpSrcPath.c_str (),
		lpDestPath.c_str (),
		pRoutine,
		pData,
		flags
	);
	return ok != FALSE;
}
bool RenameFileW (const std::wstring &lpSrcDir, const std::wstring &lpSrcName, const std::wstring &lpDestName, std::function <void (int)> fProgress = nullptr)
{
	struct BuildTask
	{
		LPWSTR src = nullptr, dest = nullptr;
		~BuildTask ()
		{
			if (src != nullptr)
			{
				delete [] src;
				src = nullptr;
			}
			if (dest != nullptr)
			{
				delete [] dest;
				dest = nullptr;
			}
		}
	};
	BuildTask bt;
	bt.src = new WCHAR [lpSrcDir.length () + lpSrcName.length () + 2];
	bt.dest = new WCHAR [lpSrcDir.length () + lpDestName.length () + 2];
	PathCombineW (bt.src, lpSrcDir.c_str (), lpSrcName.c_str ());
	PathCombineW (bt.dest, lpSrcDir.c_str (), lpDestName.c_str ());
	return RenameFileW (bt.src, bt.dest, fProgress);
}
bool RenameFileA (const std::string &lpSrcDir, const std::string &lpSrcName, const std::string &lpDestName, std::function <void (int)> fProgress = nullptr)
{
	struct BuildTask
	{
		LPSTR src = nullptr, dest = nullptr;
		~BuildTask ()
		{
			if (src != nullptr)
			{
				delete [] src;
				src = nullptr;
			}
			if (dest != nullptr)
			{
				delete [] dest;
				dest = nullptr;
			}
		}
	};
	BuildTask bt;
	bt.src = new CHAR [lpSrcDir.length () + lpSrcName.length () + 2];
	bt.dest = new CHAR [lpSrcDir.length () + lpDestName.length () + 2];
	PathCombineA (bt.src, lpSrcDir.c_str (), lpSrcName.c_str ());
	PathCombineA (bt.dest, lpSrcDir.c_str (), lpDestName.c_str ());
	return RenameFileA (bt.src, bt.dest, fProgress);
}
bool RenameFile (const std::wstring &lpSrcPath, const std::wstring &lpDestPath, std::function <void (int)> fProgress = nullptr)
{
	return RenameFileW (lpSrcPath, lpDestPath, fProgress);
}
bool RenameFile (const std::string &lpSrcPath, const std::string &lpDestPath, std::function <void (int)> fProgress = nullptr)
{
	return RenameFileA (lpSrcPath, lpDestPath, fProgress);
}
bool RenameFile (const std::wstring &lpSrcDir, const std::wstring &lpSrcName, const std::wstring &lpDestName, std::function <void (int)> fProgress = nullptr)
{
	return RenameFileW (lpSrcDir, lpSrcName, lpDestName, fProgress);
}
bool RenameFile (const std::string &lpSrcDir, const std::string &lpSrcName, const std::string &lpDestName, std::function <void (int)> fProgress = nullptr)
{
	return RenameFileA (lpSrcDir, lpSrcName, lpDestName, fProgress);
}
bool RenameDirectoryW (
	const std::wstring &lpSrcPath,
	const std::wstring &lpDestPath,
	std::function <void (int)> fProgress = nullptr
) {
	LPPROGRESS_ROUTINE pRoutine = nullptr;
	LPVOID pData = nullptr;
	if (fProgress)
	{
		pRoutine = ProgressRoutine;
		pData = &fProgress;
	}
	DWORD flags = MOVEFILE_COPY_ALLOWED;
	BOOL ok = MoveFileWithProgressW (
		lpSrcPath.c_str (),
		lpDestPath.c_str (),
		pRoutine,
		pData,
		flags
	);
	return ok != FALSE;
}
bool RenameDirectoryA (
	const std::string &lpSrcPath,
	const std::string &lpDestPath,
	std::function <void (int)> fProgress = nullptr
) {
	LPPROGRESS_ROUTINE pRoutine = nullptr;
	LPVOID pData = nullptr;
	if (fProgress) 
	{
		pRoutine = ProgressRoutine;
		pData = &fProgress;
	}
	DWORD flags = MOVEFILE_COPY_ALLOWED;
	BOOL ok = MoveFileWithProgressA (
		lpSrcPath.c_str (),
		lpDestPath.c_str (),
		pRoutine,
		pData,
		flags
	);
	return ok != FALSE;
}
bool RenameDirectoryW (
	const std::wstring &lpParentDir,
	const std::wstring &lpSrcName,
	const std::wstring &lpDestName,
	std::function <void (int)> fProgress = nullptr
) {
	struct PathBuilder
	{
		LPWSTR src = nullptr;
		LPWSTR dest = nullptr;
		~PathBuilder ()
		{
			delete [] src;
			delete [] dest;
		}
	} pb;
	pb.src = new WCHAR [lpParentDir.length () + lpSrcName.length () + 2];
	pb.dest = new WCHAR [lpParentDir.length () + lpDestName.length () + 2];
	PathCombineW (pb.src, lpParentDir.c_str (), lpSrcName.c_str ());
	PathCombineW (pb.dest, lpParentDir.c_str (), lpDestName.c_str ());
	return RenameDirectoryW (pb.src, pb.dest, fProgress);
}
bool RenameDirectoryA (
	const std::string &lpParentDir,
	const std::string &lpSrcName,
	const std::string &lpDestName,
	std::function <void (int)> fProgress = nullptr
) {
	struct PathBuilder
	{
		LPSTR src = nullptr;
		LPSTR dest = nullptr;
		~PathBuilder () 
		{
			delete [] src;
			delete [] dest;
		}
	} pb;
	pb.src = new CHAR [lpParentDir.length () + lpSrcName.length () + 2];
	pb.dest = new CHAR [lpParentDir.length () + lpDestName.length () + 2];
	PathCombineA (pb.src, lpParentDir.c_str (), lpSrcName.c_str ());
	PathCombineA (pb.dest, lpParentDir.c_str (), lpDestName.c_str ());
	return RenameDirectoryA (pb.src, pb.dest, fProgress);
}
bool RenameDirectory (
	const std::wstring &src,
	const std::wstring &dst,
	std::function <void (int)> fProgress = nullptr
) {
	return RenameDirectoryW (src, dst, fProgress);
}
bool RenameDirectory (
	const std::string &src,
	const std::string &dst,
	std::function <void (int)> fProgress = nullptr
) {
	return RenameDirectoryA (src, dst, fProgress);
}
bool RenameDirectory (
	const std::wstring &parentDir,
	const std::wstring &srcName,
	const std::wstring &dstName,
	std::function <void (int)> fProgress = nullptr
) {
	return RenameDirectoryW (parentDir, srcName, dstName, fProgress);
}
bool RenameDirectory (
	const std::string &parentDir,
	const std::string &srcName,
	const std::string &dstName,
	std::function <void (int)> fProgress = nullptr
) {
	return RenameDirectoryA (parentDir, srcName, dstName, fProgress);
}