#include "stdafx.h"
#include "HttpServer.h"
#include <regex>
#include <fstream>
#include <sstream>
#include <list>

#include <boost\asio\spawn.hpp>
#include <boost\format.hpp>
#include <boost\algorithm\string\case_conv.hpp>
#include <boost\algorithm\string\replace.hpp>
#include <boost\optional.hpp>

#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <Shlwapi.h>	// for StrCmpLogicalW
#pragma comment(lib,"shlwapi.lib")

#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Utility\json.hpp"
#include "Utility\CodeConvert.h"
#include "Config.h"
#include "MainDlg.h"

namespace asio = boost::asio;
using boost::wformat;
using boost::asio::ip::tcp;
using nlohmann::json;
using namespace CodeConvert;

namespace {

	enum { kDefaultHttpServerPort = 7896 };

	const std::unordered_map<std::string, std::string> kExtContentType = {
		{".html", "text/html"},
		{".js", "text/javascript"},
		{".css", "text/css"},
		{".woff2", "font/woff2"},
		{".txt", "text/plain"},
		{".log", "text/plain"},

		{".m3u8", "application/x-mpegURL"},
		{".ts", "video/mp2t"},

		{".png", "image/png"},
		{".jpg", "image/jpeg"},
		{".jpeg", "image/jpeg"},

	};

	const std::string kMediaExt[] = {
		".ts", ".mp4", ".mkv", ".webm", ".mpeg", ".avi", ".flv", ".wmv", ".asf", ".mov",
	};

	const std::string kDirectPlayMediaExt[] = {
		".mp4", ".webm"
	};

	std::list<std::wstring> g_playHistory;

	std::string	URLDecode(const std::string& src)
	{
		auto funcHexToInt = [](char c) -> int {
			if ('0' <= c && c <= '9') {
				return c - '0';
			} else if ('a' <= c && c <= 'f') {
				return c - 'a' + 10;
			} else if ('A' <= c && c <= 'F') {
				return c - 'A' + 10;
			} else {
				ATLASSERT(FALSE);
				return 0;
			}
		};

		std::string dest;
		for (auto it = src.begin(); it != src.end(); ++it) {
			switch (*it)
			{
			case '%': 
			{
				++it;
				ATLASSERT(it != src.end());
				char c1 = *it;
				++it;
				ATLASSERT(it != src.end());
				char c2 = *it;
				char destc = static_cast<char>((funcHexToInt(c1) << 4) | funcHexToInt(c2));
				dest.push_back(destc);
			}
			break;

			case '+':
			{
				dest.push_back(' ');
			}
			break;

			default:
				dest.push_back(*it);
				break;
			}
		}
		return dest;
	}

	bool	isMediaFile(const fs::path& filePath)
	{
		if (fs::is_regular_file(filePath)) {
			auto ext = filePath.extension().wstring();
			if (ext.empty()) {
				return false;
			}
			ext = ext.substr(1);
			boost::to_lower(ext);
			for (const auto& mediaExt : Config::s_mediaExtList) {
				if (ext == mediaExt) {
					return true;
				}
			}
		}
		return false;
	}

	bool	isDirectPlayFile(const fs::path& filePath)
	{
		auto ext = filePath.extension().wstring();
		if (ext.empty()) {
			return false;
		}
		ext = ext.substr(1);
		boost::to_lower(ext);
		for (const auto& mediaExt : Config::s_directPlayMediaExtList) {
			if (ext == mediaExt) {
				return true;
			}
		}
		return false;		
	}

	fs::path BuildWokringFolderPath(const fs::path& actualMediaPath)
	{
		const auto canonicalPath = fs::path(actualMediaPath).make_preferred();
		enum { kMaxTitleCount = 32 };
		std::hash<std::wstring> strhash;
		wchar_t workingFolderName[128] = L"";	// 最大でも "32 + 1 + 16 = 49" なので大丈夫
		const std::wstring shortFileName = canonicalPath.stem().wstring().substr(0, kMaxTitleCount);
		//swprintf_s(workingFolderName, L"%s_%zx", shortFileName.c_str(), strhash(actualMediaPath.wstring()));
		swprintf_s(workingFolderName, L"%zx", strhash(canonicalPath.wstring()));
		//std::wstring workingFolderName = io::str(wformat(L"%llx_working") % strhash(tsFileBaseName));
		fs::path segmentFolderPath = GetExeDirectory() / L"html" / L"stream" / workingFolderName;
		return segmentFolderPath.make_preferred();
	}


#define MAKELONGLONG(l, h) ((LONGLONG)(((LONG)(l) & 0xffffffff) | (((LONGLONG)(h) & 0xffffffff) << 32)))

	LONGLONG GetFileCreateTime(const fs::path& path)
	{
		WIN32_FIND_DATA wfd = {};
		HANDLE hfolder = ::FindFirstFile(path.c_str(), &wfd);
		::FindClose(hfolder);
		LONGLONG creatTime = 0;
		creatTime = MAKELONGLONG(wfd.ftCreationTime.dwLowDateTime, wfd.ftCreationTime.dwHighDateTime);
		return creatTime;
	}

	LONGLONG GetLastWriteTime(const fs::path& path)
	{
		WIN32_FIND_DATA wfd = {};
		HANDLE hfolder = ::FindFirstFile(path.c_str(), &wfd);
		::FindClose(hfolder);
		LONGLONG lastWriteTime = 0;
		lastWriteTime = MAKELONGLONG(wfd.ftLastWriteTime.dwLowDateTime, wfd.ftLastWriteTime.dwHighDateTime);
		return lastWriteTime;
	}

	std::string GetFileContentType(const fs::path& filePath)
	{
		std::string ext = filePath.extension().string();
		boost::to_lower(ext);

		std::string contentType;
		auto itfound = kExtContentType.find(ext);
		if (itfound != kExtContentType.end()) {
			contentType = itfound->second;

		} else {
			try {
				CRegKey rk;
				if (rk.Open(HKEY_CLASSES_ROOT, ConvertUTF16fromUTF8(ext).c_str(), KEY_READ) != ERROR_SUCCESS) {
					THROWEXCEPTION(L"rk.Open failed");
				}
				WCHAR temp[64] = L"";
				ULONG charLength = 64;
				if (rk.QueryStringValue(L"Content Type", temp, &charLength) != ERROR_SUCCESS) {
					THROWEXCEPTION(L"QueryStringValue(Content Type failed");
				}
				contentType = ConvertUTF8fromUTF16(temp);
			} catch (std::exception& e) {
				WARN_LOG << L"Regkey Content-Type not found - ext: " << ext << L" error:" << UTF16fromShiftJIS(e.what());
				contentType = "application/octet-stream";
			}
		}
		return contentType;
	}

	// ================================================

	bool ToggleMute()
	{
		HRESULT hr;
		IMMDeviceEnumerator* pEnum = NULL;
		IMMDevice* pEndpoint = NULL;
		IAudioEndpointVolume* pAudioEndVol = NULL;

		hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		// MMDevice インターフェースを取得
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnum));
		if (FAILED(hr)) {
			CoUninitialize();
			return false;
		}
		// 既定のマルチメディア出力デバイスを取得
		hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
		if (FAILED(hr)) {
			if (pEnum)
				pEnum->Release();
			CoUninitialize();
			return false;
		}
		// ボリュームオブジェクトを作成
		hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndVol);
		if (FAILED(hr)) {
			if (pEndpoint)
				pEndpoint->Release();
			if (pEnum)
				pEnum->Release();
			CoUninitialize();
			return false;
		}

		// ミュートトグル
		BOOL bMute = FALSE;
		hr = pAudioEndVol->GetMute(&bMute);
		bMute = !bMute;
		hr = pAudioEndVol->SetMute(bMute, NULL);

		if (pAudioEndVol)
			pAudioEndVol->Release();
		if (pEndpoint)
			pEndpoint->Release();
		if (pEnum)
			pEnum->Release();
		CoUninitialize();

		return bMute != 0;
	}

	int StepVolumeChange(bool bVolumeUp)
	{
		HRESULT hr;
		IMMDeviceEnumerator* pEnum = NULL;
		IMMDevice* pEndpoint = NULL;
		IAudioEndpointVolume* pAudioEndVol = NULL;

		hr = CoInitializeEx(0, COINIT_MULTITHREADED);
		// MMDevice インターフェースを取得
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnum));
		if (FAILED(hr)) {
			CoUninitialize();
			return 1;
		}
		// 既定のマルチメディア出力デバイスを取得
		hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pEndpoint);
		if (FAILED(hr)) {
			if (pEnum)
				pEnum->Release();
			CoUninitialize();
			return 2;
		}
		// ボリュームオブジェクトを作成
		hr = pEndpoint->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndVol);
		if (FAILED(hr)) {
			if (pEndpoint)
				pEndpoint->Release();
			if (pEnum)
				pEnum->Release();
			CoUninitialize();
			return 3;
		}
		float volLevel;
		hr = pAudioEndVol->GetMasterVolumeLevelScalar(&volLevel);
		if (bVolumeUp) {
			volLevel += 0.1f;
			if (volLevel > 1.0) {
				volLevel = 1.0f;
			}
		} else {
			volLevel -= 0.1f;
			if (volLevel < 0.0) {
				volLevel = 0.0f;
			}
		}
		hr = pAudioEndVol->SetMasterVolumeLevelScalar(volLevel, NULL);

#if 0
		float volmin, volmax, volinc;
		hr = pAudioEndVol->GetVolumeRange(&volmin, &volmax, &volinc);

		float n = (volmax - volmin) / volinc;

		// Step volume 
		hr = bVolumeUp ? pAudioEndVol->VolumeStepUp(NULL) : pAudioEndVol->VolumeStepDown(NULL);
#endif
		if (pAudioEndVol)
			pAudioEndVol->Release();
		if (pEndpoint)
			pEndpoint->Release();
		if (pEnum)
			pEnum->Release();
		CoUninitialize();

		int currentVolume = static_cast<int>(volLevel * 100);
		return currentVolume;
	}

	void SendJSON200OK(const std::string& jsonData, asio::yield_context& yield, tcp::socket& s)
	{
		std::stringstream	ss;
		ss << "HTTP/1.1 200 OK\r\n"
			<< "Content-Type: " << "application/json" << "\r\n"
			<< "Connection: close\r\n\r\n";
		std::string header = ss.str();
		boost::system::error_code ec;
		asio::async_write(s, asio::buffer(header), yield[ec]);
		if (ec) {
			ERROR_LOG << L"SendJSON200OK - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}
		asio::async_write(s, asio::buffer(jsonData), yield[ec]);
		if (ec) {
			ERROR_LOG << L"SendJSON200OK - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}

		INFO_LOG << L"\tsend: " << jsonData.size() << L" bytes";
		s.close();
	}

	void SendFile200OK(const fs::path& filePath, asio::yield_context& yield, tcp::socket& s)
	{
		auto fileSize = fs::file_size(filePath);
		std::ifstream fs(filePath.wstring(), std::ios::in | std::ios::binary);
		if (!fs) {
			ATLASSERT(FALSE);
			ERROR_LOG << L"file open failed: " << filePath.wstring();
			return;
		}
		std::string contentType = GetFileContentType(filePath);
		if (contentType == "text/plain") {
			contentType += "; charset=utf-8";
		}

		std::stringstream	ss;
		ss << "HTTP/1.1 200 OK\r\n"
			<< "Content-Type: " << contentType << "\r\n"
			<< "Content-Length: " << fileSize << "\r\n"
			<< "\r\n";
		std::string header = ss.str();
		boost::system::error_code ec;
		asio::async_write(s, asio::buffer(header), yield[ec]);
		if (ec) {
			ERROR_LOG << L"SendFile200OK - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}

		enum { kReadBufferSize = 4 * 1024 };
		auto buffer = std::make_unique<char[]>(kReadBufferSize);
		std::streamsize totalReadCount = 0;
		do {
			fs.read(buffer.get(), kReadBufferSize);
			auto readCount = fs.gcount();
			totalReadCount += readCount;
			asio::async_write(s, asio::buffer(buffer.get(), readCount), yield[ec]);
			if (ec) {
				ERROR_LOG << L"SendFile200OK - async_write failed: " << UTF16fromShiftJIS(ec.message());
				return;
			}
		} while (fs.good());

		INFO_LOG << L"\tsend: " << totalReadCount << L" bytes";
		s.close();
	}

	void SendRangeFile206PertialContent(const fs::path& filePath, std::pair<LONGLONG, LONGLONG> range, asio::yield_context& yield, tcp::socket& s)
	{
		auto fileSize = fs::file_size(filePath);
		if (range.second == -1) {
			range.second = fileSize - 1;
		}
		const LONGLONG contentLength = range.second - range.first + 1;

		std::ifstream fs(filePath.wstring(), std::ios::in | std::ios::binary);
		if (!fs) {
			ATLASSERT(FALSE);
			ERROR_LOG << L"file open failed: " << filePath.wstring();
			return;
		}
		std::string contentType = GetFileContentType(filePath);

		std::stringstream	ss;
		ss << "HTTP/1.1 206 Partial Content\r\n"
			<< "Content-Type: " << contentType << "\r\n"
			<< "Content-Range: bytes " << range.first << "-" << range.second << "/" << fileSize << "\r\n"
			<< "Content-Length: " << contentLength << "\r\n"
			<< "\r\n";
		std::string header = ss.str();
		boost::system::error_code ec;
		asio::async_write(s, asio::buffer(header), yield[ec]);
		if (ec) {
			ERROR_LOG << L"SendRangeFile206PertialContent - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}

		fs.seekg(range.first);

		enum { kReadBufferSize = 4 * 1024 };
		auto buffer = std::make_unique<char[]>(kReadBufferSize);
		std::streamsize totalReadCount = 0;
		do {
			const std::streamsize restBytes = contentLength - totalReadCount;
			const std::streamsize readBufferSize = min(restBytes, kReadBufferSize);

			fs.read(buffer.get(), readBufferSize);
			auto readCount = fs.gcount();
			totalReadCount += readCount;
			asio::async_write(s, asio::buffer(buffer.get(), readCount), yield[ec]);
			if (ec) {
				ERROR_LOG << L"SendRangeFile206PertialContent - async_write failed: " << UTF16fromShiftJIS(ec.message());
				return;
			}
			if (totalReadCount == contentLength) {
				break;	// finish!
			}
		} while (fs.good());

		INFO_LOG << L"\tsend: " << totalReadCount << L" bytes";
		s.close();
	}

	void FileTreeAPI(const std::wstring& searchFolderPath, const std::wstring& sort, const std::wstring& order, asio::yield_context& yield, tcp::socket& s)
	{
		struct FileItem {
			std::wstring name;
			bool	isFolder;
			LONGLONG lastWriteTime;
			std::wstring fileSize;

			FileItem(const std::wstring& name, bool isFolder, LONGLONG lastWriteTime, const std::wstring& fileSize) 
				: name(name), isFolder(isFolder), lastWriteTime(lastWriteTime), fileSize(fileSize) {}
		};

		std::list<FileItem> fileList;
		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path searchFolder = rootFolder / searchFolderPath;
		if (!fs::is_directory(searchFolder)) {
			json jsonFolder;
			jsonFolder["Status"] = "failed";
			jsonFolder["Message"] = "searchFolder not exists";
			SendJSON200OK(jsonFolder.dump(), yield, s);
			return;
		}

		for (auto p : fs::directory_iterator(searchFolder)) {
			bool isFolder = fs::is_directory(p);
			if (isMediaFile(p) || isFolder) {
				LONGLONG lastWriteTime = GetLastWriteTime(p.path());

				std::wstring strfileSize;
				if (!isFolder) {
					auto fileSize = fs::file_size(p.path());
					WCHAR tempBuffer[64] = L"";
					::StrFormatByteSizeW(fileSize, tempBuffer, 64);
					strfileSize = tempBuffer;
				}
				fileList.emplace_back(p.path().filename().wstring(), isFolder, lastWriteTime, strfileSize);
			}
		}
		std::function<bool(bool)> orderFunc;
		if (order == L"asc") {
			orderFunc = [](bool b) { return b; };
		} else if (order == L"desc") {
			orderFunc = [](bool b) { return !b; };
		} else {
			ATLASSERT(FALSE);
			WARN_LOG << L"order が不正な値です: " << order;
			orderFunc = [](bool b) { return b; };
		}

		const std::unordered_map<std::wstring, std::function<bool(const FileItem&, const FileItem&)>> kSortOperation = {
			{ L"name", 
				[orderFunc](const FileItem& f1, const FileItem& f2) -> bool {
					if (f1.isFolder == f2.isFolder) {
						return orderFunc(::StrCmpLogicalW(f1.name.c_str(), f2.name.c_str()) < 0);
					} else {
						return f1.isFolder;
					}
			}},
			{ L"date",
				[orderFunc](const FileItem& f1, const FileItem& f2) -> bool {
					if (f1.isFolder == f2.isFolder) {
						return orderFunc(f1.lastWriteTime > f2.lastWriteTime);
					} else {
						return f1.isFolder;
					}
			}},

		};
		auto itfound = kSortOperation.find(sort);
		if (itfound != kSortOperation.end()) {
			fileList.sort(itfound->second);
		} else {
			ATLASSERT(FALSE);
			WARN_LOG << L"sort が不正な値です: " << sort;
		}

		//std::sort(fileList.begin(), fileList.end(), 
		//	[&searchFolder](const std::wstring& n1, const std::wstring& n2) {
		//		bool isFolder1 = fs::is_directory(searchFolder / n1);
		//		bool isFolder2 = fs::is_directory(searchFolder / n2);
		//		if (isFolder1 == isFolder2) {
		//			return ::StrCmpLogicalW(n1.c_str(), n2.c_str()) < 0;
		//		} else {
		//			return isFolder1;
		//		}
		//});

		json jsonFileList = json::array();
		for (auto fileItem : fileList) {
			jsonFileList.push_back({
				{"name", ConvertUTF8fromUTF16(fileItem.name)},
				{"isFolder", fileItem.isFolder},
				{"FileSize", ConvertUTF8fromUTF16(fileItem.fileSize)},
			});
		}
		json jsonFolder;
		jsonFolder["Status"] = "ok";
		jsonFolder["FileList"] = jsonFileList;
		SendJSON200OK(jsonFolder.dump(), yield, s);
	}

	void PrepareHLSAPI(const std::wstring& mediaPath, asio::yield_context& yield, tcp::socket& s)
	{
		// 再生履歴更新
		g_playHistory.erase(std::remove(g_playHistory.begin(), g_playHistory.end(), mediaPath), g_playHistory.end());
		g_playHistory.emplace_front(mediaPath);

		if (isDirectPlayFile(mediaPath)) {
			auto playListURL = boost::replace_all_copy(mediaPath, L"#", L"<hash>");
			json jsonResponse;
			jsonResponse["Status"] = "ok";
			jsonResponse["DirectPlay"] = true;
			jsonResponse["playListURL"] = "/RawLoadAPI?path=" + ConvertUTF8fromUTF16(playListURL);
			SendJSON200OK(jsonResponse.dump(), yield, s);
			return;
		}

		try {	// 古いキャッシュフォルダを削除
			if (CurrentVideoConvertCount() == 0) {
				std::vector<std::pair<std::wstring, LONGLONG>> folderList;
				const fs::path streamFolderPath = GetExeDirectory() / L"html" / L"stream";
				for (auto p : fs::directory_iterator(streamFolderPath)) {
					if (fs::is_directory(p)) {
						LONGLONG createTime = GetFileCreateTime(p);
						folderList.emplace_back(p.path().wstring(), createTime);
					}
				}
				std::sort(folderList.begin(), folderList.end(),
					[](const std::pair<std::wstring, LONGLONG>& n1, const std::pair<std::wstring, LONGLONG>& n2) {
						return n1.second < n2.second;
					});
				while (Config::s_maxCacheFolderCount < folderList.size()) {
					INFO_LOG << L"キャッシュフォルダを削除しました : " << folderList.front().first;
					fs::remove_all(folderList.front().first);
					folderList.erase(folderList.begin());
				}
			}
		} catch (std::exception& e) {
			ERROR_LOG << L"Clear Cache failed: " << UTF16fromShiftJIS(e.what());
		}

		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path actualMediaPath = (rootFolder / mediaPath).make_preferred();
		if (!fs::is_regular_file(actualMediaPath)) {
			json jsonResponse;
			jsonResponse["Status"] = "failed";
			jsonResponse["Message"] = "file not found";
			SendJSON200OK(jsonResponse.dump(), yield, s);
			return;
		}

		// 作業フォルダ作成
		const fs::path segmentFolderPath = BuildWokringFolderPath(actualMediaPath);
		if (!fs::is_directory(segmentFolderPath)) {
			if (!fs::create_directory(segmentFolderPath)) {
				THROWEXCEPTION(L"create_directory(segmentFolderPath) failed");
			}
		}

		const fs::path playListPath = segmentFolderPath / L"a.m3u8";		
		if (!fs::exists(playListPath)) {	// プレイリストファイルが存在しなければセグメントファイルはまだ未生成
			auto enginePath = Config::GetEnginePath();
			if (!fs::exists(enginePath)) {
				ERROR_LOG << L"enginePath に実行ファイルが存在しません: " << enginePath.wstring();

				json jsonResponse;
				jsonResponse["Status"] = "failed";
				jsonResponse["Message"] = "enginePath not exists";
				SendJSON200OK(jsonResponse.dump(), yield, s);
				return;
			} else {
				std::wstring commandLine = Config::BuildVCEngineCommandLine(actualMediaPath, segmentFolderPath);
				std::thread([=]() {
					// エンコーダー起動
					UpdateVideoConvertCount(true);
					StartProcess(enginePath, commandLine);
					UpdateVideoConvertCount(false);
				}).detach();

				enum {
					kMaxRetryCount = 5
				};
				int retryCount = 0;
				while (!fs::exists(playListPath)) {
					++retryCount;
					if (kMaxRetryCount < retryCount) {
						ERROR_LOG << L"エンコードに失敗？ 'a.m3u8' が生成されていません";

						json jsonResponse;
						jsonResponse["Status"] = "failed";
						jsonResponse["Message"] = "encode failed";
						SendJSON200OK(jsonResponse.dump(), yield, s);
						return;	// failed
					}
					::Sleep(1000);
				}
			}
		}
		std::string playListURL = "/stream/";
		playListURL += ConvertUTF8fromUTF16(segmentFolderPath.filename().wstring()) + "/a.m3u8";
		json jsonResponse;
		jsonResponse["Status"] = "ok";
		jsonResponse["DirectPlay"] = false;
		jsonResponse["playListURL"] = playListURL;
		SendJSON200OK(jsonResponse.dump(), yield, s);		
	}

	void Send404NotFound(asio::yield_context& yield, tcp::socket& s)
	{
		std::stringstream	ss;
		ss << "HTTP/1.1 404 Not Found\r\n"
			<< "Connection: " << "close" << "\r\n"
			<< "\r\n";
		std::string header = ss.str();
		boost::system::error_code ec;
		asio::async_write(s, asio::buffer(header), yield[ec]);
		if (ec) {
			ERROR_LOG << L"Send404NotFound - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}
	}

	void RawLoadAPI(const std::wstring& mediaPath, boost::optional<std::pair<LONGLONG, LONGLONG>> optRange, asio::yield_context& yield, tcp::socket& s)
	{
		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path actualMediaPath = rootFolder / mediaPath;
		if (!fs::is_regular_file(actualMediaPath)) {
			Send404NotFound(yield, s);
			return;
		}
		if (optRange) {
			SendRangeFile206PertialContent(actualMediaPath, optRange.get(), yield, s);
		} else {
			SendFile200OK(actualMediaPath, yield, s);
		}
	}


	void CacheCheckAPI(const std::wstring& mediaPath, asio::yield_context& yield, tcp::socket& s)
	{
		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path actualMediaPath = rootFolder / mediaPath;

		const fs::path segmentFolderPath = BuildWokringFolderPath(actualMediaPath);
		const fs::path playListPath = segmentFolderPath / L"a.m3u8";

		bool cacheExist = fs::exists(playListPath);
		json jsonResponse;
		jsonResponse["CacheExist"] = cacheExist;
		SendJSON200OK(jsonResponse.dump(), yield, s);
	}

	void PlayHistoryAPI(asio::yield_context& yield, tcp::socket& s)
	{
		json jsonPlayHistory = json::array();
		for (const auto& playHisotory : g_playHistory) {
			jsonPlayHistory.push_back(ConvertUTF8fromUTF16(playHisotory));
		}
		json jsonResponse;
		jsonResponse["PlayHistory"] = jsonPlayHistory;
		SendJSON200OK(jsonResponse.dump(), yield, s);
	}

	void VolumeAPI(const std::string& operation, asio::yield_context& yield, tcp::socket& s)
	{
		json jsonResponse;
		jsonResponse["Operation"] = operation;
		if (operation == "toggle_mute") {
			bool bMute = ToggleMute();

			jsonResponse["NowMute"] = bMute;
			SendJSON200OK(jsonResponse.dump(), yield, s);

		} else if (operation == "volume_up" || operation == "volume_down") {
			bool bVolumeUp = operation == "volume_up";
			int currentVolume = StepVolumeChange(bVolumeUp);

			jsonResponse["CurrentVlume"] = currentVolume;
			SendJSON200OK(jsonResponse.dump(), yield, s);
		}
	}

	void PrevSleepAPI(asio::yield_context& yield, tcp::socket& s)
	{
		::SetThreadExecutionState(ES_SYSTEM_REQUIRED);
		json jsonResponse;
		jsonResponse["Status"] = "ok";
		SendJSON200OK(jsonResponse.dump(), yield, s);
	}

	void DefaultSortOrderAPI(asio::yield_context& yield, tcp::socket& s)
	{
		json jsonResponse;
		jsonResponse["DefaultSortOrder"] = Config::s_defaultSortOrder;
		SendJSON200OK(jsonResponse.dump(), yield, s);
	}

	std::string GetRealPath(const std::string& path)
	{
		std::string realPath;
		auto queryPos = path.find('?');
		if (queryPos != std::string::npos) {
			realPath = path.substr(0, queryPos);
		} else {
			realPath = path;
		}
		return realPath;
	}

	bool isCookiePasswordMatch(const std::string& httpHeader)
	{
		std::regex rxCookie("Cookie:.*password=([a-zA-Z0-9_-]+)", std::regex_constants::icase);
		std::smatch resultCookie;
		if (!std::regex_search(httpHeader, resultCookie, rxCookie)) {
			return false;	// Cookie が見つからない
		}

		std::string password = resultCookie[1].str();
		if (Config::s_password == password) {
			return true;	// パスワードが一致した！
		} else {
			return false;	// パスワードが異なる...
		}

	}

	void RedirectURL(const std::string& location, asio::yield_context& yield, tcp::socket& s)
	{
		INFO_LOG << L"RedirectURL: " << location;
		std::stringstream	ss;
		ss << "HTTP/1.1 307 Temporary Redirect\r\n"
			<< "location: " << location << "\r\n"
			<< "Connection: " << "close" << "\r\n"
			<< "\r\n";
		std::string header = ss.str();
		boost::system::error_code ec;
		asio::async_write(s, asio::buffer(header), yield[ec]);
		if (ec) {
			ERROR_LOG << L"SendFile200OK - async_write failed: " << UTF16fromShiftJIS(ec.message());
			return;
		}
	}

	bool AuthorizeProcess(const std::string& path, const std::string& httpHeader, asio::yield_context& yield, tcp::socket& s)
	{
		if (path.substr(0, 5) == "/CDN/") {
			return false;
		}

		std::string realPath = GetRealPath(path);
		if (isCookiePasswordMatch(httpHeader)) {
			if (realPath == "/login.html") {
				// ログイン成功処理 ?path=<Jump先> へ飛ばす
				std::string jumpPath = "/";
				std::regex rx(R"(path=(.*))");
				std::smatch result;
				if (std::regex_search(path, result, rx)) {
					jumpPath = result[1].str();
				}
				RedirectURL(jumpPath, yield, s);
				return true;
			} else {
				return false;	// ログイン済み 後に処理を渡す
			}
		} else {
			if (realPath != "/login.html" && realPath != "/login.js") {
				// ログインしていないので、ログインページへ飛ばす
				std::string location = "/login.html?path=" + path;
				RedirectURL(location, yield, s);
				return true;
			} else {
				// /login.html or /login.js - ログインページを返す
				static const fs::path htmlFolderPath = GetExeDirectory() / L"html";
				auto indexPath = htmlFolderPath / realPath.substr(1);
				SendFile200OK(indexPath, yield, s);
				return true;
			}
		}
	}


}	// namespace 

std::shared_ptr<HttpServer> HttpServer::RunHttpServer()
{
	auto server = std::shared_ptr<HttpServer>(new HttpServer);
	return server;
}

void HttpServer::StopHttpServer()
{
	m_ioService.stop();
	m_serverThread.join();
}

void connection_rountine(asio::yield_context& yield, tcp::socket s)
{
	ATLTRACE("enter connection_rountine\n");
	UpdateRequestCount(true);
	::SetThreadExecutionState(ES_SYSTEM_REQUIRED);

	static const fs::path htmlFolderPath = GetExeDirectory() / L"html";

	try {
		for (;;) {
			boost::system::error_code ec;
			asio::streambuf buf;
			std::string str;

			auto const n = asio::async_read_until(s, buf, "\r\n\r\n", yield[ec]);
			//auto const n = asio::async_read(s, buf, asio::transfer_at_least(200), yield[ec]);
			if (ec) break;

			// ヘッダ読み込み
			str.assign(asio::buffer_cast<const char*>(buf.data()), n);
			buf.consume(n);

			std::regex rx("^GET ([^ ]+)");
			std::smatch result;
			if (std::regex_search(str, result, rx)) {
				std::string path = result[1].str();
				INFO_LOG << L"GET " << ConvertUTF16fromUTF8(URLDecode(path));

				// ====================================================
				// パスワード確認
				if (AuthorizeProcess(path, str, yield, s)) {
					break;
				}

				std::string realPath = GetRealPath(path);
				if (realPath == "/") {
					auto indexPath = htmlFolderPath / "index.html";
					SendFile200OK(indexPath, yield, s);
					break;

				// ====================================================
				// /root/
				} else if (path.substr(0, 6) == "/root/") {
					auto templatePath = GetExeDirectory() / L"html" / L"root" / L"root_template.html";
					SendFile200OK(templatePath, yield, s);
					break;
				} else if (path.substr(0, 12) == "/filetreeAPI") {
					auto queryData = ConvertUTF16fromUTF8(URLDecode(path.substr(12)));
					std::wregex rx(LR"(\?path=\/root\/(.*)&sort=(\w+)&order=(\w+))");
					std::wsmatch result;
					if (std::regex_match(queryData, result, rx)) {						
						auto searchFolderPath = result[1].str();
						boost::replace_all(searchFolderPath, L"<hash>", L"#");
						auto sort = result[2].str();
						auto order = result[3].str();
						FileTreeAPI(searchFolderPath, sort, order, yield, s);
					}
					break;

				// ====================================================
				// /play/
				} else if (path.substr(0, 6) == "/play/") {
					auto templatePath = GetExeDirectory() / L"html" / L"play" / L"play_template.html";
					SendFile200OK(templatePath, yield, s);
					break;
				} else if (path.substr(0, 14) == "/CacheCheckAPI") {
					auto queryData = ConvertUTF16fromUTF8(URLDecode(path.substr(14)));
					std::wregex rx(LR"(\?path=\/play\/(.*))");
					std::wsmatch result;
					if (std::regex_match(queryData, result, rx)) {
						auto mediaPath = result[1].str();
						boost::replace_all(mediaPath, L"<hash>", L"#");
						CacheCheckAPI(mediaPath, yield, s);
					}
					break;
				} else if (path.substr(0, 14) == "/PrepareHLSAPI") {
					auto queryData = ConvertUTF16fromUTF8(URLDecode(path.substr(14)));
					std::wregex rx(LR"(\?path=\/play\/(.*))");
					std::wsmatch result;
					if (std::regex_match(queryData, result, rx)) {
						auto mediaPath = result[1].str();
						boost::replace_all(mediaPath, L"<hash>", L"#");
						PrepareHLSAPI(mediaPath, yield, s);
					}
					break;
				} else if (path.substr(0, 11) == "/RawLoadAPI") {
					auto queryData = ConvertUTF16fromUTF8(URLDecode(path.substr(11)));
					std::wregex rx(LR"(\?path=(.*))");
					std::wsmatch result;
					if (std::regex_match(queryData, result, rx)) {
						auto mediaPath = result[1].str();
						boost::replace_all(mediaPath, L"<hash>", L"#");

						boost::optional<std::pair<LONGLONG, LONGLONG>> optRange;
						std::regex rxRange(R"(Range: bytes=(\d+)-(\d+)?)");
						std::smatch resultRange;
						if (std::regex_search(str, resultRange, rxRange)) {
							LONGLONG startPos = std::stoll(resultRange[1].str());
							LONGLONG endPos = -1;
							if (resultRange[2].matched) {
								endPos = std::stoll(resultRange[2].str());
							}
							optRange.emplace(startPos, endPos);
						}
						RawLoadAPI(mediaPath, optRange, yield, s);
					}
					break;
				// ====================================================
				// /index.html
				} else if (path == "/PlayHistoryAPI") {
					PlayHistoryAPI(yield, s);
					break;
				} else if (path.substr(0, 10) == "/VolumeAPI") {
					auto queryData = path.substr(10);
					std::regex rx(R"(\?operation=(.*))");
					std::smatch result;
					if (std::regex_match(queryData, result, rx)) {
						std::string operation = result[1].str();
						VolumeAPI(operation, yield, s);
					}
					break;
				} else if (path == "/PrevSleepAPI") {
					PrevSleepAPI(yield, s);
					break;
				} else if (path == "/DefaultSortOrderAPI") {
					DefaultSortOrderAPI(yield, s);
					break;
				}

				// =====================================================
				// htmlフォルダ以下の その他ファイルへのリクエスト
				auto actualPath = htmlFolderPath / realPath.substr(1);
				if (fs::is_regular_file(actualPath)) {
					SendFile200OK(actualPath, yield, s);
					break;
				}
			}

#if 0
			std::smatch result;
			if (std::regex_search(str, result, std::regex("Content-Length: (\\d+)")) == false) {
				throw std::runtime_error("no content-length");
			}
			int contentLength = std::stoi(result[1].str());

			// ボディ読み込み
			auto const n1 = asio::async_read_until(s, buf, "\n", yield[ec]);
			std::string strstat(asio::buffer_cast<const char*>(buf.data()), n1);
			buf.consume(n1);

			auto const n2 = asio::async_read_until(s, buf, "\n", yield[ec]);
			std::string strmsg(asio::buffer_cast<const char*>(buf.data()), n2);
			buf.consume(n2);

			int state = std::stoi(strstat);
			std::wstring msg = ConvertUTF16fromUTF8(strmsg);
#endif
			// レスポンス送信
			asio::async_write(s, asio::buffer("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\nunknown operation"), yield[ec]);
			if (ec) break;

			s.close();
			break;
		}
	}
	catch (std::exception& e) {
		//ATLASSERT(FALSE);
		ATLTRACE("error : %s\n", e.what());
		ERROR_LOG << L"connection_rountine - exception throw: " << UTF16fromShiftJIS(e.what());
	}

	UpdateRequestCount(false);
	ATLTRACE("leave connection_rountine\n");
}

HttpServer::HttpServer()
{
	m_serverThread = std::thread([this] {
		INFO_LOG << (L"HttpServer start!") << L" port: " << Config::s_httpServerPort;

		asio::spawn(m_ioService, [&](asio::yield_context yield) {
			tcp::acceptor acceptor(m_ioService, tcp::endpoint(tcp::v4(), Config::s_httpServerPort/*kDefaultHttpServerPort*/));
			for (;;) {
				tcp::socket socket(m_ioService);
				acceptor.async_accept(socket, yield);

				asio::spawn(m_ioService, [&](asio::yield_context yc) {
					connection_rountine(yc, std::move(socket));
				});
			}
		});

		m_ioService.run();	// このスレッドで実行する

		INFO_LOG << (L"HttpServer finish!");
	});
}

