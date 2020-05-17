
#include "stdafx.h"
#include "Config.h"
#include <fstream>
#include <random>

#include <boost\format.hpp>

#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\Logger.h"
#include "Utility\CodeConvert.h"

using nlohmann::json;
using namespace CodeConvert;


/////////////////////////////////////////////////////////////////

void Config::LoadConfig()
{
	auto configPath = GetExeDirectory() / L"Config.json";
	std::ifstream fs(configPath.c_str(), std::ios::in | std::ios::binary);
	if (!fs) {
		ERROR_LOG << L"Config.json が開けませんでした...";
		return;
	}
	json jsonConfig;
	fs >> jsonConfig;
	auto& configRoot = jsonConfig["Config"];

	s_httpServerPort = configRoot.value<int>("HttpServerPort", s_httpServerPort);
	s_password = configRoot.value<std::string>("Password", "");
	if (s_password.empty()) {
		enum { kMaxGeneratePasswordLength = 16 };
		char characterPool[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYG0123456789_-";
		std::random_device rand;
		std::uniform_int_distribution<> dist(0, static_cast<int>(std::size(characterPool) - 2));
		for (int i = 0; i < kMaxGeneratePasswordLength; ++i) {
			s_password += characterPool[dist(rand)];
		}
	}
	s_rootFolder = ConvertUTF16fromUTF8(configRoot.value<std::string>("RootFolder", ""));

	auto& jsonMediaExtList = configRoot["MediaExtList"];
	if (jsonMediaExtList.is_array()) {
		s_mediaExtList.clear();
		for (auto& mediaExt : jsonMediaExtList) {
			s_mediaExtList.emplace_back(ConvertUTF16fromUTF8(mediaExt.get<std::string>()));
		}
	}
	auto& jsonDirectPlayMediaExtList = configRoot["DirectPlayMediaExtList"];
	if (jsonDirectPlayMediaExtList.is_array()) {
		s_directPlayMediaExtList.clear();
		for (auto& mediaExt : jsonDirectPlayMediaExtList) {
			s_directPlayMediaExtList.emplace_back(ConvertUTF16fromUTF8(mediaExt.get<std::string>()));
		}
	}

	s_maxCacheFolderCount = configRoot.value<int>("MaxCacheFolderCount", s_maxCacheFolderCount);
	if (s_maxCacheFolderCount < 0) {
		s_maxCacheFolderCount = 1;
	}
	s_videoConvertEngine = configRoot.value<int>("VideoConvertEngine", s_videoConvertEngine);

	s_defaultSortOrder = configRoot.value<std::string>("DefaultSortOrder", s_defaultSortOrder);

	for (int i = 0; i < kMaxEngineNum; ++i) {
		std::string name = (boost::format("VideoConvertEngine%1%") %  i).str();
		auto& jsonVCE = configRoot[name];
		if (jsonVCE.is_object()) {
			auto& VCEInfo = s_arrVideoConvertEngineInfo[i];
			if (i == kBuiltinFFmpeg) {
				VCEInfo.enginePath = (GetExeDirectory() / L"ffmpeg" / L"ffmpeg.exe").wstring();
			} else {
				VCEInfo.enginePath = ConvertUTF16fromUTF8(jsonVCE.value<std::string>("EnginePath", ""));
			}
			VCEInfo.defaultCommandLine = ConvertUTF16fromUTF8(jsonVCE.value<std::string>("DefaultCommandLine", ""));
			VCEInfo.commandLine = ConvertUTF16fromUTF8(jsonVCE.value<std::string>("CommandLine", ""));
			if (VCEInfo.commandLine.empty()) {
				VCEInfo.commandLine = VCEInfo.defaultCommandLine;
			}
			VCEInfo.deinterlaceParam = ConvertUTF16fromUTF8(jsonVCE.value<std::string>("DeinterlaceParam", ""));
		}
	}
}

void Config::SaveConfig()
{
	json jsonConfig;

	auto configPath = GetExeDirectory() / L"Config.json";
	std::ifstream fs(configPath.c_str(), std::ios::in | std::ios::binary);
	if (fs) {
		fs >> jsonConfig;
		fs.close();
	} else {

	}
	auto& configRoot = jsonConfig["Config"];

	configRoot["HttpServerPort"] = s_httpServerPort;
	configRoot["Password"] = s_password;
	configRoot["RootFolder"] = ConvertUTF8fromUTF16(s_rootFolder);

	configRoot["MediaExtList"] = json::array();
	auto& jsonMediaExtList = configRoot["MediaExtList"];
	for (const auto& mediaExt : s_mediaExtList) {
		jsonMediaExtList.push_back(ConvertUTF8fromUTF16(mediaExt));
	}

	configRoot["DirectPlayMediaExtList"] = json::array();
	auto& jsonDirectPlayMediaExtList = configRoot["DirectPlayMediaExtList"];
	for (const auto& mediaExt : s_directPlayMediaExtList) {
		jsonDirectPlayMediaExtList.push_back(ConvertUTF8fromUTF16(mediaExt));
	}

	configRoot["MaxCacheFolderCount"] = s_maxCacheFolderCount;
	configRoot["VideoConvertEngine"] = s_videoConvertEngine;
	for (int i = 0; i < kMaxEngineNum; ++i) {
		std::string name = (boost::format("VideoConvertEngine%1%") % i).str();
		auto& jsonVCE = configRoot[name];
		const auto& VCEInfo = s_arrVideoConvertEngineInfo[i];
		if (i != kBuiltinFFmpeg) {
			jsonVCE["EnginePath"] = ConvertUTF8fromUTF16(VCEInfo.enginePath);
		}
		jsonVCE["CommandLine"] = ConvertUTF8fromUTF16(VCEInfo.commandLine);
	}

	configRoot["DefaultSortOrder"] = s_defaultSortOrder;

	std::ofstream fsw(configPath.c_str(), std::ios::out | std::ios::binary);
	fsw << std::setw(4) <<  jsonConfig;
	fsw.close();
}

boost::filesystem::path Config::GetEnginePath()
{
	const auto& VCEInfo = s_arrVideoConvertEngineInfo[s_videoConvertEngine];	
	return VCEInfo.enginePath;
}

std::wstring Config::BuildVCEngineCommandLine(const boost::filesystem::path& mediaPath,
											  const boost::filesystem::path& segmentFolderPath)
{
	const fs::path actualMediaPath = boost::filesystem::path(mediaPath).make_preferred();

	const auto& VCEInfo = s_arrVideoConvertEngineInfo[s_videoConvertEngine];
	std::wstring commandLine = VCEInfo.commandLine;
	boost::replace_all(commandLine, L"\\n", L" ");

	boost::replace_all(commandLine, L"<input>", actualMediaPath.wstring().c_str());

	std::string ext = boost::to_lower_copy(actualMediaPath.extension().string());
	if (ext == ".ts") {	// .ts ファイルは、デインターレース用のパラメーターを追加する
		boost::replace_all(commandLine, L"<DeinterlaceParam>", VCEInfo.deinterlaceParam);
	} else {
		boost::replace_all(commandLine, L"<DeinterlaceParam>", L"");
	}

	boost::replace_all(commandLine, L"<segmentFolder>", 
		boost::filesystem::path(segmentFolderPath).make_preferred().wstring().c_str());

	return commandLine;
}
