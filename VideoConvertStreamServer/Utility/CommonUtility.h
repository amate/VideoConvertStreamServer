
// CommonUtility.h

#pragma once

#include <unordered_map>
#include <Windows.h>
#include <boost\optional.hpp>
#include <boost\filesystem.hpp>

#include <Psapi.h>
#include <atlmisc.h>

namespace fs = boost::filesystem;

/// 現在実行中の exeのあるフォルダのパスを返す
fs::path GetExeDirectory();

/// 例外を発生させる
#define THROWEXCEPTION(error)	FatalErrorOccur(error, __FILE__,__LINE__)

void	FatalErrorOccur(const std::string& error, const char* fileName, const int line);
void	FatalErrorOccur(const std::wstring& error, const char* fileName, const int line);

/// プロセスを起動し、終了まで待つ
DWORD	StartProcess(const fs::path& exePath, const std::wstring& commandLine);

DWORD	StartProcessGetStdOut(const fs::path& exePath, const std::wstring& commandLine, std::string& stdoutText);

fs::path	SearchFirstFile(const fs::path& search);

std::string	LoadFile(const fs::path& filePath);
void		SaveFile(const fs::path& filePath, const std::string& content);



inline bool GetExeFileName(HWND hWnd, CString& exeName)
{
	// プロセスID
	DWORD processID = NULL;
	GetWindowThreadProcessId(hWnd, &processID);

	// プロセスハンドル
	HANDLE hProcess = OpenProcess(
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (!hProcess)
		return false;

	// モジュールハンドル
	HMODULE hModule = NULL;
	DWORD dummy = 0;
	if (!EnumProcessModules(hProcess, &hModule, sizeof(HMODULE), &dummy))
		return false;

	// ファイル名
	if (!GetModuleBaseName(hProcess, hModule, exeName.GetBuffer(MAX_PATH), MAX_PATH)) {
		exeName.ReleaseBuffer();
		return false;
	}
	exeName.ReleaseBuffer();
	exeName.MakeLower();

	CloseHandle(hProcess);
	// hModuleは自分で閉じちゃいけない

	return true;
}






