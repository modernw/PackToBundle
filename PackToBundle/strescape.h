#pragma once
#include <string>
#include <sstream>
#include <stdexcept>
#include <codecvt>
#include <locale>
#include <iomanip>

std::string EscapeJsString (const std::string &input)
{
	std::ostringstream oss;
	for (char c : input) 
	{
		switch (c)
		{
			case '\b': oss << "\\b"; break;
			case '\f': oss << "\\f"; break;
			case '\n': oss << "\\n"; break;
			case '\r': oss << "\\r"; break;
			case '\t': oss << "\\t"; break;
			case '\v': oss << "\\v"; break;
			case '\"': oss << "\\\""; break;
			case '\'': oss << "\\'"; break;
			case '\\': oss << "\\\\"; break;
			default:
				if (c >= 0 && c <= 0x1F) 
				{
					oss << "\\x" << std::hex << std::setw (2) << std::setfill ('0')
						<< static_cast <int> (c);
				}
				else oss << c;
		}
	}
	return oss.str ();
}
std::string UnescapeJsString (const std::string &input)
{
	std::ostringstream oss;
	for (size_t i = 0; i < input.size (); i ++)
	{
		if (input [i] == '\\') 
		{
			if (++i >= input.size ())
			{
				throw std::invalid_argument ("Invalid escape sequence");
			}
			switch (input [i]) 
			{
				case 'b': oss << '\b'; break;
				case 'f': oss << '\f'; break;
				case 'n': oss << '\n'; break;
				case 'r': oss << '\r'; break;
				case 't': oss << '\t'; break;
				case 'v': oss << '\v'; break;
				case '\"': oss << '\"'; break;
				case '\'': oss << '\''; break;
				case '\\': oss << '\\'; break;
				case 'x':
				{ 
					if (i + 2 >= input.size ()) 
					{
						throw std::invalid_argument ("Invalid hex escape");
					}
					std::string hex = input.substr (i + 1, 2);
					i += 2;
					oss << static_cast <char> (std::stoi (hex, nullptr, 16));
					break;
				}
				case 'u':
				{
					if (i + 4 >= input.size ()) 
					{
						throw std::invalid_argument ("Invalid unicode escape");
					}
					std::string hex = input.substr (i + 1, 4);
					i += 4;
					int code = std::stoi (hex, nullptr, 16);
					if (code > 0xFF) 
					{
						throw std::out_of_range ("Only support ASCII in this implementation");
					}
					oss << static_cast<char>(code);
					break;
				}
				default:
					throw std::invalid_argument ("Unknown escape sequence");
			}
		}
		else oss << input [i];
	}
	return oss.str ();
}
std::wstring EscapeJsString (const std::wstring &input)
{
	std::wostringstream oss;
	for (wchar_t c : input) 
	{
		switch (c)
		{
			case L'\b': oss << L"\\b"; break;
			case L'\f': oss << L"\\f"; break;
			case L'\n': oss << L"\\n"; break;
			case L'\r': oss << L"\\r"; break;
			case L'\t': oss << L"\\t"; break;
			case L'\v': oss << L"\\v"; break;
			case L'\"': oss << L"\\\""; break;
			case L'\\': oss << L"\\\\"; break;
			default:
				oss << c;
				/*
				if (c <= 0x1F)
				{ 
					oss << L"\\x" << std::hex << std::setw (2) << std::setfill (L'0')
						<< static_cast <int> (c);
				}
				else if (c <= 0xFFFF) 
				{  
					oss << L"\\u" << std::hex << std::setw (4) << std::setfill (L'0')
						<< static_cast <int> (c);
				}
				else 
				{ 
					c -= 0x10000;
					wchar_t high = 0xD800 + (c >> 10);
					wchar_t low = 0xDC00 + (c & 0x3FF);
					oss << L"\\u" << std::hex << std::setw (4) << high
						<< L"\\u" << std::hex << std::setw (4) << low;
				}
				*/
		}
	}
	return oss.str ();
}
std::wstring UnescapeJsString (const std::wstring &input)
{
	std::wostringstream oss;
	for (size_t i = 0; i < input.size (); ++i)
	{
		if (input [i] == L'\\')
		{
			if (++i >= input.size ()) throw std::invalid_argument ("Invalid escape");
			switch (input [i])
			{
				case L'b': oss << L'\b'; break;
				case L'f': oss << L'\f'; break;
				case L'n': oss << L'\n'; break;
				case L'r': oss << L'\r'; break;
				case L't': oss << L'\t'; break;
				case L'v': oss << L'\v'; break;
				case L'\"': oss << L'\"'; break;
				case L'\\': oss << L'\\'; break;
				case L'x':
				{ 
					if (i + 2 >= input.size ()) throw std::invalid_argument ("Invalid hex");
					std::wstring hex = input.substr (i + 1, 2);
					i += 2;
					wchar_t val = static_cast <wchar_t> (std::stoi (hex, nullptr, 16));
					oss << val;
					break;
				}
				case L'u': 
				{ 
					if (i + 4 >= input.size ()) throw std::invalid_argument ("Invalid unicode");
					std::wstring hex = input.substr (i + 1, 4);
					i += 4;
					wchar_t code = static_cast<wchar_t>(std::stoi (hex, nullptr, 16));
					if (code >= 0xD800 && code <= 0xDBFF)
					{ 
						if (i + 6 >= input.size () || input.substr (i + 1, 2) != L"\\u")
						{
							throw std::invalid_argument ("Incomplete surrogate pair");
						}
						i += 2; 
						std::wstring hex2 = input.substr (i + 1, 4);
						i += 4;
						wchar_t low = static_cast <wchar_t> (std::stoi (hex2, nullptr, 16));
						if (low < 0xDC00 || low > 0xDFFF) 
						{
							throw std::invalid_argument ("Invalid low surrogate");
						}
						// 合并代理对
						oss << static_cast <wchar_t> (
							0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00)
							);
					}
					else oss << code;
					break;
				}
				default: throw std::invalid_argument ("Unknown escape");
			}
		}
		else oss << input [i];
	}
	return oss.str ();
}
std::string EscapeXmlString (const std::string &input) 
{
	std::ostringstream oss;
	for (char c : input) 
	{
		switch (c)
		{
			case '&':  oss << "&amp;"; break;
			case '<':  oss << "&lt;";  break;
			case '>':  oss << "&gt;";  break;
			case '"':  oss << "&quot;"; break;
			case '\'': oss << "&apos;"; break;
			default:
				unsigned char uc = static_cast <unsigned char> (c);
				if (uc < 0x20 && uc != '\t' && uc != '\n' && uc != '\r') 
				{
					oss << "&#x" << std::hex << std::setw (2) << std::setfill ('0')
						<< static_cast<int>(uc) << ";";
				}
				else 
				{
					oss << c;
				}
		}
	}
	return oss.str ();
}
std::wstring EscapeXmlString (const std::wstring &input)
{
	std::wostringstream oss;
	for (wchar_t c : input) 
	{
		switch (c)
		{
			case L'&':  oss << L"&amp;"; break;
			case L'<':  oss << L"&lt;";  break;
			case L'>':  oss << L"&gt;";  break;
			case L'"':  oss << L"&quot;"; break;
			case L'\'': oss << L"&apos;"; break;
			default:
				if (c < 0x20 && c != L'\t' && c != L'\n' && c != L'\r') 
				{
					oss << L"&#x" << std::hex << static_cast<unsigned int>(c) << L";";
				}
				else
				{
					oss << c;
				}
		}
	}
	return oss.str ();
}
template <typename CharT> unsigned long ParseNumericEntity (const std::basic_string <CharT> &entity, bool hex)
{
	try 
	{
		size_t idx = 0;
		unsigned long code = std::stoul (entity, &idx, hex ? 16 : 10);
		if (idx != entity.size ()) 
		{
			throw std::invalid_argument ("Invalid numeric entity");
		}
		return code;
	}
	catch (const std::exception &)
	{
		throw std::invalid_argument ("Invalid numeric entity");
	}
}
#undef max
std::string UnescapeXmlString (const std::string &input)
{
	std::ostringstream oss;
	for (size_t i = 0; i < input.size (); ++i) 
	{
		if (input [i] == '&')
		{
			size_t end = input.find (';', i);
			if (end == std::string::npos) 
			{
				throw std::invalid_argument ("Invalid entity: no semicolon");
			}
			std::string entity = input.substr (i + 1, end - i - 1);
			i = end; 
			if (entity == "amp")
			{
				oss << '&';
			}
			else if (entity == "lt") 
			{
				oss << '<';
			}
			else if (entity == "gt")
			{
				oss << '>';
			}
			else if (entity == "quot") 
			{
				oss << '"';
			}
			else if (entity == "apos")
			{
				oss << '\'';
			}
			else if (!entity.empty () && entity [0] == '#')
			{
				bool hex = (entity.size () > 1 && (entity [1] == 'x' || entity [1] == 'X'));
				size_t start = hex ? 2 : 1;
				std::string numStr = entity.substr (start);
				unsigned long code = ParseNumericEntity (numStr, hex);
				if (code > std::numeric_limits <unsigned char>::max ()) 
				{
					throw std::out_of_range ("Numeric entity out of char range");
				}
				oss << static_cast <char> (code);
			}
			else 
			{
				throw std::invalid_argument ("Unknown entity: " + entity);
			}
		}
		else {
			oss << input [i];
		}
	}
	return oss.str ();
}
std::wstring UnescapeXmlString (const std::wstring &input) 
{
	std::wostringstream oss;
	for (size_t i = 0; i < input.size (); ++i) 
	{
		if (input [i] == L'&') 
		{
			size_t end = input.find (L';', i);
			if (end == std::wstring::npos)
			{
				throw std::invalid_argument ("Invalid entity: no semicolon");
			}
			std::wstring entity = input.substr (i + 1, end - i - 1);
			i = end;
			if (entity == L"amp") 
			{
				oss << L'&';
			}
			else if (entity == L"lt") 
			{
				oss << L'<';
			}
			else if (entity == L"gt")
			{
				oss << L'>';
			}
			else if (entity == L"quot")
			{
				oss << L'"';
			}
			else if (entity == L"apos") 
			{
				oss << L'\'';
			}
			else if (!entity.empty () && entity [0] == L'#')
			{
				bool hex = (entity.size () > 1 && (entity [1] == L'x' || entity [1] == L'X'));
				size_t start = hex ? 2 : 1;
				std::wstring numStr = entity.substr (start);
				unsigned long code = ParseNumericEntity (numStr, hex);
				if (code > static_cast <unsigned long> (std::numeric_limits <wchar_t>::max ()))
				{
					throw std::out_of_range ("Numeric entity out of wchar_t range");
				}
				oss << static_cast<wchar_t>(code);
			}
			else 
			{
				throw std::invalid_argument ("Unknown entity");
			}
		}
		else
		{
			oss << input [i];
		}
	}
	return oss.str ();
}
std::wstring EncodeToUri (const std::wstring &str)
{
	return MPStringToStdW (Uri::EscapeUriString (CStringToMPString (str)));
}
std::wstring DecodeFromUri (const std::wstring &uri)
{
	return MPStringToStdW (Uri::UnescapeDataString (CStringToMPString (uri)));
}