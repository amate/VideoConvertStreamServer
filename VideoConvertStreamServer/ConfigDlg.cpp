#include "stdafx.h"
#include "ConfigDlg.h"
#include <atldlgs.h>
#include <boost\filesystem.hpp>
#include <boost\algorithm\string.hpp>

namespace fs = boost::filesystem;


LRESULT ConfigDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CenterWindow(GetParent());

	m_httpServerPort = Config::s_httpServerPort;
	m_rootFolder = Config::s_rootFolder.c_str();

	for (auto& mediaExt : Config::s_mediaExtList) {
		m_mediaExtList += mediaExt.c_str();
		m_mediaExtList += L",";
	}
	if (m_mediaExtList.Right(1) == L",") {
		m_mediaExtList.Delete(m_mediaExtList.GetLength() - 1);
	}

	for (auto& mediaExt : Config::s_directPlayMediaExtList) {
		m_directPlayMediaExtList += mediaExt.c_str();
		m_directPlayMediaExtList += L",";
	}
	if (m_directPlayMediaExtList.Right(1) == L",") {
		m_directPlayMediaExtList.Delete(m_directPlayMediaExtList.GetLength() - 1);
	}

	m_maxCacheFolderCount = Config::s_maxCacheFolderCount;
	m_videoConvertEngine = Config::s_videoConvertEngine;

	CComboBox cmbVEC = GetDlgItem(IDC_COMBO_CONVERTENGINE);
	for (int i = 0; i < Config::kMaxEngineNum; ++i) {
		m_arrVCInfo[i].Convert(Config::s_arrVideoConvertEngineInfo[i]);
		m_arrVCInfo[i].commandLine = 
			boost::replace_all_copy(Config::s_arrVideoConvertEngineInfo[i].commandLine, L"\\n", L"\r\n").c_str();

		cmbVEC.AddString(Config::s_videoConvertEngineName[i].c_str());	
	}

	DoDataExchange(DDX_LOAD);	// variables -> Dialog
	return TRUE;
}

LRESULT ConfigDlg::OnOk(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(DDX_SAVE);	// Dialog -> variables

	if (!fs::is_directory((LPCWSTR)m_rootFolder)) {
		MessageBox(L"ルートフォルダが存在しません。", L"エラー");
		return 0;
	}

	if (!fs::is_regular_file((LPCWSTR)m_arrVCInfo[m_videoConvertEngine].enginePath)) {
		MessageBox(L"利用するエンジンのパスに、実行ファイルが存在しません。", L"エラー");
		return 0;
	}

	Config::s_httpServerPort = m_httpServerPort;
	Config::s_rootFolder = (LPCWSTR)m_rootFolder;

	Config::s_mediaExtList.clear();
	boost::split(Config::s_mediaExtList, (LPCWSTR)m_mediaExtList, boost::is_any_of(L","), boost::algorithm::token_compress_on);

	Config::s_directPlayMediaExtList.clear();
	boost::split(Config::s_directPlayMediaExtList, (LPCWSTR)m_directPlayMediaExtList, boost::is_any_of(L","), boost::algorithm::token_compress_on);

	Config::s_maxCacheFolderCount = m_maxCacheFolderCount;
	Config::s_videoConvertEngine = m_videoConvertEngine;

	for (int i = 0; i < Config::kMaxEngineNum; ++i) {
		auto& VCEInfo = Config::s_arrVideoConvertEngineInfo[i];
		VCEInfo.enginePath = m_arrVCInfo[i].enginePath;

		std::wstring commandLine = (LPCWSTR)m_arrVCInfo[i].commandLine;
		boost::replace_all(commandLine, L"\r\n", L"\\n");
		m_arrVCInfo[i].commandLine = commandLine.c_str();
		VCEInfo.commandLine = m_arrVCInfo[i].commandLine;

		VCEInfo.deinterlaceParam = m_arrVCInfo[i].deinterlaceParam;
	}
	Config::SaveConfig();

	EndDialog(wID);
	return 0;
}

LRESULT ConfigDlg::OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT ConfigDlg::OnLoadDefault(WORD, WORD wID, HWND, BOOL&)
{
	int commandLineEditID[] = {
		IDC_EDIT_BUILTIN_FFMPEG_COMMANDLINE, 
		IDC_EDIT_EXTERNAL_FFMPEG_COMMANDLINE,
		IDC_EDIT_NVENCC_COMMANDLINE,
	};
	for (int i = 0; i < Config::kMaxEngineNum; ++i) {
		auto& VCEInfo = Config::s_arrVideoConvertEngineInfo[i];
		m_arrVCInfo[i].commandLine = boost::replace_all_copy(VCEInfo.defaultCommandLine, L"\\n", L"\r\n").c_str();	

		DoDataExchange(DDX_LOAD, commandLineEditID[i]);
	}
	return 0;
}

LRESULT ConfigDlg::OnRootFolderSelect(WORD, WORD wID, HWND, BOOL&)
{
	CShellFileOpenDialog dlg(NULL, FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST | FOS_PICKFOLDERS);
	if (dlg.DoModal(m_hWnd) == IDOK) {
		CString rootFolder;
		dlg.GetFilePath(rootFolder);
		m_rootFolder = (LPCWSTR)rootFolder;
		DoDataExchange(DDX_LOAD, IDC_EDIT_ROOTFOLDER);
	}

	return 0;
}

LRESULT ConfigDlg::OnFileSelect(WORD, WORD wID, HWND, BOOL&)
{
	CString defaultFileName;
	CString* enginePath = nullptr;
	int EditCtrlID = 0;
	switch (wID) {
	case IDC_BUTTON_FILESELECT_EXTERNALFFMPEG:
		defaultFileName = L"ffmpeg.exe";
		enginePath = &m_arrVCInfo[Config::kExternalFFmpeg].enginePath;
		EditCtrlID = IDC_EDIT_EXTERNAL_FFMPEGPATH;
		break;

	case IDC_BUTTON_FILESELECT_NVENCC:
		defaultFileName = L"NVEncC64.exe";
		enginePath = &m_arrVCInfo[Config::kNVEncC].enginePath;
		EditCtrlID = IDC_EDIT_NVENCCPATH;
		break;

	default:
		ATLASSERT(FALSE);
		return 0;
		break;
	}

	COMDLG_FILTERSPEC filter[] = {
	{ L"実行ファイル (*.exe)", L"*.exe" },
	{ L"すべてのファイル (*.*)", L"*.*" }
	};
	CShellFileOpenDialog dlg(defaultFileName,
		FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST,
		NULL, filter, 2);
	INT_PTR ret = dlg.DoModal(m_hWnd);
	if (ret == IDOK) {
		CString exePath;
		dlg.GetFilePath(exePath);
		*enginePath = exePath;
		DoDataExchange(DDX_LOAD, EditCtrlID);
	}

	return 0;
}
