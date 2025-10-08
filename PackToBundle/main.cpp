#include <Windows.h>
#include <CommCtrl.h>
#include <vector>
#include <string>
#include <algorithm>
#include <type_traits>
#include <iterator>
#include <WinUser.h>
#include <type_traits>
#include <memory>
#include <pugiconfig.hpp>
#include <pugixml.hpp>
#include <ShlObj.h>
#include <thread>
#include <wincrypt.h>
#include <fmt/format.h>
#include <Psapi.h>
#include "heap.h"
#include "module.h"
#include "pkgread.h"
#include "pkgwrite.h"
#include "rctools.h"
#include "resource.h"
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
int GetDPI ()
{
	HDC hDC = GetDC (NULL);
	int DPI_A = (int)(((double)GetDeviceCaps (hDC, 118) / (double)GetDeviceCaps (hDC, 8)) * 100);
	int DPI_B = (int)(((double)GetDeviceCaps (hDC, 88) / 96) * 100);
	ReleaseDC (NULL, hDC);
	if (DPI_A == 100) return DPI_B;
	else if (DPI_B == 100) return DPI_A;
	else if (DPI_A == DPI_B) return DPI_A;
	else return 0;
}
template <typename t> t max (const t v1, const t v2) { if (v1 > v2) return v1; else return v2; }
template <typename t> t min (const t v1, const t v2) { if (v1 < v2) return v1; else return v2; }
class AutoFont
{
	public:
	static AutoFont Create (
		const wchar_t *faceName,
		int height,
		int weight = FW_NORMAL,
		bool italic = false,
		bool underline = false,
		bool strikeOut = false,
		DWORD charSet = DEFAULT_CHARSET
	)
	{
		LOGFONT lf = {0};
		const float scaling = GetDPI () * 0.01f;
		lf.lfHeight = -static_cast <LONG> (std::abs (height) * scaling);
		lf.lfWeight = weight;
		lf.lfItalic = italic;
		lf.lfUnderline = underline;
		lf.lfStrikeOut = strikeOut;
		lf.lfCharSet = charSet;
		wcscpy_s (lf.lfFaceName, faceName);
		HFONT hFont = ::CreateFontIndirect (&lf);
		if (!hFont)
		{
			throw std::system_error (
				std::error_code (::GetLastError (), std::system_category ()),
				"CreateFontIndirect failed"
			);
		}
		return AutoFont (hFont);
	}
	static AutoFont FromHandle (HFONT hFont, bool takeOwnership = false)
	{
		if (!hFont) return AutoFont (nullptr);
		if (takeOwnership) return AutoFont (hFont);
		else
		{
			LOGFONT lf;
			if (0 == ::GetObjectW (hFont, sizeof (LOGFONT), &lf))
			{
				throw std::system_error (
					std::error_code (::GetLastError (), std::system_category ()),
					"GetObject failed"
				);
			}
			HFONT newFont = ::CreateFontIndirect (&lf);
			if (!newFont)
			{
				throw std::system_error
				(
					std::error_code (::GetLastError (), std::system_category ()),
					"CreateFontIndirect failed"
				);
			}
			return AutoFont (newFont);
		}
	}
	explicit AutoFont (HFONT hFont = nullptr): m_hFont (hFont) {}
	~AutoFont () { Release (); }
	AutoFont (const AutoFont &) = delete;
	AutoFont &operator=(const AutoFont &) = delete;
	AutoFont (AutoFont &&other) noexcept : m_hFont (other.m_hFont)
	{
		other.m_hFont = nullptr;
	}
	AutoFont &operator = (AutoFont &&other) noexcept
	{
		if (this != &other)
		{
			Release ();
			m_hFont = other.m_hFont;
			other.m_hFont = nullptr;
		}
		return *this;
	}
	HFONT Get () const noexcept { return m_hFont; }
	void Release () noexcept
	{
		if (m_hFont)
		{
			::DeleteObject (m_hFont);
			m_hFont = nullptr;
		}
	}
	void Destroy () noexcept { Release (); }
	explicit operator bool () const noexcept { return m_hFont != nullptr; }
	private:
	HFONT m_hFont = nullptr;
};
void SetControlFont (AutoFont &font, HWND hWnd) { SendMessageW (hWnd, WM_SETFONT, (WPARAM)font.Get (), TRUE); }
void InitListControl (HWND hList, const std::vector <std::wstring> &columnNames)
{
	ListView_DeleteAllItems (hList);
	DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle (hList, exStyle);
	int colCount = Header_GetItemCount (ListView_GetHeader (hList));
	for (int i = colCount - 1; i >= 0; i --)
	{
		ListView_DeleteColumn (hList, i);
	}
	for (size_t i = 0; i < columnNames.size (); i++)
	{
		LVCOLUMN lvc = {0};
		lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = 100;
		lvc.pszText = const_cast <LPWSTR> (columnNames [i].c_str ());
		ListView_InsertColumn (hList, i, &lvc);
	}
	RECT rcList;
	GetClientRect (hList, &rcList);
	int listWidth = rcList.right - rcList.left;
	int totalWidth = 0;
	int lastColumn = static_cast <int> (columnNames.size ()) - 1;
	for (int i = 0; i < lastColumn; i ++)
	{
		ListView_SetColumnWidth (hList, i, LVSCW_AUTOSIZE);
		totalWidth += ListView_GetColumnWidth (hList, i);
	}
	int lastColWidth = max (50, listWidth - totalWidth);
	ListView_SetColumnWidth (hList, lastColumn, lastColWidth);
	if (totalWidth + lastColWidth < listWidth)
	{
		ListView_SetColumnWidth (hList, lastColumn, listWidth - totalWidth);
	}
}
void AddListContent (
	HWND hList,
	const std::vector<std::wstring>& columnNames,
	const std::vector<std::vector<std::wstring>>& data,
	bool bUniqueColumn = false,
	std::function<bool (const std::vector<std::wstring>&, const std::vector<std::wstring>&)> pfCompare = nullptr,
	std::function<void (const std::vector<std::wstring>&)> pfFailedCallback = nullptr,
	int maxColumnWidth = 500 // limit for all columns
)
{
	size_t columnCount = columnNames.size ();
	if (columnCount == 0) return;

	// ----------------------------------------------------
	// 1. Deduplication: compare with both existing and new rows
	// ----------------------------------------------------
	std::vector<std::vector<std::wstring>> filteredData;

	for (size_t i = 0; i < data.size (); ++i)
	{
		const auto& row = data [i];
		if (row.empty ()) continue;

		bool duplicate = false;

		if (bUniqueColumn && pfCompare)
		{
			// Check against rows already added to filteredData
			for (size_t j = 0; j < filteredData.size (); ++j)
			{
				if (pfCompare (row, filteredData [j]))
				{
					duplicate = true;
					break;
				}
			}

			// Check against existing ListView items
			if (!duplicate)
			{
				int existingCount = ListView_GetItemCount (hList);
				for (int k = 0; k < existingCount; ++k)
				{
					std::vector<std::wstring> existingRow;
					for (size_t c = 0; c < columnCount; ++c)
					{
						wchar_t buf [512] = {0};
						ListView_GetItemText (hList, k, static_cast<int>(c), buf, 511);
						existingRow.push_back (buf);
					}
					if (pfCompare (row, existingRow))
					{
						duplicate = true;
						break;
					}
				}
			}
		}

		if (!duplicate)
			filteredData.push_back (row);
		else if (pfFailedCallback)
			pfFailedCallback (row);
	}

	// ----------------------------------------------------
	// 2. Insert rows
	// ----------------------------------------------------
	for (size_t i = 0; i < filteredData.size (); i++)
	{
		const auto& row = filteredData [i];
		if (row.empty ()) continue;

		LVITEM lvi = {0};
		lvi.mask = LVIF_TEXT;
		lvi.iItem = ListView_GetItemCount (hList);
		lvi.iSubItem = 0;
		lvi.pszText = const_cast<LPWSTR>(row [0].c_str ());
		if (ListView_InsertItem (hList, &lvi) == -1)
		{
			if (pfFailedCallback)
				pfFailedCallback (row);
			continue;
		}

		for (size_t j = 1; j < std::min<size_t> (row.size (), columnCount); j++)
		{
			ListView_SetItemText (hList, static_cast<int>(lvi.iItem), static_cast<int>(j),
				const_cast<LPWSTR>(row [j].c_str ()));
		}
	}

	// ----------------------------------------------------
	// 3. Adjust column widths
	// ----------------------------------------------------
	RECT rcList;
	GetClientRect (hList, &rcList);
	int listWidth = rcList.right - rcList.left;

	HDC hdc = GetDC (hList);
	HFONT hOldFont = (HFONT)SelectObject (hdc, (HFONT)SendMessage (hList, WM_GETFONT, 0, 0));

	int totalWidth = 0;
	int lastColumn = static_cast<int>(columnCount) - 1;

	for (int col = 0; col < static_cast<int>(columnCount); ++col)
	{
		int maxItemWidth = 0;
		std::vector<int> itemWidths;

		int itemCount = ListView_GetItemCount (hList);
		for (int i = 0; i < itemCount; ++i)
		{
			wchar_t buf [512] = {0};
			ListView_GetItemText (hList, i, col, buf, 511);
			SIZE sz;
			GetTextExtentPoint32W (hdc, buf, lstrlenW (buf), &sz);
			itemWidths.push_back (sz.cx);
		}

		std::sort (itemWidths.begin (), itemWidths.end ());

		const std::wstring& header = columnNames [col];
		SIZE headerSize = {0};
		GetTextExtentPoint32W (hdc, header.c_str (), static_cast<int>(header.length ()), &headerSize);
		int headerWidth = headerSize.cx;

		bool headerDominant = (!itemWidths.empty () && headerWidth > itemWidths [itemWidths.size () * 2 / 3]);
		if (headerDominant) itemWidths.push_back (headerWidth);

		if (!itemWidths.empty ())
			maxItemWidth = *std::max_element (itemWidths.begin (), itemWidths.end ());
		else
			maxItemWidth = headerWidth;

		maxItemWidth += 20;
		if (maxItemWidth > maxColumnWidth)
			maxItemWidth = maxColumnWidth;

		ListView_SetColumnWidth (hList, col, maxItemWidth);

		if (col != lastColumn)
			totalWidth += maxItemWidth;
	}

	// ----------------------------------------------------
	// 4. Last column adjustment — add extra width
	// ----------------------------------------------------
	const std::wstring& lastHeader = columnNames [lastColumn];
	SIZE lastHeaderSize = {0};
	GetTextExtentPoint32W (hdc, lastHeader.c_str (), static_cast<int>(lastHeader.length ()), &lastHeaderSize);
	int lastHeaderWidth = lastHeaderSize.cx;

	int maxLastItemWidth = 0;
	std::vector<int> lastItemWidths;
	int itemCount = ListView_GetItemCount (hList);
	for (int i = 0; i < itemCount; ++i)
	{
		wchar_t buf [512] = {0};
		ListView_GetItemText (hList, i, lastColumn, buf, 511);
		SIZE sz;
		GetTextExtentPoint32W (hdc, buf, lstrlenW (buf), &sz);
		lastItemWidths.push_back (sz.cx);
	}

	std::sort (lastItemWidths.begin (), lastItemWidths.end ());
	bool lastHeaderDominant = (!lastItemWidths.empty () && lastHeaderWidth > lastItemWidths [lastItemWidths.size () * 2 / 3]);
	if (lastHeaderDominant) lastItemWidths.push_back (lastHeaderWidth);
	if (!lastItemWidths.empty ()) maxLastItemWidth = *std::max_element (lastItemWidths.begin (), lastItemWidths.end ());
	else maxLastItemWidth = lastHeaderWidth;

	maxLastItemWidth += 20;
	if (maxLastItemWidth > maxColumnWidth)
		maxLastItemWidth = maxColumnWidth;

	int remaining = listWidth - totalWidth;
	int desiredWidth = maxLastItemWidth;
	if (remaining > 0)
		desiredWidth += remaining;
	else
		desiredWidth = std::max (100, desiredWidth + remaining);
	desiredWidth += 100;

	ListView_SetColumnWidth (hList, lastColumn, desiredWidth);

	SelectObject (hdc, hOldFont);
	ReleaseDC (hList, hdc);
}
void SetListContent (HWND hList, const std::vector <std::wstring> &columnNames, const std::vector <std::vector <std::wstring>> &data)
{
	size_t columnCount = columnNames.size ();
	if (columnCount == 0) return;
	ListView_DeleteAllItems (hList);
	AddListContent (hList, columnNames, data);
}
size_t GetSelectedItemValue (HWND hListView, std::vector <uint64_t> &result)
{
	result.clear ();
	int count = ListView_GetSelectedCount (hListView);
	if (count <= 0) return result.size ();
	int iItem = -1;
	for (int i = 0; i < count; ++ i)
	{
		iItem = ListView_GetNextItem (hListView, iItem, LVNI_SELECTED);
		if (iItem == -1) break;
		LVITEM item = {0};
		item.mask = LVIF_PARAM;
		item.iItem = iItem;
		if (ListView_GetItem (hListView, &item))
		{
			result.push_back ((uint64_t)item.lParam);
		}
	}
	return result.size ();
}
size_t GetListSelectedCount (HWND hList, std::vector <int64_t> &selectedIndices)
{
	selectedIndices.clear ();
	wchar_t className [64] = {0};
	GetClassNameW (hList, className, 63);
	if (wcscmp (className, L"SysListView32") == 0)
	{
		// ListView 类型
		int itemCount = ListView_GetItemCount (hList);

		for (int i = 0; i < itemCount; ++i)
		{
			if (ListView_GetItemState (hList, i, LVIS_SELECTED) & LVIS_SELECTED)
			{
				selectedIndices.push_back (static_cast<int64_t>(i));
			}
		}
	}
	else if (wcscmp (className, L"SysListBox") == 0)
	{
		int selCount = (int)SendMessage (hList, LB_GETSELCOUNT, 0, 0);
		if (selCount > 0)
		{
			std::vector<int> selItems (selCount);
			SendMessage (hList, LB_GETSELITEMS, selCount, (LPARAM)selItems.data ());
			for (int index : selItems)
				selectedIndices.push_back (static_cast<int64_t>(index));
		}
	}
	else
	{
	}
	return selectedIndices.size ();
}
size_t GetCheckedItemValue (HWND hListView, std::vector <uint64_t> &result)
{
	result.clear ();
	int count = ListView_GetItemCount (hListView);
	if (count <= 0) return result.size ();
	for (int i = 0; i < count; ++ i)
	{
		if (ListView_GetCheckState (hListView, i))
		{
			LVITEM item = {0};
			item.mask = LVIF_PARAM;
			item.iItem = i;
			if (ListView_GetItem (hListView, &item))
			{
				result.push_back ((uint64_t)item.lParam);
			}
		}
	}

	return result.size ();
}
size_t GetListViewItemRow (HWND hList, int index, std::vector <std::wstring> &row)
{
	row.clear ();
	int colCount = Header_GetItemCount (ListView_GetHeader (hList));

	for (int col = 0; col < colCount; ++col)
	{
		size_t bufferlen = 256;

		while (true)
		{
			std::unique_ptr<WCHAR []> lpbuf (new WCHAR [bufferlen] ());
			ListView_GetItemText (hList, index, col, lpbuf.get (), (int)bufferlen);
			lpbuf [bufferlen - 1] = 0;

			if (lstrlenW (lpbuf.get ()) < bufferlen - 1)
			{
				row.emplace_back (lpbuf.get ());
				break;
			}
			else if (bufferlen > 65536)
			{
				row.emplace_back (lpbuf.get ());
				break;
			}
			else bufferlen += 256;
		}
	}
	return row.size ();
}
void RemoveListViewByCount (HWND hList, int index)
{
	if (!hList) return;
	int count = ListView_GetItemCount (hList);
	if (index < 0 || index >= count) return;
	ListView_DeleteItem (hList, index);
}
void RemoveListViewByCount (HWND hList, std::vector <int> &indexs)
{
	if (!hList) return;
	int count = ListView_GetItemCount (hList);
	if (count <= 0 || indexs.empty ()) return;
	std::sort (indexs.begin (), indexs.end (), std::greater<int> ());
	for (int idx : indexs)
	{
		if (idx >= 0 && idx < count)
		{
			ListView_DeleteItem (hList, idx);
		}
	}
}
void RemoveListViewByCount (HWND hList, std::vector <int64_t> &indexs)
{
	if (!hList) return;
	int count = ListView_GetItemCount (hList);
	if (count <= 0 || indexs.empty ()) return;
	std::sort (indexs.begin (), indexs.end (), std::greater<int> ());
	for (int idx : indexs)
	{
		if (idx >= 0 && idx < count)
		{
			ListView_DeleteItem (hList, idx);
		}
	}
}
#undef max
#undef min
template <typename T> size_t MergeVectorsKeepSame (const std::vector <std::vector <T>> &arrays, std::vector <T> &result)
{
	using vector = std::vector <T>;
	result.clear ();
	size_t min_length = 0;
	for (auto &it : arrays) min_length = std::max (min_length, it.size ());
	for (auto &it : arrays) min_length = std::min (min_length, it.size ());
	if (arrays.size () > 0)
	{
		for (size_t i = 0; i < min_length; i ++)
		{
			result.push_back (arrays [0] [i]);
		}
	}
	for (size_t i = 0; i < min_length; i ++)
	{
		for (auto &it : arrays)
		{
			auto &curr = it [i];
			if (result [i] == curr) continue;
			else
			{
				auto &res_it = result.begin () + i;
				*res_it = T ();
				break;
			}
		}
	}
	return result.size ();
}
static std::wstring lastfile = L"";
size_t ExploreFile (HWND hParent, std::vector <std::wstring> &results, LPWSTR lpFilter = L"Windows Store App Package (*.appx)\0*.appx", DWORD dwFlags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST, const std::wstring &swWndTitle = std::wstring (L"Please select the file(-s): "), const std::wstring &swInitDir = GetFileDirectoryW (lastfile))
{
	results.clear ();
	const DWORD BUFFER_SIZE = 65536; // 64KB
	std::vector <WCHAR> buffer (BUFFER_SIZE, 0);
	OPENFILENAME ofn;
	ZeroMemory (&ofn, sizeof (ofn));
	ofn.hwndOwner = hParent;
	ofn.lpstrFile = (LPWSTR)buffer.data ();
	ofn.nMaxFile = BUFFER_SIZE;
	ofn.lpstrFilter = lpFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrTitle = swWndTitle.c_str ();
	ofn.Flags = dwFlags;
	ofn.lpstrInitialDir = swInitDir.c_str ();
	ofn.lStructSize = sizeof (ofn);
	if (GetOpenFileNameW (&ofn))
	{
		LPCWSTR p = buffer.data ();
		std::wstring dir = p;
		p += dir.length () + 1;
		if (*p == 0) results.push_back (dir);
		else
		{
			while (*p)
			{
				std::wstring fullPath = dir + L"\\" + p;
				results.push_back (fullPath);
				p += wcslen (p) + 1;
			}
		}
		if (!results.empty ()) lastfile = results.back ();
	}
	return results.size ();
}
int CALLBACK BrowseCallbackProc (HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		SendMessageW (hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}
	return 0;
}
static std::wstring lastdir = L"";
std::wstring ExploreDirectory (HWND hParent, UINT uFlag = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE, LPCWSTR lpWndTitle = L"Please select a folder: ")
{
	WCHAR path [MAX_PATH] = {0};
	BROWSEINFO bi = {0};
	bi.hwndOwner = hParent;
	bi.lpszTitle = lpWndTitle;
	bi.ulFlags = uFlag;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM)lastdir.c_str ();
	LPITEMIDLIST pidl = SHBrowseForFolderW (&bi);
	if (pidl)
	{
		SHGetPathFromIDListW (pidl, path);
		std::wstring res (L"");
		if (path) res += path;
		lastdir = res;
		CoTaskMemFree (pidl);
		return res;
	}
	return L"";
}
static std::vector <std::wstring> thead = {
	GetRCStringSW (IDS_THEAD_FILENAME),
	GetRCStringSW (IDS_THEAD_FILEPATH),
	GetRCStringSW (IDS_THEAD_PKGTYPE),
	GetRCStringSW (IDS_THEAD_PKGRES),
	GetRCStringSW (IDS_THEAD_RESVALUE)
};
size_t GetTableRowFromPackage (const std::wstring &filepath, std::vector <std::wstring> &row)
{
	package_info inf (filepath);
	if (!inf.is_valid) return row.size ();
	row.push_back (PathFindFileNameW (filepath.c_str ()));
	{
		std::vector <WCHAR> buf (filepath.size () + 1);
		lstrcpyW (buf.data (), filepath.c_str ());
		PathRemoveFileSpecW (buf.data ());
		row.emplace_back (buf.data ());
	}
	switch (inf.package_type)
	{
		case PackageType::application:
		{
			row.push_back (GetRCStringSW (IDS_TBODY_PKGTYPE_APP));
			row.push_back (GetRCStringSW (IDS_TBODY_PKGRES_ARCHI));

			switch (inf.identity.architecture)
			{
				case APPX_PACKAGE_ARCHITECTURE_ARM:      row.push_back (L"Arm"); break;
				case APPX_PACKAGE_ARCHITECTURE_NEUTRAL:  row.push_back (L"Neutral"); break;
				case APPX_PACKAGE_ARCHITECTURE_X64:      row.push_back (L"x64"); break;
				case APPX_PACKAGE_ARCHITECTURE_X86:      row.push_back (L"x86"); break;
				case (APPX_PACKAGE_ARCHITECTURE)12:      row.push_back (L"Arm64"); break;
				default:                                 row.push_back (L"Unknown"); break;
			}
		}
		break;

		case PackageType::resource:
		{
			row.push_back (GetRCStringSW (IDS_TBODY_PKGTYPE_RES));

			// 资源类型列
			{
				std::vector<std::wstring> restype;
				if (!inf.resources.languages.empty ())
					restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_LANG));
				if (!inf.resources.scales.empty ())
					restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_SCALE));
				if (!inf.resources.dxlevels.empty ())
					restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_DX));

				std::wstring restypeline;
				for (size_t i = 0; i < restype.size (); ++i)
				{
					if (i)
						restypeline += L", ";
					restypeline += restype [i];
				}
				row.push_back (restypeline);
			}

			// 资源值列
			{
				std::wstring resvaluerow;
				std::vector<std::wstring> parts;

				// Languages
				if (!inf.resources.languages.empty ())
				{
					std::wstringstream ss;
					ss << L"[";
					for (size_t i = 0; i < inf.resources.languages.size (); ++i)
					{
						if (i)
							ss << L", ";
						ss << inf.resources.languages [i];
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				// Scales
				if (!inf.resources.scales.empty ())
				{
					std::wstringstream ss;
					ss << L"[";
					for (size_t i = 0; i < inf.resources.scales.size (); ++i)
					{
						if (i)
							ss << L", ";
						ss << inf.resources.scales [i];
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				// DX levels
				if (!inf.resources.dxlevels.empty ())
				{
					std::wstringstream ss;
					ss << L"[";
					for (size_t i = 0; i < inf.resources.dxlevels.size (); ++i)
					{
						if (i)
							ss << L", ";
						DX_FEATURE_LEVEL dx = inf.resources.dxlevels [i];
						switch (dx)
						{
							case DX_FEATURE_LEVEL_9:  ss << L"dx9"; break;
							case DX_FEATURE_LEVEL_10: ss << L"dx10"; break;
							case DX_FEATURE_LEVEL_11: ss << L"dx11"; break;
							case (DX_FEATURE_LEVEL)4: ss << L"dx12"; break;
						}
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				// Join all parts with " | "
				for (size_t i = 0; i < parts.size (); ++i)
				{
					if (i)
						resvaluerow += L" | ";
					resvaluerow += parts [i];
				}

				row.push_back (resvaluerow);
			}
		}
		break;
	}

	return row.size ();
}
#define ARRTS_DIV_RIGHT_SPACE    0x00000001  // 成员分割符右侧有空格 ", "
#define ARRTS_DIV_LEFT_SPACE     0x00000002  // 成员分割符左侧有空格 " ,"
#define ARRTS_EDGE_RIGHT_SPACE   0x00000004  // 边缘括弧右侧有空格 "[3, 2 ]"
#define ARRTS_EDGE_LEFT_SPACE    0x00000008  // 边缘括弧左侧有空格 "[ 3, 2]"
#define ARRTS_SINGLE_NO_QUOTE    0x00000010  // 单个成员时不用括弧 "1"
#define ARRTS_EMPTY_NO_QUOTE     0x00000020  // 无成员时不用括弧 ""
#define ARRTS_ENABLE_RIGHT_QUOTE 0x00000040  // 启用右括弧
#define ARRTS_ENABLE_LEFT_QUOTE  0x00000080  // 启用左括弧
#define ARRTS_EMPTY_SPACE_QUOTE  0x00000100  // 无成员时括弧中只有一个空格 "[ ]"
namespace selfns
{
	inline bool HasFlag (DWORD dwFlags, DWORD style) {
		return (dwFlags & static_cast<DWORD>(style)) != 0;
	}

	template<typename ArrayType>
	std::wstring GetArrayText (
		const ArrayType& aArray,
		DWORD dwFlags =
		ARRTS_DIV_RIGHT_SPACE |
		ARRTS_EMPTY_NO_QUOTE |
		ARRTS_ENABLE_LEFT_QUOTE |
		ARRTS_ENABLE_RIGHT_QUOTE |
		ARRTS_SINGLE_NO_QUOTE,
		WCHAR wchDivide = L',',
		WCHAR wchLeftQuote = L'[',
		WCHAR wchRightQuote = L']',
		std::function<std::wstring (const typename std::decay<decltype(*std::begin (aArray))>::type&)> pfProcess = nullptr
	)
	{
		using MemberType = typename std::decay<decltype(*std::begin (aArray))>::type;

		bool isEmpty = std::begin (aArray) == std::end (aArray);
		size_t count = std::distance (std::begin (aArray), std::end (aArray));

		std::wstring result;

		// 空成员处理
		if (isEmpty) {
			if (!HasFlag (dwFlags, ARRTS_EMPTY_NO_QUOTE)) {
				if (HasFlag (dwFlags, ARRTS_ENABLE_LEFT_QUOTE))
					result += wchLeftQuote;

				if (HasFlag (dwFlags, ARRTS_EMPTY_SPACE_QUOTE))
					result += L" ";

				if (HasFlag (dwFlags, ARRTS_ENABLE_RIGHT_QUOTE))
					result += wchRightQuote;
			}
			return result;
		}

		// 单成员处理
		if (count == 1 && HasFlag (dwFlags, ARRTS_SINGLE_NO_QUOTE)) {
			auto it = std::begin (aArray);
			if (pfProcess)
				result += pfProcess (*it);
			else
				result += std::to_wstring (*it);
			return result;
		}

		// 左括弧
		if (HasFlag (dwFlags, ARRTS_ENABLE_LEFT_QUOTE)) {
			result += wchLeftQuote;
			if (HasFlag (dwFlags, ARRTS_EDGE_LEFT_SPACE))
				result += L" ";
		}

		auto it = std::begin (aArray);
		for (size_t i = 0; i < count; ++i, ++it) {
			if (pfProcess)
				result += pfProcess (*it);
			else
				result += std::to_wstring (*it);

			if (i + 1 < count) {
				if (HasFlag (dwFlags, ARRTS_DIV_LEFT_SPACE))
					result += L" ";

				result += wchDivide;

				if (HasFlag (dwFlags, ARRTS_DIV_RIGHT_SPACE))
					result += L" ";
			}
		}

		// 右括弧
		if (HasFlag (dwFlags, ARRTS_ENABLE_RIGHT_QUOTE))
		{
			if (HasFlag (dwFlags, ARRTS_EDGE_RIGHT_SPACE))
				result += L" ";
			result += wchRightQuote;
		}

		return result;
	}
};
size_t GetPackageInfoForDisplay (const std::wstring &filepath, std::vector <std::wnstring> &vec)
{
	vec.clear ();
	package_info inf (filepath);
	if (!inf.is_valid) return vec.size ();
	vec.push_back (filepath);
	vec.push_back (inf.identity.name);
	vec.push_back (inf.identity.publisher);
	vec.push_back (inf.identity.version.stringifyw ());

	switch (inf.package_type)
	{
		case PackageType::application:
		{
			vec.push_back (GetRCStringSW (IDS_TBODY_PKGTYPE_APP));
			switch (inf.identity.architecture)
			{
				case APPX_PACKAGE_ARCHITECTURE_ARM:      vec.push_back (L"Arm"); break;
				case APPX_PACKAGE_ARCHITECTURE_NEUTRAL:  vec.push_back (L"Neutral"); break;
				case APPX_PACKAGE_ARCHITECTURE_X64:      vec.push_back (L"x64"); break;
				case APPX_PACKAGE_ARCHITECTURE_X86:      vec.push_back (L"x86"); break;
				case (APPX_PACKAGE_ARCHITECTURE)12:      vec.push_back (L"Arm64"); break;
				default:                                 vec.push_back (L"");
			}

			// 资源类型描述
			{
				std::vector<std::wstring> restype;
				if (!inf.resources.languages.empty ()) restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_LANG));
				if (!inf.resources.scales.empty ())    restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_SCALE));
				if (!inf.resources.dxlevels.empty ())  restype.push_back (GetRCStringSW (IDS_TBODY_PKGRES_DX));

				std::wstring restypeline;
				for (size_t i = 0; i < restype.size (); ++i)
				{
					if (i) restypeline += L", ";
					restypeline += restype [i];
				}
				vec.push_back (restypeline);
			}

			// 资源值描述
			{
				std::wstring resvaluerow;
				std::vector<std::wstring> parts;

				// Languages
				if (!inf.resources.languages.empty ())
				{
					std::wstring format = GetRCStringSW (IDS_TBODY_RESVALUE_LANG);
					std::wstringstream ss;
					ss << L"[\r\n";
					for (size_t i = 0; i < inf.resources.languages.size (); ++i)
					{
						const std::wstring& lang = inf.resources.languages [i];
						WCHAR buf [100] = {0};
						GetLocaleInfoEx (lang.c_str (), 2, buf, 86);

						size_t len = lstrlenW (buf);
						if (len)
						{
							std::vector<WCHAR> lpbuf (format.length () + len + lang.length () + 4, 0);
							swprintf (lpbuf.data (), lpbuf.size (), format.c_str (), buf, lang.c_str ());

							ss << L"    " << lpbuf.data ();
							if (i) ss << L", ";
							ss << L"\r\n";
						}
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				// Scales
				if (!inf.resources.scales.empty ())
				{
					std::wstringstream ss;
					ss << L"[";
					for (size_t i = 0; i < inf.resources.scales.size (); ++i)
					{
						if (i) ss << L", ";
						ss << inf.resources.scales [i];
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				// DX Levels
				if (!inf.resources.dxlevels.empty ())
				{
					std::wstringstream ss;
					ss << L"[";
					for (size_t i = 0; i < inf.resources.dxlevels.size (); ++i)
					{
						if (i) ss << L", ";
						DX_FEATURE_LEVEL dx = inf.resources.dxlevels [i];
						switch (dx)
						{
							case DX_FEATURE_LEVEL_9:  ss << L"dx9"; break;
							case DX_FEATURE_LEVEL_10: ss << L"dx10"; break;
							case DX_FEATURE_LEVEL_11: ss << L"dx11"; break;
							case (DX_FEATURE_LEVEL)4: ss << L"dx12"; break;
						}
					}
					ss << L"]";
					parts.push_back (ss.str ());
				}

				for (size_t i = 0; i < parts.size (); ++i)
				{
					if (i) resvaluerow += L"\r\n";
					resvaluerow += parts [i];
				}
				vec.push_back (resvaluerow);
			}
		}
		break;

		case PackageType::resource:
		{
			vec.push_back (GetRCStringSW (IDS_TBODY_PKGTYPE_RES));
			vec.push_back (L"");

			std::wstring res;
			if (!inf.resources.languages.empty ())
			{
				vec.push_back (GetRCStringSW (IDS_TBODY_PKGRES_LANG));
				std::wstring format = GetRCStringSW (IDS_TBODY_RESVALUE_LANG);

				for (size_t i = 0; i < inf.resources.languages.size (); ++i)
				{
					const std::wstring& lang = inf.resources.languages [i];
					WCHAR buf [100] = {0};
					GetLocaleInfoEx (lang.c_str (), 2, buf, 86);
					size_t len = lstrlenW (buf);
					if (len)
					{
						std::vector<WCHAR> lpbuf (format.length () + len + lang.length () + 4, 0);
						swprintf (lpbuf.data (), lpbuf.size (), format.c_str (), buf, lang.c_str ());
						res += lpbuf.data ();
						res += L"\r\n";
					}
				}
			}
			else if (!inf.resources.scales.empty ())
			{
				vec.push_back (GetRCStringSW (IDS_TBODY_PKGRES_SCALE));
				vec.push_back (selfns::GetArrayText (
					inf.resources.scales,
					ARRTS_DIV_RIGHT_SPACE | ARRTS_EMPTY_NO_QUOTE | ARRTS_SINGLE_NO_QUOTE));
			}
			else if (!inf.resources.dxlevels.empty ())
			{
				vec.push_back (GetRCStringSW (IDS_TBODY_PKGRES_DX));
				vec.push_back (selfns::GetArrayText (
					inf.resources.dxlevels,
					ARRTS_DIV_RIGHT_SPACE | ARRTS_EMPTY_NO_QUOTE | ARRTS_SINGLE_NO_QUOTE,
					L',', L'[', L']',
					[] (const DX_FEATURE_LEVEL& dx) -> std::wstring {
					switch (dx)
					{
						case DX_FEATURE_LEVEL_9:  return L"9";
						case DX_FEATURE_LEVEL_10: return L"10";
						case DX_FEATURE_LEVEL_11: return L"11";
						case (DX_FEATURE_LEVEL)4: return L"12";
					}
					return L"Unknown";
				}));
			}

			vec.push_back (std::wnstring::trim (res));
		}
		break;
	}

	return vec.size ();
}
static void EnsureRichEditLoaded ()
{
	static bool loaded = false;
	if (!loaded)
	{
		LoadLibraryW (L"Riched20.dll");
		loaded = true;
	}
}
static std::wstring g_msgText;
static std::wstring g_msgTitle;
class ScopedHICON
{
	HICON hIcon = nullptr;
	public:
	ScopedHICON () = default;
	explicit ScopedHICON (HICON icon): hIcon (icon) {}
	ScopedHICON (const ScopedHICON&) = delete;
	ScopedHICON &operator = (const ScopedHICON&) = delete;
	ScopedHICON (ScopedHICON &&other) noexcept : hIcon (other.hIcon) { other.hIcon = nullptr; }
	ScopedHICON &operator = (ScopedHICON&& other) noexcept
	{
		if (this != &other)
		{
			reset ();
			hIcon = other.hIcon;
			other.hIcon = nullptr;
		}
		return *this;
	}
	~ScopedHICON () { reset (); }
	void reset (HICON newIcon = nullptr)
	{
		if (hIcon)
		{
			DestroyIcon (hIcon);
			hIcon = nullptr;
		}
		hIcon = newIcon;
	}
	HICON get () const { return hIcon; }
	operator HICON() const { return hIcon; }
	bool valid () const { return hIcon != nullptr; }
};
ScopedHICON g_hIcon;
INT_PTR CALLBACK MsgWndProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG: {
			if (g_hIcon.valid ())
			{
				SendMessageW (hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon.get ());
				SendMessageW (hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon.get ());
			}
			if (!g_msgTitle.empty ()) SetWindowTextW (hDlg, g_msgTitle.c_str ());
			HWND hEdit = GetDlgItem (hDlg, IDC_EDITDISPLAY);
			if (hEdit)
			{
				SendMessageW (hEdit, WM_SETTEXT, 0, (LPARAM)g_msgText.c_str ());
				SendMessageW (hEdit, EM_SETSEL, 0, 0);
				SendMessageW (hEdit, EM_SCROLLCARET, 0, 0);
			}
			return TRUE;
		} break;
		case WM_COMMAND: {
			switch (LOWORD (wParam))
			{
				case IDOK: {
					return EndDialog (hDlg, IDCANCEL);
				} break;
				case IDCANCEL: {
					return EndDialog (hDlg, IDCANCEL);
				}
			}
		} break;
	}
	return FALSE;
}
void MessageBoxLongStringW (HWND hDlg, const std::wstring &lpText = std::wstring (L""), const std::wstring &lpTitle = std::wstring (L""))
{
	g_msgTitle = lpTitle;
	g_msgText = lpText;
	DialogBoxW (NULL, MAKEINTRESOURCEW (IDD_DIALOGMSG), hDlg, MsgWndProc);
}
INT_PTR CALLBACK CfmWndProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG: {
			if (g_hIcon.valid ())
			{
				SendMessageW (hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon.get ());
				SendMessageW (hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon.get ());
			}
			if (!g_msgTitle.empty ()) SetWindowTextW (hDlg, g_msgTitle.c_str ());
			HWND hEdit = GetDlgItem (hDlg, IDC_EDITDISPLAY);
			if (hEdit)
			{
				SendMessageW (hEdit, WM_SETTEXT, 0, (LPARAM)g_msgText.c_str ());
				SendMessageW (hEdit, EM_SETSEL, 0, 0);
				SendMessageW (hEdit, EM_SCROLLCARET, 0, 0);
			}
			return TRUE;
		} break;
		case WM_COMMAND: {
			switch (LOWORD (wParam))
			{
				case IDOK: {
					return EndDialog (hDlg, true);
				} break;
				case IDCLOSE:
				case IDCANCEL: {
					return EndDialog (hDlg, false);
				} break;
			}
		} break;
	}
	return FALSE;
}
bool ConfirmBoxLongStringW (HWND hDlg, const std::wstring &lpText = std::wstring (L""), const std::wstring &lpTitle = std::wstring (L""))
{
	g_msgTitle = lpTitle;
	g_msgText = lpText;
	return DialogBoxW (NULL, MAKEINTRESOURCEW (IDD_DIALOGCONFIRM), hDlg, CfmWndProc);
}
std::wstring AppendFilesByDirectory (const std::vector <std::wnstring> &files)
{
	std::wstring result;
	std::map<std::wstring, std::vector<std::wstring>> dirMap;
	for (auto& fullPath : files) {
		wchar_t dir [MAX_PATH] = {0};
		wchar_t file [MAX_PATH] = {0};
		wcsncpy_s (dir, fullPath.c_str (), MAX_PATH);
		LPWSTR pFile = PathFindFileNameW (dir);
		wcsncpy_s (file, pFile, MAX_PATH);
		PathRemoveFileSpecW (dir);
		dirMap [dir].emplace_back (file);
	}
	for (auto& pair : dirMap) {
		result += fmt::format (GetRCStringSW (IDS_FORMAT_INDIR).c_str (), pair.first.c_str ());
		for (auto& filename : pair.second) {
			result += L"        " + filename + L"\r\n";
		}
		result += L"\r\n";
	}
	return result;
};
std::wstring AppendFilesByDirectory (const std::vector <std::wstring> &files)
{
	std::wstring result;
	std::map<std::wstring, std::vector<std::wstring>> dirMap;
	for (auto& fullPath : files) {
		wchar_t dir [MAX_PATH] = {0};
		wchar_t file [MAX_PATH] = {0};
		wcsncpy_s (dir, fullPath.c_str (), MAX_PATH);
		LPWSTR pFile = PathFindFileNameW (dir);
		wcsncpy_s (file, pFile, MAX_PATH);
		PathRemoveFileSpecW (dir);
		dirMap [dir].emplace_back (file);
	}
	for (auto& pair : dirMap) {
		result += fmt::format (GetRCStringSW (IDS_FORMAT_INDIR).c_str (), pair.first.c_str ());
		for (auto& filename : pair.second) {
			result += L"        " + filename + L"\r\n";
		}
		result += L"\r\n";
	}
	return result;
};
BOOL IsCheckboxChecked (HWND hCheckbox)
{
	return (SendMessage (hCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
}
void SetCheckboxState (HWND hCheckbox, BOOL checked)
{
	SendMessage (hCheckbox, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}
std::wstring GetWindowTextW (HWND hWnd)
{
	static HANDLE hHeap = HeapCreate (0, 0, 0);
	if (!hHeap) return L"";
	int len = GetWindowTextLengthW (hWnd);
	if (len <= 0) return L"";
	WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * (len + 1));
	raii heapReleaser ([buf] () {
		if (buf) HeapFree (hHeap, 0, buf);
	});
	if (GetWindowTextW (hWnd, buf, len + 1) == 0) return L"";
	return std::wstring (buf);
}
ITaskbarList3 *g_pTaskbarList = NULL;
void InvokeSetWndEnable (HWND hParent, HWND hWnd, bool bEnable);
std::wstring InvokeGetWndText (HWND hParent, HWND hWnd);
void AppendTextToEdit (HWND hEdit, const std::wstring& text)
{
	// 获取当前文本长度
	LRESULT length = GetWindowTextLengthW (hEdit);
	if (length < 0) length = 0;

	// 获取编辑框的最大文本长度
	// EM_GETLIMITTEXT 返回控件允许的最大字符数
	LRESULT maxLen = SendMessageW (hEdit, EM_GETLIMITTEXT, 0, 0);
	if (maxLen <= 0) maxLen = 65535; // 默认上限（安全兜底）

									 // 如果超过上限，则删除前半部分旧内容
	if (length + (LRESULT)text.size () > maxLen)
	{
		// 删除一半旧文本（可调整策略）
		int removeCount = static_cast<int>(length / 2);
		SendMessageW (hEdit, EM_SETSEL, 0, removeCount);
		SendMessageW (hEdit, EM_REPLACESEL, FALSE, (LPARAM)L"");
		length = GetWindowTextLengthW (hEdit); // 更新长度
	}

	// 追加文本到末尾
	SendMessageW (hEdit, EM_SETSEL, (WPARAM)length, (LPARAM)length);
	SendMessageW (hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str ());
	SendMessageW (hEdit, EM_SCROLLCARET, 0, 0);
}
bool CheckTestPositiveInteger (const std::wstring &t)
{
	std::wstring trimed = std::wnstring::trim (t);
	for (auto &ch : trimed)
	{
		if (ch >= '0' && ch <= '9') continue;
		else return false;
	}
	return true;
}
std::wstring FormatTime (const std::wstring &fmt = L"HH:mm:ss", const SYSTEMTIME &st = GetSystemCurrentTime ())
{
	static HANDLE hHeap = HeapCreate (0, 0, 0); 
	if (!hHeap) return L"";
	size_t size = GetTimeFormatW (LOCALE_USER_DEFAULT, 0, &st, fmt.c_str (), NULL, 0);
	if (size == 0) return L"";
	WCHAR* buf = (WCHAR*)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * (size + 1));
	raii heapReleaser ([buf] () {
		if (buf) HeapFree (hHeap, 0, buf);
	});
	if (GetTimeFormatW (LOCALE_USER_DEFAULT, 0, &st, fmt.c_str (), buf, (int)size) == 0) return L"";
	return std::wstring (buf);
}
std::wstring FormatDate (const std::wstring &fmt = L"yyyy-MM-dd", const SYSTEMTIME &st = GetSystemCurrentTime ())
{
	static HANDLE hHeap = HeapCreate (0, 0, 0);
	if (!hHeap) return L"";
	size_t size = GetDateFormatW (LOCALE_USER_DEFAULT, 0, &st, fmt.c_str (), NULL, 0);
	if (size == 0) return L"";
	WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * (size + 1));
	raii heapReleaser ([buf] () {
		if (buf) HeapFree (hHeap, 0, buf);
	});
	if (GetDateFormatW (LOCALE_USER_DEFAULT, 0, &st, fmt.c_str (), buf, (int)size) == 0) return L"";
	return std::wstring (buf);
}
CriticalSection g_cs;
void RunTask (HWND hDlg)
{
	CreateScopedLock (g_cs);
	HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
	HWND hListButton [] = {
		GetDlgItem (hDlg, IDC_ADDPKG),
		GetDlgItem (hDlg, IDC_REMOVEPKG),
		GetDlgItem (hDlg, IDC_CLEARLIST),
		GetDlgItem (hDlg, IDC_LOADXML)
	};
	HWND hOutInfo [] = {
		GetDlgItem (hDlg, IDC_OUTDIR),
		GetDlgItem (hDlg, IDC_BVER_MAJOR),
		GetDlgItem (hDlg, IDC_BVER_MINOR),
		GetDlgItem (hDlg, IDC_BVER_BUILD),
		GetDlgItem (hDlg, IDC_BVER_REVISION),
		GetDlgItem (hDlg, IDC_NAME_PFN),
		GetDlgItem (hDlg, IDC_NAME_CUSTOM),
		GetDlgItem (hDlg, IDC_CUSTOM_NAME),
		GetDlgItem (hDlg, IDC_SIGNPKG)
	};
	HWND hRunButton = GetDlgItem (hDlg, IDC_RUN);
	HWND hWnd [] = {
		hList,
		hListButton [0],
		hListButton [1],
		hListButton [2],
		hListButton [3],
		hOutInfo [0],
		hOutInfo [1],
		hOutInfo [2],
		hOutInfo [3],
		hOutInfo [4],
		hOutInfo [5],
		hOutInfo [6],
		hOutInfo [7],
		hOutInfo [8],
		hRunButton
	};
	HWND hConsole = GetDlgItem (hDlg, IDC_OUTPUT);
	HWND hProgress = GetDlgItem (hDlg, IDC_PROGRESSBAR);
	HWND hPercent = GetDlgItem (hDlg, IDC_PROGRESSDISPLAY);
	HWND hStatus = GetDlgItem (hDlg, IDC_STATUSDISPLAY);
	IODirection io (
		nullptr,
		[&hConsole] (LPCWSTR c) -> int {
		AppendTextToEdit (hConsole, c);
		return 0;
	}, [&hConsole, &hStatus] (LPCWSTR c) -> int {
		AppendTextToEdit (hConsole, std::wstring (c) + L"\r\n");
		{
			std::wstring format = GetRCStringSW (IDS_STATUS);
			static HANDLE hHeap = HeapCreate (0, 0, 0);
			if (!hHeap) return -1;
			size_t len = format.length () + lstrlenW (c) + 4;
			WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * len);
			raii freeBuf ([buf] () {
				if (buf) HeapFree (hHeap, 0, buf);
			});
			if (buf)
			{
				swprintf (buf, len, format.c_str (), std::wnstring (c).trim ().c_str ());
				SetWindowTextW (hStatus, buf);
			}
		}
		return 0;
	});
	auto progresscb = [&hDlg, &hProgress, &hPercent] (int curr, int total, int progress) {
		g_pTaskbarList->SetProgressState (hDlg, TBPF_NORMAL);
		g_pTaskbarList->SetProgressValue (hDlg, curr, total);
		SendMessageW (hProgress, PBM_SETRANGE, 0, MAKELPARAM (0, total));
		SendMessageW (hProgress, PBM_SETSTATE, PBST_NORMAL, 0);
		SendMessageW (hProgress, PBM_SETPOS, curr, 0);
		WCHAR buf [256] = {0};
		swprintf (buf, GetRCStringSW (IDS_PROGRESS).c_str (), progress, curr, total);
		SetWindowTextW (hPercent, buf);
	};
	raii endtask ([&hDlg, &hWnd, &hOutInfo, &hConsole, &io] () {
		for (auto &it : hWnd) InvokeSetWndEnable (hDlg, it, true);
		InvokeSetWndEnable (hDlg, hOutInfo [7], IsCheckboxChecked (hOutInfo [6]));
		io.safeOutputLine (fmt::format (
			GetRCStringSW (IDS_TASK_FORMAT).c_str (),
			FormatDate (GetRCStringSW (IDS_DATE_FORMAT)).c_str (),
			FormatTime (GetRCStringSW (IDS_TIME_FORMAT)).c_str (),
			GetRCStringSW (IDS_TASK_END).c_str ()
		));
		g_pTaskbarList->SetProgressState (hDlg, TBPF_NOPROGRESS);
	});
	SetWindowLongPtr (hProgress, GWL_STYLE, (GetWindowLongPtr (hProgress, GWL_STYLE) & ~PBS_MARQUEE));
	g_pTaskbarList->SetProgressState (hDlg, TBPF_NORMAL);
	SendMessageW (hProgress, PBM_SETMARQUEE, FALSE, 0);
	SendMessageW (hProgress, PBM_SETSTATE, PBST_NORMAL, 0);
	SendMessageW (hProgress, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
	SendMessageW (hProgress, PBM_SETPOS, 0, 0);
	{
		WCHAR buf [256] = {0};
		swprintf (buf, GetRCStringSW (IDS_PROGRESS).c_str (), 0, 0, 0);
		SetWindowTextW (hPercent, buf);
	}
	for (auto &it : hWnd) InvokeSetWndEnable (hDlg, it, false);
	io.safeOutputLine (fmt::format (
		GetRCStringSW (IDS_TASK_FORMAT).c_str (), 
		FormatDate (GetRCStringSW (IDS_DATE_FORMAT)).c_str (),
		FormatTime (GetRCStringSW (IDS_TIME_FORMAT)).c_str (),
		GetRCStringSW (IDS_TASK_START).c_str ()
	));
	std::wstring outdir = ProcessEnvVars (InvokeGetWndText (hDlg, hOutInfo [0]));
	if (std::wnstring::empty (outdir) || !IsDirectoryExists (outdir) && !CreateDirectoryW (outdir.c_str (), 0))
	{
		std::wstring errort = GetRCStringSW (IDS_ERROR_DIRINVALID);
		io.safeOutputLine (errort);
		MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
		return;
	}
	enum class NameType { pfn, custom } nametype;
	std::wnstring customname = L"";
	{
		bool namepfn = IsCheckboxChecked (hOutInfo [5]),
			namecust = IsCheckboxChecked (hOutInfo [6]);
		if (!(namepfn ^ namecust))
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_NAMENAMED);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			return;
		}
		else if (namepfn) nametype = NameType::pfn;
		else if (namecust)
		{
			nametype = NameType::custom;
			customname = InvokeGetWndText (hDlg, hOutInfo [7]);
			if (!IsValidWindowsName (customname) || customname.empty ())
			{
				std::wstring errort = GetRCStringSW (IDS_ERROR_NAMEINVALID);
				io.safeOutputLine (errort);
				MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
				return;
			}
		}
	}
	version bundlever;
	{
		std::wnstring verparts [4] = {};
		for (size_t i = 0; i < 4; i ++)
		{
			verparts [i] = InvokeGetWndText (hDlg, hOutInfo [1 + i]);
			int vernum = _wtoi (verparts [i].c_str ());
			bool iserror = false;
			std::wstring errormsg = L"";
			if (verparts [i].empty ())
			{
				iserror = true;
				errormsg = GetRCStringSW (IDS_ERROR_VEREMPTY);
			}
			else if (!CheckTestPositiveInteger (verparts [i]))
			{
				iserror = true;
				errormsg = GetRCStringSW (IDS_ERROR_VERCHARS);
			}
			else if (vernum < 0 || vernum > 65535)
			{
				iserror = true;
				errormsg = GetRCStringSW (IDS_ERROR_VERRANGE);
			}
			if (iserror)
			{
				io.safeOutputLine (errormsg);
				MessageBoxW (hDlg, errormsg.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
				return;
			}
			else
			{
				switch (i)
				{
					case 0: bundlever.major = (UINT16)vernum; break;
					case 1: bundlever.minor = (UINT16)vernum; break;
					case 2: bundlever.build = (UINT16)vernum; break;
					case 3: bundlever.revision = (UINT16)vernum; break;
				}
			}
		}
		if (bundlever.empty ())
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_VERINVAILD);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			return;
		}
	}
	bool autosigned = IsCheckboxChecked (hOutInfo [8]);
	std::vector <std::wstring> files;
	{
		int64_t tlen = (int64_t)ListView_GetItemCount (hList);
		if (tlen < 0)
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_NOFILES);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			return;
		}
		std::vector <std::vector <std::wstring>> allitems;
		[] (HWND hList, std::vector <std::vector <std::wstring>> &out) {
			int64_t tlen = (int64_t)ListView_GetItemCount (hList);
			for (size_t cnt = 0; cnt < tlen; cnt ++)
			{
				std::vector <std::wstring> row;
				GetListViewItemRow (hList, cnt, row);
				out.push_back (row);
			}
		} (hList, allitems);
		for (auto &it : allitems)
		{
			static HANDLE hHeap = HeapCreate (0, 0, 0);
			if (!hHeap) break;
			size_t len = it [0].length () + it [1].length () + 4;
			WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * len);
			raii freeBuf ([buf] () {
				if (buf) HeapFree (hHeap, 0, buf);
			});
			if (buf)
			{
				PathCombineW (buf, it [1].c_str (), it [0].c_str ());
				files.push_back (buf);
			}
		}
	}
	std::wstring outfile = L"";
	io.safeOutputLine (GetRCStringSW (IDS_PROGRESS_MAKEPKG));
	HRESULT hr = CreateBundlePackageFile (
		files,
		outdir,
		outfile,
		bundlever.data (),
		customname,
		nametype == NameType::pfn,
		progresscb,
		io
	);
	if (FAILED (hr) || !IsFileExists (outfile))
	{
		g_pTaskbarList->SetProgressState (hDlg, TBPF_ERROR);
		SendMessageW (hProgress, PBM_SETSTATE, PBST_ERROR, 0);
		std::wstring format = GetRCStringSW (IDS_ERROR_PACKAGE);
		static HANDLE hHeap = HeapCreate (0, 0, 0);
		if (!hHeap)
		{
			io.outputLine (L"Heap creation failed");
			return;
		}
		size_t len = format.length () + std::max (std::to_wstring (hr).length (), (size_t)16) + 4;
		WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * len);
		raii freeBuf ([buf] () {
			if (buf) HeapFree (hHeap, 0, buf);
		});
		if (buf)
		{
			swprintf (buf, len, format.c_str (), hr);
			io.outputLine (buf);
		}
		return;
	}
	if (autosigned)
	{
		std::wstring finaloutdir = GetFileDirectoryW (outfile);
		static HANDLE hHeap = HeapCreate (0, 0, 0);
		if (!hHeap)
		{
			io.safeOutputLine (L"HeapCreate failed");
			return;
		}
		size_t fnameLen = outfile.length () + 1;
		WCHAR* finaloutfname = (WCHAR*)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * fnameLen);
		raii freeFinaloutfname ([finaloutfname] () {
			if (finaloutfname) HeapFree (hHeap, 0, finaloutfname);
		});
		StrCpyW (finaloutfname, PathFindFileNameW (outfile.c_str ()));
		PathRemoveExtensionW (finaloutfname);
		std::wstring publisher = GetBundlePackagePublisher (outfile);
		std::wstring outcert, outpvk, outpfx;
		io.safeOutputLine (GetRCStringSW (IDS_PROGRESS_MAKECERT));
		bool res = MakeCert (publisher, finaloutdir, finaloutfname, outcert, outpvk, io, hDlg);
		if (!res)
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_CERT);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			g_pTaskbarList->SetProgressState (hDlg, TBPF_ERROR);
			SendMessageW (hProgress, PBM_SETSTATE, PBST_ERROR, 0);
			return;
		}
		io.safeOutputLine (GetRCStringSW (IDS_PROGRESS_PVK2PFX));
		res = Pvk2Pfx (outcert, outpvk, finaloutdir, finaloutfname, outpfx, io, hDlg);
		if (!res)
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_PFX);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			g_pTaskbarList->SetProgressState (hDlg, TBPF_ERROR);
			SendMessageW (hProgress, PBM_SETSTATE, PBST_ERROR, 0);
			return;
		}
		io.safeOutputLine (GetRCStringSW (IDS_PROGRESS_SIGN));
		res = SignTool (outfile, outpfx, io);
		if (!res)
		{
			std::wstring errort = GetRCStringSW (IDS_ERROR_SIGN);
			io.safeOutputLine (errort);
			MessageBoxW (hDlg, errort.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
			g_pTaskbarList->SetProgressState (hDlg, TBPF_ERROR);
			SendMessageW (hProgress, PBM_SETSTATE, PBST_ERROR, 0);
			return;
		}
	}
	{
		std::wstring format = GetRCStringSW (IDS_SUCCESS);
		static HANDLE hHeap = HeapCreate (0, 0, 0);
		if (!hHeap)
		{
			io.outputLine (L"HeapCreate failed");
			return;
		}
		size_t bufLen = format.length () + outfile.length () + bundlever.stringifyw ().length () + 4;
		WCHAR *buf = (WCHAR *)HeapAlloc (hHeap, HEAP_ZERO_MEMORY, sizeof (WCHAR) * bufLen);
		raii freeBuf ([buf] () {
			if (buf) HeapFree (hHeap, 0, buf);
		});
		swprintf (buf, bufLen, format.c_str (), outfile.c_str (), bundlever.stringifyw ().c_str ());
		io.outputLine (buf);
	}
}
#define WM_ENABLE_CONTROL (WM_APP + 1)
#define WM_GET_TEXT (WM_APP + 2)
#define WM_SET_TEXT (WM_APP + 3)
#define TIMER_DEBOUNCE_LIST 1001
void InvokeSetWndEnable (HWND hParent, HWND hWnd, bool bEnable)
{
	PostMessageW (hParent, WM_ENABLE_CONTROL, (WPARAM)hWnd, (LPARAM)bEnable);
}
std::wstring InvokeGetWndText (HWND hParent, HWND hWnd)
{
	LPWSTR wsptr = nullptr;
	raii endt ([&wsptr] () {
		if (wsptr) free (wsptr);
		wsptr = nullptr;
	});
	SendMessageW (hParent, WM_GET_TEXT, (WPARAM)hWnd, (LPARAM)&wsptr);
	return std::wstring (wsptr);
}
std::wstring Base64Encode (const std::wstring &input)  
{
	DWORD outputLength = 0;
	if (!CryptBinaryToStringW (
		(const BYTE*)input.data (),
		input.size (),
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
		NULL,
		&outputLength)) {
		return L"";
	}
	std::wstring output (outputLength, '\0');
	if (!CryptBinaryToStringW (
		(const BYTE*)input.data (),
		input.size (),
		CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
		&output [0],
		&outputLength)) {
		return L"";
	}
	if (!output.empty () && output.back () == '\0') output.pop_back ();
	return output;
}
std::wstring GetCurrentUserRemark ()
{
	std::wstring text = ProcessEnvVars (L"%UserProfile%");
	return Base64Encode (text);
}
static WInitFile g_inputsave (EnsureTrailingSlash (GetProgramRootDirectoryW ()) + L"inputsave.ini");
BOOL SetWindowTextW (HWND hWnd, const std::wstring &wstr)
{
	return SetWindowTextW (hWnd, wstr.c_str ());
}
std::wstring GetWindowTextWString (HWND hWnd)
{
	int len = GetWindowTextLengthW (hWnd);
	std::wstring text (len, L'\0');
	GetWindowTextW (hWnd, &text [0], len + 1);
	return text;
}
INT_PTR CALLBACK WndProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_ENABLE_CONTROL: {
			HWND hCtrl = (HWND)wParam;
			BOOL enable = (BOOL)lParam;
			EnableWindow (hCtrl, enable);
			return 0;
		} break;
		case WM_GET_TEXT: {
			*(LPWSTR *)lParam = _wcsdup (GetWindowTextW ((HWND)wParam).c_str ());
			return 0;
		} break;
		case WM_INITDIALOG: {
			if (g_hIcon.valid ())
			{
				SendMessageW (hDlg, WM_SETICON, ICON_BIG, (LPARAM)g_hIcon.get ());
				SendMessageW (hDlg, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon.get ());
			}
			HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
			InitListControl (hList, thead);
			DragAcceptFiles (hDlg, TRUE);
			HWND hEnablePFN = GetDlgItem (hDlg, IDC_NAME_PFN);
			SetCheckboxState (hEnablePFN, TRUE);
			HWND hCustomNameInput = GetDlgItem (hDlg, IDC_CUSTOM_NAME);
			EnableWindow (hCustomNameInput, FALSE);
			HWND hProgress = GetDlgItem (hDlg, IDC_PROGRESSBAR);
			LONG_PTR style = GetWindowLongPtr (hProgress, GWL_STYLE);
			HWND hSignPkg = GetDlgItem (hDlg, IDC_SIGNPKG);
			SetCheckboxState (hSignPkg, TRUE);
			SetWindowLongPtr (hProgress, GWL_STYLE, style | PBS_MARQUEE);
			SendMessageW (hProgress, PBM_SETMARQUEE, TRUE, 50);
			HWND hEdit = GetDlgItem (hDlg, IDC_OUTPUT);
			SendMessageW (hEdit, EM_LIMITTEXT, (WPARAM)-1, 0);
			{
				std::wstring user = GetCurrentUserRemark ();
				HWND hOutInfo [] = {
					GetDlgItem (hDlg, IDC_OUTDIR),
					GetDlgItem (hDlg, IDC_BVER_MAJOR),
					GetDlgItem (hDlg, IDC_BVER_MINOR),
					GetDlgItem (hDlg, IDC_BVER_BUILD),
					GetDlgItem (hDlg, IDC_BVER_REVISION),
					GetDlgItem (hDlg, IDC_NAME_PFN),
					GetDlgItem (hDlg, IDC_NAME_CUSTOM),
					GetDlgItem (hDlg, IDC_CUSTOM_NAME),
					GetDlgItem (hDlg, IDC_SIGNPKG)
				};
				SetWindowTextW (hOutInfo [0], g_inputsave.readStringValue (user, L"OutputDirectory"));
				SetWindowTextW (hOutInfo [1], std::to_wstring (g_inputsave.readUIntValue (user, L"BundleVersionMajor", GetSystemCurrentTime ().wYear)));
				SetWindowTextW (hOutInfo [2], std::to_wstring (g_inputsave.readUIntValue (user, L"BundleVersionMinor")));
				SetWindowTextW (hOutInfo [3], std::to_wstring (g_inputsave.readUIntValue (user, L"BundleVersionBuild")));
				SetWindowTextW (hOutInfo [4], std::to_wstring (g_inputsave.readUIntValue (user, L"BundleVersionRevision")));
				SetCheckboxState (hOutInfo [5], !g_inputsave.readIntValue (user, L"OutputFileNameMethod"));
				SetCheckboxState (hOutInfo [6], g_inputsave.readIntValue (user, L"OutputFileNameMethod"));
				SetWindowTextW (hOutInfo [7], g_inputsave.readStringValue (user, L"CustomOutputFileName"));
				SetCheckboxState (hOutInfo [8], g_inputsave.readBoolValue (user, L"SignPackageAfterPackagedSuccessfully"));
				HWND hEnableCustom = GetDlgItem (hDlg, IDC_NAME_CUSTOM);
				EnableWindow (hCustomNameInput, IsCheckboxChecked (hEnableCustom));
			}
			return TRUE;
		} break;
		case WM_COMMAND: {
			static heapmgr hheap;
			switch (LOWORD (wParam))
			{
				case IDCANCEL: {
					return EndDialog (hDlg, IDCANCEL);
				} break;
				case IDC_ADDPKG: {
					HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
					std::wstring filterdisplay = GetRCStringSW (IDS_DIALOG_APPXDESP);
					std::wstring filtertypes = L"*.appx;*.msix";
					size_t len = filterdisplay.capacity () + filtertypes.capacity () + 4;
					auto filter = hheap.alloc <WCHAR> (len);
					raii endt ([&filter] () {
						if (filter) hheap.free (filter);
						filter = nullptr;
					});
					strcpynull (filter, filterdisplay.c_str (), len);
					strcpynull (filter, filtertypes.c_str (), len);
					std::vector <std::wstring> files;
					ExploreFile (hDlg, files, filter, OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST, GetRCStringSW (IDS_DIALOG_ADDAPPX));
					switch (files.size ())
					{
						case 0: return FALSE;
						case 1: {
							std::vector <std::wstring> row;
							GetTableRowFromPackage (files [0], row);
							if (row.empty ())
							{
								std::wstring failedstr = GetRCStringSW (IDS_FILE_SINGLE_FAILED);
								std::vector<WCHAR> lpbuf (failedstr.length () + files [0].length () + 16, 0);
								swprintf (lpbuf.data (), lpbuf.size (), failedstr.c_str (), files [0].c_str ());
								MessageBoxW (hDlg, lpbuf.data (), GetRCStringSW (IDS_TITLE_ADDED_ERROR).c_str (), MB_ICONERROR);
								return FALSE;
							}
							else
							{
								using tablerow = std::vector<std::wstring>;
								std::vector<std::vector<std::wstring>> vec;
								vec.emplace_back (row);
								AddListContent (hList, thead, vec, true,
									[] (const tablerow& r1, const tablerow& r2) -> bool {
									if (r1.size () > 2 && r2.size () > 2)
									{
										std::vector<WCHAR> path1 (r1 [0].length () + r1 [1].length () + 8, 0);
										std::vector<WCHAR> path2 (r2 [0].length () + r2 [1].length () + 8, 0);
										PathCombineW (path1.data (), r1 [1].c_str (), r1 [0].c_str ());
										PathCombineW (path2.data (), r2 [1].c_str (), r2 [0].c_str ());
										return std::wnstring::equals (std::wnstring (path1.data ()), std::wnstring (path2.data ()));
									}
									return false;
								},
									[hDlg] (const tablerow& tr) {
									std::vector<WCHAR> path (tr [0].length () + tr [1].length () + 8, 0);
									PathCombineW (path.data (), tr [1].c_str (), tr [0].c_str ());

									std::wstring failedstr = GetRCStringSW (IDS_FILE_SINGLE_FAILED);
									std::vector<WCHAR> lpbuf (failedstr.length () + lstrlenW (path.data ()) + 16, 0);
									swprintf (lpbuf.data (), lpbuf.size (), failedstr.c_str (), path.data ());
									MessageBoxW (hDlg, lpbuf.data (), GetRCStringSW (IDS_TITLE_ADDED_ERROR).c_str (), MB_ICONERROR);
								}, 250);
							}
						} break;
						default: {
							std::vector<std::vector<std::wstring>> rows;
							std::vector<std::wnstring> failed;
							std::vector<std::wnstring> success;

							for (auto& it : files)
							{
								std::vector <std::wstring> r;
								GetTableRowFromPackage (it, r);
								if (!r.empty ()) { rows.push_back (r); success.emplace_back (it); }
								else failed.emplace_back (it);
							}

							using tablerow = std::vector<std::wstring>;
							AddListContent (hList, thead, rows, true,
								[] (const tablerow& r1, const tablerow& r2) -> bool {
								if (r1.size () > 2 && r2.size () > 2)
								{
									std::vector<WCHAR> path1 (r1 [0].length () + r1 [1].length () + 8, 0);
									std::vector<WCHAR> path2 (r2 [0].length () + r2 [1].length () + 8, 0);
									PathCombineW (path1.data (), r1 [1].c_str (), r1 [0].c_str ());
									PathCombineW (path2.data (), r2 [1].c_str (), r2 [0].c_str ());
									return std::wnstring::equals (std::wnstring (path1.data ()), std::wnstring (path2.data ()));
								}
								return false;
							},
								[&success, &failed] (const tablerow& tr) {
								std::vector<WCHAR> path (tr [0].length () + tr [1].length () + 8, 0);
								PathCombineW (path.data (), tr [1].c_str (), tr [0].c_str ());
								success.erase (std::remove (success.begin (), success.end (), std::wnstring (path.data ())), success.end ());
								push_unique (failed, std::wnstring (path.data ()));
							}, 250);

							std::wstring result;

							auto appendFilesByDirectory = [&result] (const std::vector<std::wnstring>& files) {
								std::map<std::wstring, std::vector<std::wstring>> dirMap;
								for (auto& fullPath : files) {
									wchar_t dir [MAX_PATH] = {0};
									wchar_t file [MAX_PATH] = {0};
									wcsncpy_s (dir, fullPath.c_str (), MAX_PATH);
									LPWSTR pFile = PathFindFileNameW (dir);
									wcsncpy_s (file, pFile, MAX_PATH);
									PathRemoveFileSpecW (dir);
									dirMap [dir].emplace_back (file);
								}
								for (auto& pair : dirMap) {
									result += fmt::format (GetRCStringSW (IDS_FORMAT_INDIR).c_str (), pair.first.c_str ());
									for (auto& filename : pair.second) {
										result += L"        " + filename + L"\r\n";
									}
									result += L"\r\n";
								}
							};

							if (!success.empty ())
							{
								std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_SUCCESS);
								std::vector<WCHAR> lpbuf (title.length () + 64, 0);
								swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), success.size (), files.size ());
								result += lpbuf.data ();
								result += L"\r\n";
								appendFilesByDirectory (success);
								result += L"\r\n";
							}

							if (!failed.empty ())
							{
								std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_FAILED);
								std::vector<WCHAR> lpbuf (title.length () + 64, 0);
								swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), failed.size (), files.size ());
								result += lpbuf.data ();
								result += L"\r\n";
								appendFilesByDirectory (failed);
							}

							MessageBoxLongStringW (hDlg, result.c_str (), GetRCStringSW (IDS_TITLE_RESULT).c_str ());
						}
					}
					SetProcessWorkingSetSize (GetCurrentProcess (), -1, -1);
					EmptyWorkingSet (GetCurrentProcess ());
					return TRUE;
				} break;
				case IDC_REMOVEPKG: {
					HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
					std::vector <int64_t> serials;
					GetListSelectedCount (hList, serials);
					std::vector <std::wstring> filepaths;

					for (auto &uit : serials)
					{
						std::vector <std::wstring> row;
						GetListViewItemRow (hList, uit, row);
						auto buf = hheap.alloc <WCHAR> (row [0].length () + row [1].length () + 4);
						raii entd ([&buf] () {
							if (buf) hheap.free (buf);
							buf = nullptr;
						});
						PathCombineW (buf, row [1].c_str (), row [0].c_str ());
						filepaths.push_back (buf);
					}
					std::wstring msg = L"";
					msg += GetRCStringSW (IDS_ASK_REMOVE) + L"\r\n";
					msg += AppendFilesByDirectory (filepaths);
					EmptyWorkingSet (GetCurrentProcess ());
					if (ConfirmBoxLongStringW (hDlg, msg, GetRCStringSW (IDS_TITLE_CONFIRM).c_str ()))
					{
						RemoveListViewByCount (hList, serials);
						return TRUE;
					}
					return FALSE;
				} break;
				case IDC_CLEARLIST: {
					if (MessageBoxW (hDlg, GetRCStringSW (IDS_AKS_CLEAR).c_str (), GetRCStringSW (IDS_TITLE_CONFIRM).c_str (), MB_OKCANCEL) == IDOK)
					{
						HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
						return ListView_DeleteAllItems (hList);
					}
					return FALSE;
				} break;
				case IDC_LOADXML: {
					std::wstring filterdisplay = GetRCStringSW (IDS_DIALOG_BUNDLEXML);
					std::wstring filtertypes = L"AppxBundleManifest.xml";
					size_t len = filterdisplay.capacity () + filtertypes.capacity () + 4;
					auto filter = hheap.alloc <WCHAR> (len);
					raii endt ([&filter] () {
						if (filter) hheap.free (filter);
						filter = nullptr;
					});
					strcpynull (filter, filterdisplay.c_str (), len);
					strcpynull (filter, filtertypes.c_str (), len);
					std::vector <std::wstring> xmlpaths;
					ExploreFile (
						hDlg,
						xmlpaths,
						filter,
						OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST,
						GetRCStringSW (IDS_DIALOG_LOADXML)
					);
					if (xmlpaths.size () != 1) return FALSE;
					auto &xmlpath = xmlpaths [0];
					if (!IsFileExists (xmlpath)) return FALSE;
					auto bundledir = hheap.alloc <WCHAR> (xmlpath.length () + 1);
					raii endt1 ([&bundledir] () {
						if (bundledir) hheap.free (bundledir);
						bundledir = nullptr;
					});
					lstrcpyW (bundledir, xmlpath.c_str ());
					PathRemoveFileSpecW (bundledir);
					PathRemoveFileSpecW (bundledir);
					pugi::xml_document doc;
					pugi::xml_parse_result result = doc.load_file (xmlpath.c_str ());
					if (!result)
					{
						std::wstring errmsg = GetRCStringSW (IDS_ERROR_XMLPARSE) + pugi::as_wide (result.description ());
						MessageBoxW (hDlg, errmsg.c_str (), GetRCStringSW (IDS_TITLE_ERROR).c_str (), MB_ICONERROR);
						return FALSE;
					}
					pugi::xml_node bundle = doc.child ("Bundle");
					if (!bundle)
					{
						pugi::xml_node root = doc.document_element ();
						bundle = root;
					}
					pugi::xml_node packages = bundle.child ("Packages");
					std::vector <std::wstring> files;
					for (pugi::xml_node package : packages.children ("Package"))
					{
						pugi::xml_attribute attr = package.attribute ("FileName");
						if (attr)
						{
							WCHAR buf [32767] = {0};
							PathCombineW (buf, bundledir, pugi::as_wide (attr.as_string ()).c_str ());
							files.push_back (buf);
						}
					}
					{
						HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
						std::vector<std::vector<std::wstring>> rows;
						std::vector<std::wnstring> failed;
						std::vector<std::wnstring> success;

						for (auto& it : files)
						{
							std::vector <std::wstring> r;
							GetTableRowFromPackage (it, r);
							if (!r.empty ()) { rows.push_back (r); success.emplace_back (it); }
							else failed.emplace_back (it);
						}

						using tablerow = std::vector<std::wstring>;
						AddListContent (hList, thead, rows, true,
							[] (const tablerow& r1, const tablerow& r2) -> bool {
							if (r1.size () > 2 && r2.size () > 2)
							{
								std::vector<WCHAR> path1 (r1 [0].length () + r1 [1].length () + 8, 0);
								std::vector<WCHAR> path2 (r2 [0].length () + r2 [1].length () + 8, 0);
								PathCombineW (path1.data (), r1 [1].c_str (), r1 [0].c_str ());
								PathCombineW (path2.data (), r2 [1].c_str (), r2 [0].c_str ());
								return std::wnstring::equals (std::wnstring (path1.data ()), std::wnstring (path2.data ()));
							}
							return false;
						},
							[&success, &failed] (const tablerow& tr) {
							std::vector<WCHAR> path (tr [0].length () + tr [1].length () + 8, 0);
							PathCombineW (path.data (), tr [1].c_str (), tr [0].c_str ());
							success.erase (std::remove (success.begin (), success.end (), std::wnstring (path.data ())), success.end ());
							push_unique (failed, std::wnstring (path.data ()));
						}, 250);

						std::wstring result;

						auto appendFilesByDirectory = [&result] (const std::vector<std::wnstring>& files) {
							std::map<std::wstring, std::vector<std::wstring>> dirMap;
							for (auto& fullPath : files) {
								wchar_t dir [MAX_PATH] = {0};
								wchar_t file [MAX_PATH] = {0};
								wcsncpy_s (dir, fullPath.c_str (), MAX_PATH);
								LPWSTR pFile = PathFindFileNameW (dir);
								wcsncpy_s (file, pFile, MAX_PATH);
								PathRemoveFileSpecW (dir);
								dirMap [dir].emplace_back (file);
							}
							for (auto& pair : dirMap) {
								result += fmt::format (GetRCStringSW (IDS_FORMAT_INDIR).c_str (), pair.first.c_str ());
								for (auto& filename : pair.second) {
									result += L"        " + filename + L"\r\n";
								}
								result += L"\r\n";
							}
						};

						if (!success.empty ())
						{
							std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_SUCCESS);
							std::vector<WCHAR> lpbuf (title.length () + 64, 0);
							swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), success.size (), files.size ());
							result += lpbuf.data ();
							result += L"\r\n";
							appendFilesByDirectory (success);
							result += L"\r\n";
						}
						EmptyWorkingSet (GetCurrentProcess ());
						if (!failed.empty ())
						{
							std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_FAILED);
							std::vector<WCHAR> lpbuf (title.length () + 64, 0);
							swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), failed.size (), files.size ());
							result += lpbuf.data ();
							result += L"\r\n";
							appendFilesByDirectory (failed);
						}

						MessageBoxLongStringW (hDlg, result.c_str (), GetRCStringSW (IDS_TITLE_RESULT).c_str ());
						return TRUE;
					}
					return FALSE;
				} break;
				case IDC_NAME_PFN:
				case IDC_NAME_CUSTOM: {
					HWND hEnableCustomName = GetDlgItem (hDlg, IDC_NAME_CUSTOM);
					HWND hCustomNameInput = GetDlgItem (hDlg, IDC_CUSTOM_NAME);
					EnableWindow (hCustomNameInput, IsCheckboxChecked (hEnableCustomName));
				} break;
				case IDC_EXPDIR: {
					HWND hDir = GetDlgItem (hDlg, IDC_OUTDIR);
					std::wstring getdir = ExploreDirectory (hDlg);
					if (!std::wnstring::empty (getdir))
					{
						SetWindowTextW (hDir, getdir.c_str ());
					}
					return true;
				} break;
				case IDC_RUN: {
					{
						HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
						HWND hListButton [] = {
							GetDlgItem (hDlg, IDC_ADDPKG),
							GetDlgItem (hDlg, IDC_REMOVEPKG),
							GetDlgItem (hDlg, IDC_CLEARLIST),
							GetDlgItem (hDlg, IDC_LOADXML)
						};
						HWND hOutInfo [] = {
							GetDlgItem (hDlg, IDC_OUTDIR),
							GetDlgItem (hDlg, IDC_BVER_MAJOR),
							GetDlgItem (hDlg, IDC_BVER_MINOR),
							GetDlgItem (hDlg, IDC_BVER_BUILD),
							GetDlgItem (hDlg, IDC_BVER_REVISION),
							GetDlgItem (hDlg, IDC_NAME_PFN),
							GetDlgItem (hDlg, IDC_NAME_CUSTOM),
							GetDlgItem (hDlg, IDC_CUSTOM_NAME),
							GetDlgItem (hDlg, IDC_SIGNPKG)
						};
						HWND hRunButton = GetDlgItem (hDlg, IDC_RUN);
						HWND hWnd [] = {
							hList,
							hListButton [0],
							hListButton [1],
							hListButton [2],
							hListButton [3],
							hOutInfo [0],
							hOutInfo [1],
							hOutInfo [2],
							hOutInfo [3],
							hOutInfo [4],
							hOutInfo [5],
							hOutInfo [6],
							hOutInfo [7],
							hOutInfo [8],
							hRunButton
						};
						for (auto &it : hWnd) EnableWindow (it, false);
					}
					auto thread = std::thread (RunTask, hDlg);
					thread.detach ();
					return TRUE;
				} break;
			}
		} break;
		case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			switch (pnmh->idFrom)
			{
				case IDC_APPXLIST:
				{
					HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
					switch (pnmh->code)
					{
						case LVN_ITEMCHANGED:
						{
							LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
							if ((pnmv->uChanged & LVIF_STATE) && (pnmv->uNewState & LVIS_SELECTED) || !(pnmv->uNewState & LVIS_SELECTED) && (pnmv->uOldState & LVIS_SELECTED))
							{
								KillTimer (hDlg, TIMER_DEBOUNCE_LIST);
								SetTimer (hDlg, TIMER_DEBOUNCE_LIST, 200, NULL);
							}
						}
						break;
					}
				}
				break;
			}
			return TRUE;
		}
		break;
		case WM_TIMER:
		{
			if (wParam == TIMER_DEBOUNCE_LIST)
			{
				KillTimer (hDlg, TIMER_DEBOUNCE_LIST);
				HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
				std::vector <int64_t> list;
				GetListSelectedCount (hList, list);
				for (auto &it : list) it++;
				{
					int64_t tlen = (int64_t)ListView_GetItemCount (hList);
					std::wstring selected = selfns::GetArrayText (list);
					std::wstring str = GetRCStringSW (IDS_TABLE_CURRENTSELECT);
					WCHAR lpstr [512];
					swprintf (lpstr, str.c_str (), selected.c_str (), tlen);
					HWND hStatus = GetDlgItem (hDlg, IDC_CURRENTSELECT);
					SetWindowTextW (hStatus, lpstr);
				}
				for (auto &it : list) it--;
				{
					std::vector <std::wstring> files;
					std::vector <std::vector <std::wnstring>> strings;
					for (auto &it : list)
					{
						std::vector <std::wstring> row;
						GetListViewItemRow (hList, it, row);
						WCHAR path [MAX_PATH];
						PathCombineW (path, row [1].c_str (), row [0].c_str ());
						files.push_back (path);
						std::vector <std::wnstring> inf;
						GetPackageInfoForDisplay (path, inf);
						strings.push_back (inf);
					}
					std::vector <std::wnstring> same;
					MergeVectorsKeepSame (strings, same);
					same.resize (8);
					HWND edits [] = {
						GetDlgItem (hDlg, IDC_APPXFILEPATH),
						GetDlgItem (hDlg, IDC_PKGIDNAME),
						GetDlgItem (hDlg, IDC_PKGIDPUBLISHER),
						GetDlgItem (hDlg, IDC_PKGIDVERSION),
						GetDlgItem (hDlg, IDC_PKGTYPE),
						GetDlgItem (hDlg, IDC_PKGARCH),
						GetDlgItem (hDlg, IDC_PKGRESTYPE),
						GetDlgItem (hDlg, IDC_PKGRESSUPP)
					};
					for (size_t i = 0; i < 8; i++)
					{
						std::wnstring &text = same [i];
						HWND &hEdit = edits [i];
						SetWindowTextW (hEdit, text.c_str ());
						if (i == 0)
						{
							int length = GetWindowTextLengthW (hEdit);
							SendMessageW (hEdit, EM_SETSEL, length, length);
							SendMessageW (hEdit, EM_SCROLLCARET, 0, 0);
						}
					}
				}
				SetProcessWorkingSetSize (GetCurrentProcess (), -1, -1);
				EmptyWorkingSet (GetCurrentProcess ());
			}
			return TRUE;
		}
		case WM_DESTROY: {
			{
				std::wstring user = GetCurrentUserRemark ();
				HWND hOutInfo [] = {
					GetDlgItem (hDlg, IDC_OUTDIR),
					GetDlgItem (hDlg, IDC_BVER_MAJOR),
					GetDlgItem (hDlg, IDC_BVER_MINOR),
					GetDlgItem (hDlg, IDC_BVER_BUILD),
					GetDlgItem (hDlg, IDC_BVER_REVISION),
					GetDlgItem (hDlg, IDC_NAME_PFN),
					GetDlgItem (hDlg, IDC_NAME_CUSTOM),
					GetDlgItem (hDlg, IDC_CUSTOM_NAME),
					GetDlgItem (hDlg, IDC_SIGNPKG)
				};
				g_inputsave.writeStringValue (user, L"OutputDirectory", GetWindowTextWString (hOutInfo [0]));
				g_inputsave.writeUIntValue (user, L"BundleVersionMajor", std::stoi (GetWindowTextWString (hOutInfo [1])));
				g_inputsave.writeUIntValue (user, L"BundleVersionMinor", std::stoi (GetWindowTextWString (hOutInfo [2])));
				g_inputsave.writeUIntValue (user, L"BundleVersionBuild", std::stoi (GetWindowTextWString (hOutInfo [3])));
				g_inputsave.writeUIntValue (user, L"BundleVersionRevision", std::stoi (GetWindowTextWString (hOutInfo [4])));
				g_inputsave.writeIntValue (user, L"OutputFileNameMethod", IsCheckboxChecked (hOutInfo [6]));
				g_inputsave.writeStringValue (user, L"CustomOutputFileName", GetWindowTextWString (hOutInfo [7]));
				g_inputsave.writeBoolValue (user, L"SignPackageAfterPackagedSuccessfully", IsCheckboxChecked (hOutInfo [8]));
			}
			return TRUE;
		} break;
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP)wParam;
			WCHAR filePath [MAXSHORT] = {0};
			UINT fileCount = DragQueryFileW (hDrop, 0xFFFFFFFF, NULL, 0); // 获取文件数量
			std::vector <std::vector <std::wstring>> rows;
			std::vector <std::wnstring> failed;
			std::vector <std::wnstring> success;
			HWND hList = GetDlgItem (hDlg, IDC_APPXLIST);
			for (UINT i = 0; i < fileCount; i++)
			{
				DragQueryFileW (hDrop, i, filePath, MAX_PATH);
				std::vector <std::wstring> r;
				GetTableRowFromPackage (filePath, r);
				if (!r.empty ()) { rows.push_back (r); success.push_back (filePath); }
				else failed.push_back (filePath);
			}
			using tablerow = std::vector<std::wstring>;
			AddListContent (hList, thead, rows, true,
				[] (const tablerow& r1, const tablerow& r2) -> bool {
				if (r1.size () > 2 && r2.size () > 2)
				{
					std::vector <WCHAR> path1 (r1 [0].length () + r1 [1].length () + 8, 0);
					std::vector <WCHAR> path2 (r2 [0].length () + r2 [1].length () + 8, 0);
					PathCombineW (path1.data (), r1 [1].c_str (), r1 [0].c_str ());
					PathCombineW (path2.data (), r2 [1].c_str (), r2 [0].c_str ());
					return std::wnstring::equals (std::wnstring (path1.data ()), std::wnstring (path2.data ()));
				}
				return false;
			},
				[&success, &failed] (const tablerow& tr) {
				std::vector <WCHAR> path (tr [0].length () + tr [1].length () + 8, 0);
				PathCombineW (path.data (), tr [1].c_str (), tr [0].c_str ());
				success.erase (std::remove (success.begin (), success.end (), std::wnstring (path.data ())), success.end ());
				push_unique (failed, std::wnstring (path.data ()));
			}, 250);
			if (failed.size ())
			{
				std::wstring result;
				auto appendFilesByDirectory = [&result] (const std::vector<std::wnstring>& files) {
					std::map<std::wstring, std::vector<std::wstring>> dirMap;
					for (auto& fullPath : files) {
						wchar_t dir [MAX_PATH] = {0};
						wchar_t file [MAX_PATH] = {0};
						wcsncpy_s (dir, fullPath.c_str (), MAX_PATH);
						LPWSTR pFile = PathFindFileNameW (dir);
						wcsncpy_s (file, pFile, MAX_PATH);
						PathRemoveFileSpecW (dir);
						dirMap [dir].emplace_back (file);
					}
					for (auto& pair : dirMap) {
						result += fmt::format (GetRCStringSW (IDS_FORMAT_INDIR).c_str (), pair.first.c_str ());
						for (auto& filename : pair.second) {
							result += L"        " + filename + L"\r\n";
						}
						result += L"\r\n";
					}
				};
				if (!success.empty ())
				{
					std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_SUCCESS);
					std::vector<WCHAR> lpbuf (title.length () + 64, 0);
					swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), success.size (), fileCount);
					result += lpbuf.data ();
					result += L"\r\n";
					appendFilesByDirectory (success);
					result += L"\r\n";
				}
				if (!failed.empty ())
				{
					std::wstring title = GetRCStringSW (IDS_FILE_MULTIPLE_FAILED);
					std::vector<WCHAR> lpbuf (title.length () + 64, 0);
					swprintf (lpbuf.data (), lpbuf.size (), title.c_str (), failed.size (), fileCount);
					result += lpbuf.data ();
					result += L"\r\n";
					appendFilesByDirectory (failed);
				}
				MessageBoxLongStringW (hDlg, result.c_str (), GetRCStringSW (IDS_TITLE_RESULT).c_str ());
			}
			DragFinish (hDrop);
			return 0;
		}
		break;
	}
	return FALSE;
}
int APIENTRY wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	raii relp ([] () {
		if (g_pTaskbarList)
		{
			g_pTaskbarList->Release ();
			g_pTaskbarList = NULL;
		}
	});
	SetProcessWorkingSetSize (GetCurrentProcess (), -1, -1);
	SetupInstanceEnvironment ();
	g_hIcon = ScopedHICON (LoadIconW (hInstance, MAKEINTRESOURCE (IDI_ICONMAIN)));
	CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	OleInitialize (NULL);
	CoCreateInstance (CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&g_pTaskbarList));
	DialogBoxW (hInstance, MAKEINTRESOURCEW (IDD_DIALOGMAIN), NULL, WndProc);
	if (g_pTaskbarList)
	{
		g_pTaskbarList->Release ();
		g_pTaskbarList = NULL;
	}
	CoUninitialize ();
	return 0;
}