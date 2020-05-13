
#include "stdafx.h"
#include "CodeConvert.h"
#include <Windows.h>

namespace CodeConvert
{

	std::string ShiftJISfromUTF16(const std::wstring & utf16)
	{
		int requireBytes = ::WideCharToMultiByte(CP_ACP, 0, utf16.c_str(), utf16.length(), nullptr, 0, NULL, NULL);
		if (requireBytes > 0) {
			std::string sjis;
			sjis.resize(requireBytes);
			int ret = ::WideCharToMultiByte(CP_ACP, 0, utf16.c_str(), utf16.length(), (LPSTR)sjis.data(), requireBytes, NULL, NULL);
			if (ret > 0) {
				return sjis;
			}
		}
		return std::string();
	}

	std::wstring	UTF16fromShiftJIS(const std::string& sjis)
	{
		int requireChars = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, sjis.c_str(), sjis.length(), nullptr, 0);
		if (requireChars > 0) {
			std::wstring utf16;
			utf16.resize(requireChars);
			int ret = ::MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, sjis.c_str(), sjis.length(), (LPWSTR)utf16.data(), requireChars);
			if (ret > 0) {
				return utf16;
			}
		}
		return std::wstring();
	}

	std::string ConvertUTF8fromUTF16(const std::wstring& utf16)
	{
		std::string utf8;
		int size = ::WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), utf16.length(), (LPSTR)utf8.data(), 0, nullptr, nullptr);
		if (size > 0) {
			utf8.resize(size);
			if (::WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), utf16.length(), (LPSTR)utf8.data(), size, nullptr, nullptr))
				return utf8;
		}
		return utf8;
	}

	std::wstring ConvertUTF16fromUTF8(const std::string& utf8)
	{
		std::wstring utf16;
		int size = ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8.length(), (LPWSTR)utf16.data(), 0);
		if (size > 0) {
			utf16.resize(size);
			if (::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8.length(), (LPWSTR)utf16.data(), size))
				return utf16;
		}
		return utf16;
	}





}	// namespace CodeConvert