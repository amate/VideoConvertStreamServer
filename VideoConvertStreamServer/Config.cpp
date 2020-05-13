
#include "stdafx.h"
#include "Config.h"
#include <fstream>

#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\Logger.h"
#include "Utility\CodeConvert.h"

using nlohmann::json;
using namespace CodeConvert;


int			Config::s_httpServerPort = 7896;
std::wstring Config::s_rootFolder;
std::wstring Config::s_NVEncCPath;
std::wstring Config::s_hlsParam;
std::wstring Config::s_extra_DeinterlaceParam;
int			Config::s_maxCacheFolderCount = 2;

void Config::LoadConfig()
{
	auto configPath = GetExeDirectory() / L"Config.json";
	std::ifstream fs(configPath.c_str(), std::ios::in | std::ios::binary);
	if (!fs) {
		ERROR_LOG << L"Config.json ‚ªŠJ‚¯‚Ü‚¹‚ñ‚Å‚µ‚½...";
		return;
	}
	json jsonConfig;
	fs >> jsonConfig;
	auto& configRoot = jsonConfig["Config"];

	s_httpServerPort = configRoot["HttpServerPort"].get<int>();

	s_rootFolder = ConvertUTF16fromUTF8(configRoot["RootFolder"].get<std::string>());
	s_NVEncCPath = ConvertUTF16fromUTF8(configRoot["NVEncCPath"].get<std::string>());
	s_hlsParam = ConvertUTF16fromUTF8(configRoot["hlsParam"].get<std::string>());
	s_extra_DeinterlaceParam = ConvertUTF16fromUTF8(configRoot["extra_DeinterlaceParam"].get<std::string>());
	s_maxCacheFolderCount = configRoot["MaxCacheFolderCount"].get<int>();
	if (s_maxCacheFolderCount < 0) {
		s_maxCacheFolderCount = 1;
	}
}
