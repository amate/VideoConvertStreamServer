
// CommonUtility.h

#pragma once

#include <unordered_map>
#include <Windows.h>
#include <boost\optional.hpp>
#include <boost\filesystem.hpp>

#include <Psapi.h>
#include <atlmisc.h>

namespace fs = boost::filesystem;

/// ���ݎ��s���� exe�̂���t�H���_�̃p�X��Ԃ�
fs::path GetExeDirectory();

/// ��O�𔭐�������
#define THROWEXCEPTION(error)	FatalErrorOccur(error, __FILE__,__LINE__)

void	FatalErrorOccur(const std::string& error, const char* fileName, const int line);
void	FatalErrorOccur(const std::wstring& error, const char* fileName, const int line);

/// �v���Z�X���N�����A�I���܂ő҂�
DWORD	StartProcess(const fs::path& exePath, const std::wstring& commandLine);

DWORD	StartProcessGetStdOut(const fs::path& exePath, const std::wstring& commandLine, std::string& stdoutText);

fs::path	SearchFirstFile(const fs::path& search);

std::string	LoadFile(const fs::path& filePath);
void		SaveFile(const fs::path& filePath, const std::string& content);



inline bool GetExeFileName(HWND hWnd, CString& exeName)
{
	// �v���Z�XID
	DWORD processID = NULL;
	GetWindowThreadProcessId(hWnd, &processID);

	// �v���Z�X�n���h��
	HANDLE hProcess = OpenProcess(
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
	if (!hProcess)
		return false;

	// ���W���[���n���h��
	HMODULE hModule = NULL;
	DWORD dummy = 0;
	if (!EnumProcessModules(hProcess, &hModule, sizeof(HMODULE), &dummy))
		return false;

	// �t�@�C����
	if (!GetModuleBaseName(hProcess, hModule, exeName.GetBuffer(MAX_PATH), MAX_PATH)) {
		exeName.ReleaseBuffer();
		return false;
	}
	exeName.ReleaseBuffer();
	exeName.MakeLower();

	CloseHandle(hProcess);
	// hModule�͎����ŕ����Ⴂ���Ȃ�

	return true;
}






