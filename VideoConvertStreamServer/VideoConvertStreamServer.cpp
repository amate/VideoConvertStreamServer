// VideoConvertStreamServer.cpp : main source file for VideoConvertStreamServer.exe
//

#include "stdafx.h"

#include "MainDlg.h"

#include "Utility\CommonUtility.h"
#include "HttpServer.h"
#include "Config.h"

CAppModule _Module;

std::string	LogFileName()
{
	return (GetExeDirectory() / L"html" / L"test.log").string();
}

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainDlg dlgMain;

	if(dlgMain.Create(NULL) == NULL)
	{
		ATLTRACE(_T("Main dialog creation failed!\n"));
		return 0;
	}

	//dlgMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	Config::LoadConfig();
	auto httpServer = HttpServer::RunHttpServer();

	int nRet = Run(lpstrCmdLine, nCmdShow);

	httpServer->StopHttpServer();

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
