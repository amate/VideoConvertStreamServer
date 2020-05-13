
#pragma once

#include <string>

namespace CodeConvert
{
	std::string		ShiftJISfromUTF16(const std::wstring& utf16);
	std::wstring	UTF16fromShiftJIS(const std::string& sjis);

	std::string ConvertUTF8fromUTF16(const std::wstring& utf16);
	std::wstring ConvertUTF16fromUTF8(const std::string& utf8);
}	// namespace
