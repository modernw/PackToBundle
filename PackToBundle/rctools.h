#pragma once
#include <Windows.h>
#include <WinBase.h>
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "typestrans.h"
#include "module.h"


// 返回的指针如果非空则一定需要用 free 释放
LPWSTR GetRCStringW (UINT resID, HMODULE hModule hModule_DefaultParam)
{
	size_t bufferSize = 256;
	WCHAR *buffer = NULL;
	int length = 0;
	do
	{
		if (buffer)
		{
			free (buffer);
			buffer = NULL;
		}
		buffer = (LPWSTR)calloc (bufferSize, sizeof (WCHAR));
		length = LoadStringW (hModule, resID, buffer, bufferSize);
		if ((size_t)length >= bufferSize)
		{
			bufferSize += 20;
		}
	} while ((size_t)length >= bufferSize && buffer);
	return buffer;
}
// 返回的指针如果非空则一定需要用 free 释放
LPSTR GetRCStringA (UINT resID, HMODULE hModule hModule_DefaultParam)
{
	size_t bufferSize = 256;
	char *buffer = NULL;
	int length = 0;
	do
	{
		if (buffer)
		{
			free (buffer);
			buffer = NULL;
		}
		buffer = (LPSTR)calloc (bufferSize, sizeof (char));
		length = LoadStringA (hModule, resID, buffer, bufferSize);
		if ((size_t)length >= bufferSize)
		{
			bufferSize += 20;
		}
	} while ((size_t)length >= bufferSize && buffer);
	return buffer;
}

HICON LoadRCIcon (UINT resID, HMODULE hModule)
{
	HICON hIcon = (HICON)LoadImageW (hModule, MAKEINTRESOURCEW (resID), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
	if (hIcon == NULL) return NULL;
	return hIcon;
}
HRSRC FindResourceByName (LPCWSTR resourceName, LPCWSTR resourceType, HMODULE hModule)
{
	return FindResourceW (hModule, resourceName, resourceType);
}
#ifdef __cplusplus
#include <string>
std::wstring GetRCStringSW (UINT resID, HMODULE hModule hModule_DefaultParam)
{
	LPWSTR pstr = GetRCStringW (resID, hModule);
	std::wstring sobj (L"");
	if (pstr) sobj += pstr;
	if (pstr) free (pstr);
	return sobj;
}
std::string GetRCStringSA (UINT resID, HMODULE hModule hModule_DefaultParam)
{
	LPSTR pstr = GetRCStringA (resID, hModule);
	std::string sobj ("");
	if (pstr) sobj += pstr;
	if (pstr) free (pstr);
	return sobj;
}
#endif
#if defined (__cplusplus) && defined (__cplusplus_cli)
using namespace System;
String ^GetRCStringCli (UINT resID, HMODULE hModule hModule_DefaultParam)
{
	size_t bufferSize = 256;
	wchar_t *buffer = nullptr;
	int length = 0;
	do
	{
		delete [] buffer;
		buffer = new wchar_t [bufferSize];
		length = LoadStringW ( (hModule), resID, buffer, bufferSize);
		if ((size_t)length >= bufferSize)
		{
			bufferSize += 20;
		}
	} while ((size_t)length >= bufferSize);
	String ^result = gcnew String (buffer);
	delete [] buffer;
	return result;
}
#define GetRCIntValue(_UINT__resID_) toInt (GetRCStringCli (_UINT__resID_))
#define GetRCDoubleValue(_UINT__resID_) toDouble (GetRCStringCli (_UINT__resID_))
#define GetRCBoolValue(_UINT__resID_) toBool (GetRCStringCli (_UINT__resID_))
#define GetRCDateTimeValue(_UINT__resID_) toDateTime (GetRCStringCli (_UINT__resID_))
#define rcString(resID) GetRCStringCli (resID)
#define rcInt(resID) GetRCIntValue (resID)
#define rcDouble(resID) GetRCDoubleValue (resID)
#define rcBool(resID) GetRCBoolValue (resID)
#define rcDTime(resID) GetRCDateTimeValue (resID)
#define rcIcon(resID) LoadRCIcon (resID)
#endif