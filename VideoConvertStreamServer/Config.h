#pragma once

#include <string>
#include <array>
#include <vector>
#include <boost\filesystem.hpp>

struct Config
{
	static void LoadConfig();
	static void SaveConfig();

	static inline int			s_httpServerPort = 7896;
	static inline std::string	s_password;
	static inline std::wstring s_rootFolder;
	static inline std::vector<std::wstring> s_mediaExtList = {
		L"ts", L"mp4", L"mkv", L"webm", L"mpeg", L"avi", L"flv", L"wmv", L"asf", L"mov",
	};
	static inline std::vector<std::wstring> s_directPlayMediaExtList = {
		L"mp4", L"webm"
	};
	static inline int			s_maxCacheFolderCount = 2;

	enum VideoConvertEngine {
		kBuiltinFFmpeg = 0,
		kExternalFFmpeg,
		kNVEncC,

		kMaxEngineNum,
	};
	static inline std::array<std::wstring, kMaxEngineNum> s_videoConvertEngineName = {
		L"“à‘  ffmpeg", L"ŠO•” ffmpeg", L"NVEncC",
	};
	static inline int			 s_videoConvertEngine = Config::kBuiltinFFmpeg;

	static inline std::string	 s_defaultSortOrder = "sort=name&order=asc"; // Name (A - Z) ;
	static inline const std::pair<std::string, std::string> kNameSortOrderPair[] = {
		{"Name (A - Z)", "sort=name&order=asc"},
		{"Name (Z - A)", "sort=name&order=desc"},
		{"Date (New - Old)", "sort=date&order=asc"},
		{"Date (Old - New)", "sort=date&order=desc"},
	};

	struct VideoConvertEngineInfo {
		std::wstring enginePath;
		std::wstring commandLine;
		std::wstring defaultCommandLine;
		std::wstring deinterlaceParam;
	};
	static inline std::array<VideoConvertEngineInfo, kMaxEngineNum> s_arrVideoConvertEngineInfo;


	static boost::filesystem::path GetEnginePath();
	static std::wstring	BuildVCEngineCommandLine(const boost::filesystem::path& mediaPath, 
												 const boost::filesystem::path& segmentFolderPath);
};