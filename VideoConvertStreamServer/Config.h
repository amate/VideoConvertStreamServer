#pragma once

#include <string>

struct Config
{
	static void LoadConfig();

	static int			s_httpServerPort;
	static std::wstring s_rootFolder;
	static std::wstring s_NVEncCPath;
	static std::wstring s_hlsParam;
	static std::wstring s_extra_DeinterlaceParam;
	static int			s_maxCacheFolderCount;
};