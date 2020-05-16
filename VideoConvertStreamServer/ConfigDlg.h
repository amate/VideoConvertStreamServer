#pragma once

#include <atlctrls.h>
#include <atlddx.h>

#include "Config.h"
#include "resource.h"

class ConfigDlg : 
	public CDialogImpl<ConfigDlg>,
	public CWinDataExchange<ConfigDlg>
{
public:
	enum { IDD = IDD_CONFIG };


	BEGIN_DDX_MAP(ConfigDlg)
		DDX_INT_RANGE(IDC_EDIT_HTTPSERVERPORT, m_httpServerPort, 1024, 65535)
		DDX_TEXT(IDC_EDIT_ROOTFOLDER, m_rootFolder)
		DDX_TEXT(IDC_EDIT_PASSWORD, m_password)
		DDX_TEXT(IDC_EDIT_MEDIAEXTLIST, m_mediaExtList)
		DDX_TEXT(IDC_EDIT_DIRECTPLAYMEDIAEXTLIST, m_directPlayMediaExtList)
		DDX_INT_RANGE(IDC_EDIT_MAXCACHEFOLDERCOUNT, m_maxCacheFolderCount, 2, INT_MAX)
		DDX_COMBO_INDEX(IDC_COMBO_CONVERTENGINE, m_videoConvertEngine)

		DDX_TEXT(IDC_EDIT_BUILTIN_FFMPEG_COMMANDLINE, m_arrVCInfo[Config::kBuiltinFFmpeg].commandLine)

		DDX_TEXT(IDC_EDIT_EXTERNAL_FFMPEGPATH, m_arrVCInfo[Config::kExternalFFmpeg].enginePath)
		DDX_TEXT(IDC_EDIT_EXTERNAL_FFMPEG_COMMANDLINE, m_arrVCInfo[Config::kExternalFFmpeg].commandLine)

		DDX_TEXT(IDC_EDIT_NVENCCPATH, m_arrVCInfo[Config::kNVEncC].enginePath)
		DDX_TEXT(IDC_EDIT_NVENCC_COMMANDLINE, m_arrVCInfo[Config::kNVEncC].commandLine)
	END_DDX_MAP()

	BEGIN_MSG_MAP(ConfigDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_ID_HANDLER(ID_LOAD_DEFAULT, OnLoadDefault)

		COMMAND_ID_HANDLER(IDC_BUTTON_FOLDERSELECT, OnRootFolderSelect)
		COMMAND_ID_HANDLER(IDC_BUTTON_FILESELECT_EXTERNALFFMPEG, OnFileSelect)
		COMMAND_ID_HANDLER(IDC_BUTTON_FILESELECT_NVENCC, OnFileSelect)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnLoadDefault(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnRootFolderSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnFileSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	int		m_httpServerPort;
	CString	m_rootFolder;
	CString m_password;
	CString m_mediaExtList;
	CString m_directPlayMediaExtList;
	int		m_maxCacheFolderCount;
	int		m_videoConvertEngine;

	struct VCInfo {
		CString enginePath;
		CString commandLine;
		CString deinterlaceParam;

		void Convert(const Config::VideoConvertEngineInfo& vcei)
		{
			enginePath = vcei.enginePath.c_str();
			commandLine = vcei.commandLine.c_str();
			deinterlaceParam = vcei.deinterlaceParam.c_str();
		}
	};
	std::array<VCInfo, Config::kMaxEngineNum>	m_arrVCInfo;
};