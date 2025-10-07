#pragma once
#include <Windows.h>
#include <atlbase.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <string>
#include <vector>
#include <map>
#include <AppxPackaging.h>
#include <process.h>
#include <fmt/format.h>
#include "filedir.h"
#include "initread.h"
#include "threadcer.h"
#include "pkgread.h"
#include "version.h"
#include "cmdpipe.h"
#include "wndlibs.h"
#include "rctools.h"
#include "resource.h"

extern WInitFile g_config (EnsureTrailingSlash (GetProgramRootDirectoryW ()) + L"config.ini");
typedef struct IODirection
{
	typedef std::function <INT (LPCTSTR)> Input;
	typedef std::function <INT (LPCTSTR)> Output;
	typedef std::function <INT (LPCTSTR)> OutputLine;
	Input input = nullptr;
	Output output = nullptr;
	OutputLine outputLine = nullptr;
	IODirection (Input inputFunc = nullptr, Output outputFunc = nullptr, OutputLine outLineFunc = nullptr):
		input (inputFunc), output (outputFunc), outputLine (outLineFunc) {}
	IODirection (const IODirection &io): input (io.input), output (io.output), outputLine (io.outputLine) {}
	IODirection &operator = (const IODirection &io)
	{
		this->input = io.input;
		this->output = io.output;
		this->outputLine = io.outputLine;
	}
	INT safeInput (LPCTSTR str) { CreateScopedLock (m_cs); if (input && strvalid (str)) return input (str); else return 0; }
	INT safeOutput (LPCTSTR str) { CreateScopedLock (m_cs); if (output && strvalid (str)) return output (str); else return 0; }
	INT safeOutputLine (LPCTSTR str) {
		CreateScopedLock (m_cs);
		if (outputLine && strvalid (str)) outputLine (str);
		else if (output && strvalid (str)) output ((std::basic_string <_TCHAR> (str) + _T ("\n")).c_str ());
		else return 0;
	}
	INT safeInput (const std::basic_string <_TCHAR> &str) { return safeInput (str.c_str ()); }
	INT safeOutput (const std::basic_string <_TCHAR> &str) { return safeOutput (str.c_str ()); }
	INT safeOutputLine (const std::basic_string <_TCHAR> &str) { return safeOutputLine (str.c_str ()); }
	private:
	CriticalSection m_cs;
} IODIRECTION, *HIODIRECTION;

const std::wstring GetConfigFilePath ()
{
	return g_config.getFilePath ();
}
std::wstring GetKitDirectory ()
{
	std::wstring kitDir = g_config.readStringValue (
		L"Settings",
		L"KitDirectory",
		EnsureTrailingSlash (GetProgramRootDirectoryW ()) + L"kits"
	);
	kitDir = ProcessEnvVars (kitDir);
	return kitDir;
}
bool CheckKits ()
{
	std::wstring kitDir = GetKitDirectory ();
	if (!IsDirectoryExists (kitDir)) return false;
	bool kitsOk =
		IsFileExists (EnsureTrailingSlash (kitDir) + L"makecert.exe") &&
		IsFileExists (EnsureTrailingSlash (kitDir) + L"pvk2pfx.exe") &&
		IsFileExists (EnsureTrailingSlash (kitDir) + L"signtool.exe");
	return kitsOk;
}

bool MakeCert (const std::wstring &swIdentityPublisher, const std::wstring &swOutputDir, const std::wstring &swOutputFileName, std::wstring &outCerFilePath, std::wstring &outPvkFilePath, IODirection &outputConsole = IODirection (), HWND owner = NULL)
{
	ConsolePipe console;
	std::wstring m_kitdir = GetKitDirectory (), cmdline (L"");
	std::wstring m_pvkfile = EnsureTrailingSlash (ProcessEnvVars (swOutputDir)) + swOutputFileName + L".pvk";
	std::wstring m_cerfile = EnsureTrailingSlash (ProcessEnvVars (swOutputDir)) + swOutputFileName + L".cer";
	cmdline += L"\"" + EnsureTrailingSlash (m_kitdir) + L"makecert.exe" + L"\"";
	cmdline += L" -n \"" + swIdentityPublisher + L"\" -r -a sha256 -len 2048 -cy end -h 0 -eku 1.3.6.1.5.5.7.3.3 -b 01/01/2000 -sv " +
		L"\"" + m_pvkfile + L"\" " +
		L"\"" + m_cerfile + L"\"";
	console.Execute (cmdline.c_str (), owner);
	bool setSuccess = false;
	while (console.IsProcessRunning ())
	{
		if (&outputConsole && outputConsole.output)
		{
			std::wstring buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
		}
		{
			//Sleep (100);
			//HWND hrandom = console.GetRandomHWndFromCurrentProcess ();
			//if (IsWindowOwner (hrandom, owner) && IsWindowActived (hrandom)) continue;
			//if (!IsWindowOwner (hrandom, owner)) SetWindowOwner (hrandom, owner);
			//// SetWindowOwner (hrandom, owner);
			//if (IsIconic (hrandom)) ShowWindow (hrandom, SW_RESTORE);
			//if (IsWindowActived (hrandom)) SetWindowActived (hrandom);
		}
	}
	WaitForSingleObject (console.GetThreadHandle (), INFINITE);
	if (&outputConsole && outputConsole.output)
	{
		int cnt = 0;
		std::wstring buf = L"";
		do {
			buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
			if (buf.empty () || buf.length () <= 0)
			{
				cnt ++;
			}
		} while (!buf.empty () && buf.length () > 0 && cnt > 2);
	}
	Sleep (100);
	std::wstring allout = console.GetAllOutputW ();
	bool res =
		(StrInclude (allout, L"Success", true) || StrInclude (allout, L"Succeed", true) || StrInclude (allout, L"Succeeded", true)) &&
		(!StrInclude (allout, L"Error", true) && !StrInclude (allout, L"Failed", true)) &&
		IsFileExists (m_cerfile) &&
		IsFileExists (m_pvkfile);
	if (&outCerFilePath) outCerFilePath = m_cerfile;
	if (&outPvkFilePath) outPvkFilePath = m_pvkfile;
	return res;
}
bool Pvk2Pfx (const std::wstring &swInputCer, const std::wstring &swInputPvk, const std::wstring &swOutputDir, const std::wstring &swOutputFileName, std::wstring &outPfxFileName, IODirection &outputConsole = IODirection (), HWND owner = NULL)
{
	ConsolePipe console;
	std::wstring m_kitdir = GetKitDirectory (), cmdline (L"");
	std::wstring m_pfxfile = EnsureTrailingSlash (ProcessEnvVars (swOutputDir)) + swOutputFileName + L".pfx";
	cmdline += L"\"" + EnsureTrailingSlash (m_kitdir) + L"pvk2pfx.exe" + L"\"";
	cmdline += std::wstring (L" -pvk ") + L"\"" + swInputPvk + L"\"" +
		L" -spc " + L"\"" + swInputCer + L"\"" +
		L" -pfx " + L"\"" + m_pfxfile + L"\"";
	console.Execute (cmdline.c_str (), owner);
	bool setSuccess = false;
	while (console.IsProcessRunning ())
	{
		if (&outputConsole && outputConsole.output)
		{
			std::wstring buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
		}
		{
			Sleep (100);
			if (IsWindowOwner (console.GetRandomHWndFromCurrentProcess (), owner)) break;
			setSuccess = console.SetCurrentProgressWndOwner (owner);
			SetWindowState (console.GetRandomHWndFromCurrentProcess ());
			if (!setSuccess)
			{
				if (!setSuccess)
				{
					std::vector <HWND> &hres = FindWindowsByTitleBlurW (L"Enter Private Key Password");
					setSuccess = false;
					for (auto it : hres)
					{
						if (GetProcessHandleFromHwnd (it) == console.GetProcessHandle ())
						{
							if (IsWindowOwner (it, owner)) break;
							setSuccess = SetWindowOwner (it, owner);
							SetWindowState (it);
							break;
						}
					}
				}
			}
		}
	}
	WaitForSingleObject (console.GetThreadHandle (), INFINITE);
	if (&outputConsole && outputConsole.output)
	{
		std::wstring buf = L"";
		do {
			buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
		} while (!buf.empty () && buf.length () > 0);
	}
	std::wstring allout = console.GetAllOutputW ();
	bool res = (LabelEqual (allout, L"") || !StrInclude (allout, L"ERROR", true) && !StrInclude (allout, L"Failed", true)) && IsFileExists (m_pfxfile);
	if (&outPfxFileName) outPfxFileName = m_pfxfile;
	return res;
}
bool SignTool (const std::wstring &swInputPkg, const std::wstring &swInputPfx, IODirection &outputConsole = IODirection ())
{
	ConsolePipe console;
	std::wstring m_kitdir = GetKitDirectory (), cmdline (L"");
	cmdline += L"\"" + EnsureTrailingSlash (m_kitdir) + L"signtool.exe" + L"\"";
	cmdline += std::wstring (L" sign -fd SHA256 -a -f ") + L"\"" + swInputPfx + L"\"" +
		L" " + L"\"" + swInputPkg + L"\"";
	console.Execute (cmdline.c_str ());
	while (console.IsProcessRunning ())
	{
		if (&outputConsole && outputConsole.output)
		{
			std::wstring buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
		}
	}
	WaitForSingleObject (console.GetThreadHandle (), INFINITE);
	if (&outputConsole && outputConsole.output)
	{
		std::wstring buf = L"";
		do {
			buf = console.GetOutputTextW (240);
			if (buf.empty ()) Sleep (50);
			outputConsole.safeOutput (buf.c_str ());
		} while (!buf.empty () && buf.length () > 0);
	}
	std::wstring allout = console.GetAllOutputW ();
	bool res = StrInclude (allout, L"Successfully signed:", true);
	return res;
}

typedef void (*PROGRESS_CALLBACK) (int uProgress);

HRESULT GetOutputStream (_In_ LPCWSTR path, _In_ LPCWSTR fileName, _Outptr_ IStream** stream)
{
	HRESULT hr = S_OK;
	const int MaxFileNameLength = 200;
	WCHAR fullFileName [MaxFileNameLength + 1];
	hr = StringCchCopyW (fullFileName, MaxFileNameLength, path);
	if (SUCCEEDED (hr)) hr = StringCchCatW (fullFileName, MaxFileNameLength, L"\\");
	if (SUCCEEDED (hr)) hr = StringCchCatW (fullFileName, MaxFileNameLength, fileName);
	for (int i = 0; SUCCEEDED (hr) && (i < MaxFileNameLength); i++)
	{
		if (fullFileName [i] == L'\0') break;
		else if (fullFileName [i] == L'\\')
		{
			fullFileName [i] = L'\0';
			if (!CreateDirectory (fullFileName, NULL))
			{
				DWORD lastError = GetLastError ();
				if (lastError != ERROR_ALREADY_EXISTS) hr = HRESULT_FROM_WIN32 (lastError);
			}
			fullFileName [i] = L'\\';
		}
	}
	if (SUCCEEDED (hr))
	{
		hr = SHCreateStreamOnFileEx (
			fullFileName,
			STGM_CREATE | STGM_WRITE | STGM_SHARE_DENY_NONE,
			0,
			TRUE,
			NULL,
			stream);
	}
	return hr;
}
HRESULT ReadBundleManifestFromStream (IStream *stream, IAppxBundleManifestReader **output)
{
	if (!output || !stream) return E_INVALIDARG;
	CComPtr <IAppxBundleFactory> bundleFactory = nullptr;
	HRESULT hr = CoCreateInstance (
		__uuidof(AppxBundleFactory),
		nullptr,
		CLSCTX_INPROC_SERVER,
		__uuidof(IAppxBundleFactory),
		reinterpret_cast<void**>(&bundleFactory)
	);
	if (FAILED (hr)) return hr;
	return bundleFactory->CreateBundleManifestReader (
		stream,
		output
	);
}
HRESULT ReadBundleManifestFromFile (const std::wstring &filePath, IAppxBundleManifestReader **output)
{
	if (!output) return E_INVALIDARG;
	CComPtr <IStream> fileStream = nullptr;
	HRESULT hr = SHCreateStreamOnFileEx (
		filePath.c_str (),
		STGM_READ | STGM_SHARE_DENY_WRITE,
		FILE_ATTRIBUTE_NORMAL,
		FALSE,
		nullptr,
		&fileStream
	);
	if (FAILED (hr)) return hr;
	return ReadBundleManifestFromStream (fileStream, output);
}
HRESULT GetBundleWriter (_In_ LPCWSTR outputFileName, _Outptr_ IAppxBundleWriter **writer, UINT64 version = 0)
{
	HRESULT hr = S_OK;
	IStream* outputStream = NULL;
	IAppxBundleFactory *appxBundleFactory = NULL;
	// Create a stream over the output file where the bundle will be written
	hr = SHCreateStreamOnFileEx (
		outputFileName,
		STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE,
		0, // default file attributes
		TRUE, // create file if it does not exist
		NULL, // no template
		&outputStream);
	// Create a new Appx Bundle factory
	if (SUCCEEDED (hr))
	{
		hr = CoCreateInstance (
			__uuidof (AppxBundleFactory),
			NULL,
			CLSCTX_INPROC_SERVER,
			__uuidof (IAppxBundleFactory),
			(LPVOID *)(&appxBundleFactory));
	}
	// Create a new bundle writer using the factory
	if (SUCCEEDED (hr))
	{
		hr = appxBundleFactory->CreateBundleWriter (
			outputStream,
			version, // by specifying 0, the bundle will have an automatically
					 // generated version number based on the current time
			writer);
	}
	if (appxBundleFactory != NULL)
	{
		appxBundleFactory->Release ();
		appxBundleFactory = NULL;
	}
	if (outputStream != NULL)
	{
		outputStream->Release ();
		outputStream = NULL;
	}
	return hr;
}

SYSTEMTIME GetSystemCurrentTime ()
{
	SYSTEMTIME st;
	GetLocalTime (&st);
	return st;
}
std::wstring GetTimestampForFileName (const std::wstring &format = L"%d%02d%02d%02d%02d%02d", const SYSTEMTIME &current = GetSystemCurrentTime ())
{
	WCHAR buf [128] = {0};
	swprintf (buf, format.c_str (),
		current.wYear, current.wMonth, current.wDay,
		current.wHour, current.wMinute, current.wSecond,
		current.wMilliseconds
	);
	return buf;
}

std::wstring GenerateRandomFileName (int length = 12)
{
	static const wchar_t charset [] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	const int charsetSize = sizeof (charset) / sizeof (charset [0]) - 1;
	std::wstring result;
	result.reserve (length);
	srand ((unsigned int)(GetTickCount64 () ^ GetCurrentProcessId () ^ time (NULL)));
	for (int i = 0; i < length; ++i)
	{
		int index = rand () % charsetSize;
		result += charset [index];
	}
	return result;
}

std::wstring GetTempFileNameW () { return GetTimestampForFileName (L"%d%02d%02d%02d%02d%02d.%03d") + GenerateRandomFileName (6) + L".tmp"; }

HRESULT CreateBundlePackageFile (const std::vector <std::wstring> &swaPkgFileList, const std::wstring &lpOutputDir, std::wstring &pswOutputFilePath, UINT64 u64Version = 0, std::wstring &lpOutputName = std::wstring (L""), bool bNameByPFN = true, std::function <void (int finished, int total, int progress)> fCallback = nullptr, IODIRECTION &ioDirector = IODirection ())
{
	HRESULT hr = E_FAIL;
	if (!swaPkgFileList.size ())
	{
		ioDirector.safeOutputLine (GetRCStringSW (IDS_PKGERROR_FINDFILES));
		return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
	}
	std::wstring idname = L"";
	UINT64 ver = u64Version;
	if (std::wnstring::empty (lpOutputName))
	{
		WCHAR buf [MAX_PATH + 1] = {0};
		lstrcpyW (buf, GetTempFileNameW ().c_str ());
		PathRemoveExtensionW (buf);
		lpOutputName = PathFindFileNameW (buf);
	}
	std::map <std::wstring, std::wstring> writename;
	for (auto it : swaPkgFileList)
	{
		if (!IsFileExists (it))
		{
			ioDirector.safeOutputLine (
				fmt::format (
					GetRCStringSW (IDS_PKGERROR_FILEEXIST).c_str (),
					it.c_str ()
				)
			);
			return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
		}
		package_info pread (it);
		if (!pread.is_valid)
		{
			ioDirector.safeOutputLine (
				fmt::format (
					GetRCStringSW (IDS_PKGERROR_PKGEXIST).c_str (),
					it.c_str ()
				)
			);
			return HRESULT_FROM_WIN32 (ERROR_INSTALL_INVALID_PACKAGE);
		}
		if (LabelEmpty (idname)) idname += pread.identity.name;
		std::wstring fname (L"");
		fname += StringTrim (StringTrim (idname).substr (0, 20) + L"_" + pread.identity.version.stringifyw ());
		std::vector <APPX_PACKAGE_ARCHITECTURE> archi;
		std::wstring label (L"");
		{
			push_unique (archi, pread.identity.architecture);
		}
		if (archi.size () && pread.package_type == PackageType::application)
		{
			for (auto ait : archi)
			{
				switch (ait)
				{
					case APPX_PACKAGE_ARCHITECTURE_NEUTRAL: label += L"_neutral"; break;
					case APPX_PACKAGE_ARCHITECTURE_X86: label += L"_x86"; break;
					case APPX_PACKAGE_ARCHITECTURE_X64: label += L"_x64"; break;
					case APPX_PACKAGE_ARCHITECTURE_ARM: label += L"_arm"; break;
					default:
						if ((int)ait == 12)//APPX_PACKAGE_ARCHITECTURE_ARM64
						{
							fname += L"_arm64"; break;
						}
				}
			}
		}
		if (pread.package_type != PackageType::application)
		{
			{
				std::vector <std::wnstring> langs = pread.resources.languages;
				for (auto &lit : langs)
				{
					label += L"_language-" + lit;
				}
				std::vector <UINT32> scales = pread.resources.scales;
				for (auto sit : scales)
				{
					std::wstringstream ss;
					ss << sit;
					label += L"_scale-" + ss.str ();
				}
				std::vector <DX_FEATURE_LEVEL> dxs = pread.resources.dxlevels;
				for (auto sit : scales)
				{
					switch (sit)
					{
						case DX_FEATURE_LEVEL_UNSPECIFIED: continue; break;
						case DX_FEATURE_LEVEL_9: label += L"_dx9"; break;
						case DX_FEATURE_LEVEL_10: label += L"_dx10"; break;
						case DX_FEATURE_LEVEL_11: label += L"_dx11"; break;
						case (DX_FEATURE_LEVEL)4: label += L"_dx12"; break;
					}
				}
			}
		}
		else
		{
			if (!ver)
			{
				version ver1 (pread.identity.version);
				ver1.major = GetSystemCurrentTime ().wYear;
				ver = ver1.data ();
			}
		}
		writename [it] = std::wstring (fname.c_str ()) + label + L".appx";
	}
	if (!IsDirectoryExists (lpOutputDir) && !CreateDirectoryW (lpOutputDir.c_str (), NULL))
	{
		ioDirector.safeOutputLine (GetRCStringSW (IDS_PKGERROR_DIRERROR));
		return HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND);
	}
	std::wstreambuf wsbuf ();
	WCHAR outpath [MAX_PATH + 1] = {0};
	bool usetemp = false;
	if (bNameByPFN)
	{
		PathCombineW (outpath, lpOutputDir.c_str (), (lpOutputName).c_str ());
		PathCombineW (outpath, outpath, (lpOutputName + L".appxbundle").c_str ());
	}
	else
	{
		WCHAR tempopath [MAX_PATH + 1] = {0};
		PathCombineW (tempopath, lpOutputDir.c_str (), (lpOutputName).c_str ());
		PathCombineW (tempopath, tempopath, (lpOutputName + L".appxbundle").c_str ());
		if (IsFileExists (tempopath))
		{
			std::wstring ts = GetTimestampForFileName ();
			lpOutputName = ts + L"_" + lpOutputName;
			ZeroMemory (tempopath, sizeof (WCHAR) * (MAX_PATH + 1));
			PathCombineW (tempopath, lpOutputDir.c_str (), (lpOutputName).c_str ());
			PathCombineW (tempopath, tempopath, (lpOutputName + L".appxbundle").c_str ());
			lstrcpyW (outpath, tempopath);
		}
		else lstrcpyW (outpath, tempopath);
	}
	if (!IsDirectoryExists (GetFileDirectoryW (outpath)))
	{
		if (!CreateDirectoryW (GetFileDirectoryW (outpath).c_str (), NULL))
		{
			ioDirector.safeOutputLine (GetRCStringSW (IDS_PKGERROR_DIRERROR));
			return HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND);
		}
	}
	struct EndTask
	{
		using CleanupFunc = void (*)(LPCWSTR fpath, HRESULT hr);
		CleanupFunc end = nullptr;
		LPCWSTR fpath;
		HRESULT &hr;
		EndTask (LPCWSTR _fpath, HRESULT &_hr)
			: fpath (_fpath), hr (_hr)
		{
			end = [] (LPCWSTR lpFileName, HRESULT hr)
			{
				if (FAILED (hr) && IsFileExists (lpFileName))
				{
					DeleteFileW (lpFileName);
					RemoveDirectoryW (GetFileDirectoryW (lpFileName).c_str ());
				}
			};
		}
		~EndTask () {
			if (end) end (fpath, hr);
		}
	};
	EndTask endtask (outpath, hr);
	IAppxBundleWriter *bwrit = nullptr;
	raii releasep ([&bwrit] () {
		if (bwrit) bwrit->Release ();
		bwrit = nullptr;
	});
	hr = GetBundleWriter (outpath, &bwrit, ver);
	if (FAILED (hr))
	{
		ioDirector.safeOutputLine (GetRCStringSW (IDS_PKGERROR_FSTREAMOPEN));
		return hr;
	}
	for (size_t cnt = 0; cnt < swaPkgFileList.size (); cnt ++)
	{
		CComPtr <IStream> ifile = nullptr;
		hr = SHCreateStreamOnFileEx (
			swaPkgFileList [cnt].c_str (),
			STGM_READ | STGM_SHARE_EXCLUSIVE,
			0, // default file attributes
			FALSE, // do not create new file
			NULL, // no template
			&ifile);
		if (FAILED (hr))
		{
			ioDirector.safeOutputLine ((L"Error: cannot create the stream of file \"" + swaPkgFileList [cnt] + L"\".").c_str ());
			return hr;
		}
		hr = bwrit->AddPayloadPackage (writename [swaPkgFileList [cnt]].c_str (), ifile);
		if (FAILED (hr))
		{
			ioDirector.safeOutputLine ((L"Error: cannot add payload package \"" + swaPkgFileList [cnt] + L"\" to \"" + writename [swaPkgFileList [cnt]] + L"\".").c_str ());
			return hr;
		}
		std::wstringstream ss;
		ss << "[" << (cnt + 1) << "/" << swaPkgFileList.size () << "]\t" << "Added package named \"" << writename [swaPkgFileList [cnt]] << "\" from package \"" << swaPkgFileList [cnt] << "\".";
		ioDirector.safeOutputLine (ss.str ().c_str ());
		if (fCallback) fCallback ((int)cnt  + 1, (int)swaPkgFileList.size (), (int)((double)(cnt + 1) / (double)swaPkgFileList.size () * 100));
	}
	hr = bwrit->Close ();
	if (FAILED (hr))
	{
		ioDirector.safeOutputLine (GetRCStringSW (IDS_PKGERROR_WRITERCLOSE));
		return hr;
	}
	pswOutputFilePath = outpath;
	if (bNameByPFN)
	{
		bwrit->Release ();
		bwrit = nullptr;
		std::wstring bpfn = GetBundlePackageFullName (outpath);
		if (!std::wnstring::empty (bpfn))
		{
			std::wstring oldFilePath = outpath;
			std::wstring oldDir = GetFileDirectoryW (oldFilePath);
			std::wstring newFileName = bpfn + L".appxbundle";
			std::wstring newFilePath = GetFileDirectoryW (oldFilePath) + L"\\" + newFileName;
			if (!MoveFileExW (oldFilePath.c_str (), newFilePath.c_str (), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
			{
				ioDirector.safeOutputLine (fmt::format (
					GetRCStringSW (IDS_PKGWARN_NAMEFILE).c_str (),
					newFileName.c_str ()
				));
			}
			else
			{
				std::wstring newDir = GetFileDirectoryW (oldDir) + L"\\" + bpfn;
				if (IsDirectoryExists (newDir))
				{
					newDir = GetFileDirectoryW (oldDir) + L"\\" + GetTimestampForFileName () + L"_" + bpfn;
				}
				if (oldDir != newDir)
				{
					if (!MoveFileExW (oldDir.c_str (), newDir.c_str (), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING))
					{
						ioDirector.safeOutputLine (fmt::format (GetRCStringSW (IDS_PKGWARN_NAMEDIR).c_str (), oldDir.c_str (), newDir.c_str ()));
					}
					else
					{
						newFilePath = newDir + L"\\" + newFileName;
					}
				}
				pswOutputFilePath = newFilePath;
			}
		}
	}
	{
		if (fCallback)fCallback ((int)swaPkgFileList.size (), (int)swaPkgFileList.size (), 100);
		ioDirector.safeOutputLine (fmt::format (GetRCStringSW (IDS_PKEOUTPUT_FINISH).c_str (), swaPkgFileList.size (), pswOutputFilePath.c_str ()));
	}
	return S_OK;
}