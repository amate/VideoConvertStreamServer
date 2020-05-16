#pragma once

#include <string>
#include <array>
#include <vector>
#include <boost\filesystem.hpp>

struct Config
{
	static void LoadConfig();
	static void SaveConfig();

	static int			s_httpServerPort;
	static std::wstring s_rootFolder;
	static std::vector<std::wstring> s_mediaExtList;
	static std::vector<std::wstring> s_directPlayMediaExtList;
	static int			s_maxCacheFolderCount;

	enum VideoConvertEngine {
		kBuiltinFFmpeg = 0,
		kExternalFFmpeg,
		kNVEncC,

		kMaxEngineNum,
	};
	static std::array<std::wstring, kMaxEngineNum> s_videoConvertEngineName;
	static int			 s_videoConvertEngine;

	struct VideoConvertEngineInfo {
		std::wstring enginePath;
		std::wstring commandLine;
		std::wstring defaultCommandLine;
		std::wstring deinterlaceParam;
	};
	static std::array<VideoConvertEngineInfo, kMaxEngineNum> s_arrVideoConvertEngineInfo;

	static boost::filesystem::path GetEnginePath();
	static std::wstring	BuildVCEngineCommandLine(const boost::filesystem::path& mediaPath, 
												 const boost::filesystem::path& segmentFolderPath);
};