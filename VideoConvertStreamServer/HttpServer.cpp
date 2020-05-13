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
			auto ext = filePath.extension().string();
			boost::to_lower(ext);
			for (const auto& mediaExt : kMediaExt) {
				if (ext == mediaExt) {
					return true;
				}
			}
		}
		return false;
	}

	bool	isDirectPlayFile(const fs::path& filePath)
	{
		auto ext = filePath.extension().string();
		boost::to_lower(ext);
		for (const auto& mediaExt : kDirectPlayMediaExt) {
			if (ext == mediaExt) {
				return true;
			}
		}
		return false;		
	}

	fs::path BuildWokringFolderPath(const fs::path& actualMediaPath)
	{
		enum { kMaxTitleCount = 32 };
		std::hash<std::wstring> strhash;
		wchar_t workingFolderName[128] = L"";	// 最大でも "32 + 1 + 16 = 49" なので大丈夫
		const std::wstring shortFileName = actualMediaPath.stem().wstring().substr(0, kMaxTitleCount);
		//swprintf_s(workingFolderName, L"%s_%zx", shortFileName.c_str(), strhash(actualMediaPath.wstring()));
		swprintf_s(workingFolderName, L"%zx", strhash(actualMediaPath.wstring()));
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

	int ToggleMute()
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

		return 0;
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
			volLevel += 0.1;
			if (volLevel > 1.0) {
				volLevel = 1.0;
			}
		} else {
			volLevel -= 0.1;
			if (volLevel < 0) {
				volLevel = 0.0;
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

		return 0;
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
		jsonFolder["FileList"] = jsonFileList;

		SendJSON200OK(jsonFolder.dump(), yield, s);
	}

	void PrepareHLSAPI(const std::wstring& mediaPath, asio::yield_context& yield, tcp::socket& s)
	{
		if (isDirectPlayFile(mediaPath)) {
			auto playListURL = boost::replace_all_copy(mediaPath, L"#", L"<hash>");
			json jsonResponse;
			jsonResponse["DirectPlay"] = true;
			jsonResponse["playListURL"] = "/RawLoadAPI?path=" + ConvertUTF8fromUTF16(playListURL);
			SendJSON200OK(jsonResponse.dump(), yield, s);
			return;
		}

		try {	// 古いキャッシュフォルダを削除
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
		} catch (std::exception& e) {
			ERROR_LOG << L"Clear Cache failed: " << UTF16fromShiftJIS(e.what());
		}

		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path actualMediaPath = (rootFolder / mediaPath).make_preferred();

		const fs::path hlsPath = Config::s_NVEncCPath;//LR"(C:\#app\tv\#enc\NVEnc_5.01\NVEncC\x64\NVEncC64.exe)";
		std::wstring hlsParam = Config::s_hlsParam;

		std::string ext = boost::to_lower_copy(actualMediaPath.extension().string());
		if (ext == ".ts") {	// .ts ファイルは、デインターレース用のパラメーターを追加する
			boost::replace_all(hlsParam, L"<DeinterlaceParam>", Config::s_extra_DeinterlaceParam);
		} else {
			boost::replace_all(hlsParam, L"<DeinterlaceParam>", L"");
		}
			//LR"( --avhw -i "<input>" -o "<segmentFolder>\a.m3u8" -f hls -m hls_time:5 -m hls_list_size:0 -m hls_segment_filename:"<segmentFolder>\segment_%08d.ts" --gop-len 30 --interlace tff --vpp-deinterlace normal --audio-codec aac --audio-samplerate 48000 --audio-bitrate 192)";
		boost::replace_all(hlsParam, L"<input>", actualMediaPath.wstring().c_str());

		// 作業フォルダ作成
		const fs::path segmentFolderPath = BuildWokringFolderPath(actualMediaPath);
		if (!fs::is_directory(segmentFolderPath)) {
			if (!fs::create_directory(segmentFolderPath)) {
				THROWEXCEPTION(L"create_directory(segmentFolderPath) failed");
			}
		}
		const fs::path playListPath = segmentFolderPath / L"a.m3u8";
		if (!fs::exists(playListPath)) {	
			// プレイリストファイルが存在しなければセグメントファイルはまだ未生成
			boost::replace_all(hlsParam, L"<segmentFolder>", segmentFolderPath.wstring().c_str());
			std::thread([=]() {
				// エンコーダー起動！
				StartProcess(hlsPath, hlsParam);
			}).detach();

			enum {
				kMaxRetryCount = 5
			};
			int retryCount = 0;
			while (!fs::exists(playListPath)) {
				++retryCount;
				if (kMaxRetryCount < retryCount) {
					ERROR_LOG << L"エンコードに失敗？ 'a.m3u8' が生成されていません";
					break;	// failed
				}
				::Sleep(1000);
			}
		}
		std::string playListURL = "/stream/";
		playListURL += ConvertUTF8fromUTF16(segmentFolderPath.filename().wstring()) + "/a.m3u8";
		json jsonResponse;
		jsonResponse["DirectPlay"] = false;
		jsonResponse["playListURL"] = playListURL;
		SendJSON200OK(jsonResponse.dump(), yield, s);		
	}

	void RawLoadAPI(const std::wstring& mediaPath, boost::optional<std::pair<LONGLONG, LONGLONG>> optRange, asio::yield_context& yield, tcp::socket& s)
	{
		const fs::path rootFolder = Config::s_rootFolder;//LR"(G:\Videos)";
		const fs::path actualMediaPath = rootFolder / mediaPath;
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

void connection_rountine(asio::yield_context& yield, tcp::socket s, std::function<void(int, std::wstring)> funcCallback)
{
	ATLTRACE("enter connection_rountine\n");

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
				if (path == "/") {
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
				}

				auto actualPath = htmlFolderPath / path.substr(1);
				if (fs::is_regular_file(actualPath)) {
					SendFile200OK(actualPath, yield, s);
					break;
				}
	
				
				if (path == "/prev_sleep") {
					int state = 0;
					std::wstring msg;
					funcCallback(state, msg);

					// レスポンス送信
					asio::async_write(s, asio::buffer("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"), yield[ec]);
					if (ec) break;
					asio::async_write(s, asio::buffer("prev_seep ok"), yield[ec]);
					if (ec) break;

					s.close();
					break;
				} else if (path == "/toggle_mute") {
					ToggleMute();

					// レスポンス送信
					asio::async_write(s, asio::buffer("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"), yield[ec]);
					if (ec) break;
					asio::async_write(s, asio::buffer("toggle_mute ok"), yield[ec]);
					if (ec) break;

					s.close();
					break;
				} else if (path == "/VolumeUp" || path == "/VolumeDown") {
					bool bVolumeUp = path == "/VolumeUp";
					StepVolumeChange(bVolumeUp);

					// レスポンス送信
					asio::async_write(s, asio::buffer("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"), yield[ec]);
					if (ec) break;
					asio::async_write(s, asio::buffer("volume change ok"), yield[ec]);
					if (ec) break;

					s.close();
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
					connection_rountine(yc, std::move(socket), m_funcCallback);
				});
			}
		});

		m_ioService.run();	// このスレッドで実行する

		INFO_LOG << (L"HttpServer finish!");
	});
}

