
#include "stdafx.h"
#include "MainDlg.h"
#include <fstream>
#include <atomic>
#include <iphlpapi.h>
#include <ip2string.h>

#include "aboutdlg.h"
#include "ConfigDlg.h"

//#include <boost\format.hpp>

#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\Logger.h"

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "Ntdll.lib")

using nlohmann::json;


#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

//
CWindow	g_hwndMainDlg;
std::atomic<int>	g_activeRequestCount = 0;
std::atomic<int>	g_videoConvertCount = 0;

std::wstring GetLocalIPAddress()
{
    /* Declare and initialize variables */
    std::wstring localIPAddress;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    unsigned int i = 0;

    // Set the flags to pass to GetAdaptersAddresses
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

    // default to unspecified address family (both)
    ULONG family = AF_UNSPEC;

    LPVOID lpMsgBuf = NULL;

    PIP_ADAPTER_ADDRESSES pAddresses = NULL;
    ULONG outBufLen = 0;
    ULONG Iterations = 0;

    PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
    PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
    PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
    IP_ADAPTER_DNS_SERVER_ADDRESS* pDnServer = NULL;
    IP_ADAPTER_PREFIX* pPrefix = NULL;

    //if (argc != 2) {
    //    printf(" Usage: getadapteraddresses family\n");
    //    printf("        getadapteraddresses 4 (for IPv4)\n");
    //    printf("        getadapteraddresses 6 (for IPv6)\n");
    //    printf("        getadapteraddresses A (for both IPv4 and IPv6)\n");
    //    exit(1);
    //}

    //if (atoi(argv[1]) == 4)
    family = AF_INET;
    //else if (atoi(argv[1]) == 6)
    //    family = AF_INET6;

    printf("Calling GetAdaptersAddresses function with family = ");
    if (family == AF_INET)
        printf("AF_INET\n");
    if (family == AF_INET6)
        printf("AF_INET6\n");
    if (family == AF_UNSPEC)
        printf("AF_UNSPEC\n\n");

    // Allocate a 15 KB buffer to start with.
    outBufLen = WORKING_BUFFER_SIZE;

    do {

        pAddresses = (IP_ADAPTER_ADDRESSES*)MALLOC(outBufLen);
        if (pAddresses == NULL) {
            printf
            ("Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
            //exit(1);
            return L"";
        }

        dwRetVal =
            GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            FREE(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }

        Iterations++;

    } while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

    if (dwRetVal == NO_ERROR) {
        // If successful, output some information from the data we received
        pCurrAddresses = pAddresses;
        while (pCurrAddresses) {
            printf("\tLength of the IP_ADAPTER_ADDRESS struct: %ld\n",
                pCurrAddresses->Length);
            printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
            printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);

            pUnicast = pCurrAddresses->FirstUnicastAddress;
            if (pUnicast != NULL) {
                WCHAR  szAddr[32];
                auto sockAddr = pUnicast->Address.lpSockaddr;
                RtlIpv4AddressToStringW(&((struct sockaddr_in*)sockAddr)->sin_addr, szAddr);
                localIPAddress = szAddr;
                break;  // success!
                //printf("\tUnicast Address: %s\n", szAddr);
                //inet_ntop(AF_INET, &((struct sockaddr_in*)sockAddr)->sin_addr, szAddr, sizeof(szAddr));

                for (i = 0; pUnicast != NULL; i++)
                    pUnicast = pUnicast->Next;
                printf("\tNumber of Unicast Addresses: %d\n", i);
            } else
                printf("\tNo Unicast Addresses\n");

            pAnycast = pCurrAddresses->FirstAnycastAddress;
            if (pAnycast) {
                for (i = 0; pAnycast != NULL; i++)
                    pAnycast = pAnycast->Next;
                printf("\tNumber of Anycast Addresses: %d\n", i);
            } else
                printf("\tNo Anycast Addresses\n");

            pMulticast = pCurrAddresses->FirstMulticastAddress;
            if (pMulticast) {
                for (i = 0; pMulticast != NULL; i++)
                    pMulticast = pMulticast->Next;
                printf("\tNumber of Multicast Addresses: %d\n", i);
            } else
                printf("\tNo Multicast Addresses\n");

            pDnServer = pCurrAddresses->FirstDnsServerAddress;
            if (pDnServer) {
                for (i = 0; pDnServer != NULL; i++)
                    pDnServer = pDnServer->Next;
                printf("\tNumber of DNS Server Addresses: %d\n", i);
            } else
                printf("\tNo DNS Server Addresses\n");

            printf("\tDNS Suffix: %wS\n", pCurrAddresses->DnsSuffix);
            printf("\tDescription: %wS\n", pCurrAddresses->Description);
            printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);

            if (pCurrAddresses->PhysicalAddressLength != 0) {
                printf("\tPhysical address: ");
                for (i = 0; i < (int)pCurrAddresses->PhysicalAddressLength;
                    i++) {
                    if (i == (pCurrAddresses->PhysicalAddressLength - 1))
                        printf("%.2X\n",
                            (int)pCurrAddresses->PhysicalAddress[i]);
                    else
                        printf("%.2X-",
                            (int)pCurrAddresses->PhysicalAddress[i]);
                }
            }
            printf("\tFlags: %ld\n", pCurrAddresses->Flags);
            printf("\tMtu: %lu\n", pCurrAddresses->Mtu);
            printf("\tIfType: %ld\n", pCurrAddresses->IfType);
            printf("\tOperStatus: %ld\n", pCurrAddresses->OperStatus);
            printf("\tIpv6IfIndex (IPv6 interface): %u\n",
                pCurrAddresses->Ipv6IfIndex);
            printf("\tZoneIndices (hex): ");
            for (i = 0; i < 16; i++)
                printf("%lx ", pCurrAddresses->ZoneIndices[i]);
            printf("\n");

            printf("\tTransmit link speed: %I64u\n", pCurrAddresses->TransmitLinkSpeed);
            printf("\tReceive link speed: %I64u\n", pCurrAddresses->ReceiveLinkSpeed);

            pPrefix = pCurrAddresses->FirstPrefix;
            if (pPrefix) {
                for (i = 0; pPrefix != NULL; i++)
                    pPrefix = pPrefix->Next;
                printf("\tNumber of IP Adapter Prefix entries: %d\n", i);
            } else
                printf("\tNumber of IP Adapter Prefix entries: 0\n");

            printf("\n");

            pCurrAddresses = pCurrAddresses->Next;
        }
    } else {
        printf("Call to GetAdaptersAddresses failed with error: %d\n",
            dwRetVal);
        if (dwRetVal == ERROR_NO_DATA)
            printf("\tNo addresses were found for the requested parameters\n");
        else {
#if 0
            if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                // Default language
                (LPTSTR)&lpMsgBuf, 0, NULL)) {
                printf("\tError: %s", (LPTSTR)lpMsgBuf);
                LocalFree(lpMsgBuf);
                if (pAddresses)
                    FREE(pAddresses);
                //exit(1);
                return L"";
            }
#endif
        }
    }

    if (pAddresses) {
        FREE(pAddresses);
    }
    return localIPAddress;
}

void UpdateRequestCount(bool increment)
{
    if (increment) {
        ++g_activeRequestCount;
    } else {
        --g_activeRequestCount;
    }
	CString text;
	text.Format(L"アクティブなリクエスト数: %02d", g_activeRequestCount);

	CWindow edit = g_hwndMainDlg.GetDlgItem(IDC_STATIC_ACTIVEREQUEST);
	edit.SetWindowTextW(text);
}

void UpdateVideoConvertCount(bool increment)
{
    if (increment) {
        ++g_videoConvertCount;
    } else {
        --g_videoConvertCount;
    }
	CString text;
	text.Format(L"動画変換処理中: %02d", g_videoConvertCount);

	CWindow edit = g_hwndMainDlg.GetDlgItem(IDC_STATIC_VIDEOCONVERT);
	edit.SetWindowTextW(text);	
}

int		CurrentVideoConvertCount()
{
    return g_videoConvertCount;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UIAddChildWindowContainer(m_hWnd);

    _CreateTasktrayIcon();

    std::wstring localIPAddress = GetLocalIPAddress();
    if (localIPAddress.empty()) {
        localIPAddress = L"127.0.0.1";
    }
    m_localHttpServerAddress.Format(L"http://%s:%d/", localIPAddress.c_str(), Config::s_httpServerPort);
    GetDlgItem(IDC_BUTTON_OPEN_LOCALSERVER).SetWindowText(m_localHttpServerAddress);

	g_hwndMainDlg = m_hWnd;

    try {
        auto configPath = GetExeDirectory() / L"Config.json";
        std::ifstream fs(configPath.c_str(), std::ios::in | std::ios::binary);
        if (fs) {
            json jsonConfig;
            fs >> jsonConfig;
            auto& jsonMainDlg = jsonConfig["MainDlg"];
            if (jsonMainDlg.is_object()) {
                bool bShowWindow = jsonMainDlg["Window"]["Show"].get<bool>();
                int top = jsonMainDlg["Window"]["top"].get<int>();
                int left = jsonMainDlg["Window"]["left"].get<int>();
                SetWindowPos(NULL, left, top, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                if (bShowWindow) {
                    ShowWindow(TRUE);
                }
            } else {
                // center the dialog on the screen
                CenterWindow();
                ShowWindow(TRUE);
            }
        }
    } catch (std::exception& e) {
        ERROR_LOG << L"Load MainDlg Config failed : " << e.what();
    }
 
	return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

    {	// トレイアイコンを削除
        NOTIFYICONDATA	nid = { sizeof(NOTIFYICONDATA) };
        nid.hWnd = m_hWnd;
        nid.uID = kTrayIconId;
        ::Shell_NotifyIcon(NIM_DELETE, &nid);
    }

	return 0;
}

LRESULT CMainDlg::OnTrayIconNotify(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
    if (lParam == WM_LBUTTONDOWN) {
        ShowWindow(SW_SHOWNORMAL);
        SetForegroundWindow(m_hWnd);
    } else if (lParam == WM_RBUTTONUP) {
        enum {
            kOpenLocalServer = 100,
            kExit,
        };

        CMenu menu;
        menu.CreatePopupMenu();
        menu.AppendMenu(MF_STRING, kOpenLocalServer, L"ローカルサーバーを開く");
        menu.AppendMenu(MF_SEPARATOR);
        menu.AppendMenu(MF_STRING, kExit, L"終了");
        //menu.SetMenuDefaultItem(kExit);

        POINT pt;
        ::GetCursorPos(&pt);
        ::SetForegroundWindow(m_hWnd);
        BOOL bRet = menu.TrackPopupMenu(TPM_RETURNCMD, pt.x, pt.y, m_hWnd, NULL);
        switch (bRet) {
        case kOpenLocalServer:
            BOOL b;
            OnOpenLocalServer(0, 0, NULL, b);
            break;

        case kExit:
            CloseDialog(0);
            break;

        default:
            break;
        }
    }
    return 0;
}

void CMainDlg::OnSysCommand(UINT nID, CPoint point)
{
    if (nID == SC_MINIMIZE) {
        ShowWindow(SW_HIDE);
    } else {
        SetMsgHandled(FALSE);
    }
    //if (nID == SC_CLOSE/* && CSettings::s_tasktrayOnCloseBotton*/) {
    //    ShowWindow(FALSE);
    //}
}

LRESULT CMainDlg::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// TODO: Add validation code 
	CloseDialog(wID);
	return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CloseDialog(0);
	return 0;
}

LRESULT CMainDlg::OnOpenLocalServer(WORD, WORD wID, HWND, BOOL&)
{
    ::ShellExecute(NULL, L"open", m_localHttpServerAddress + L"?autoLoginPassword=" + Config::s_password.c_str(), NULL, NULL, SW_NORMAL);
    return LRESULT();
}

LRESULT CMainDlg::OnConfig(WORD, WORD wID, HWND, BOOL&)
{
	ConfigDlg dlg;
	dlg.DoModal();
	return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
    {
        json jsonConfig;

        auto configPath = GetExeDirectory() / L"Config.json";
        std::ifstream fs(configPath.c_str(), std::ios::in | std::ios::binary);
        if (fs) {
            fs >> jsonConfig;
            fs.close();
        }

        auto& jsonMainDlg = jsonConfig["MainDlg"];
        bool bVisible = IsWindowVisible() != 0;
        jsonMainDlg["Window"]["Show"] = bVisible;
        if (bVisible) {
            CRect rcWindow;
            GetWindowRect(&rcWindow);
            jsonMainDlg["Window"]["top"] = rcWindow.top;
            jsonMainDlg["Window"]["left"] = rcWindow.left;
        }

        std::ofstream fsw(configPath.c_str(), std::ios::out | std::ios::binary);
        fsw << std::setw(4) << jsonConfig;
        fsw.close();
    }

	DestroyWindow();
	::PostQuitMessage(nVal);
}

// トレイアイコンを作成
void	CMainDlg::_CreateTasktrayIcon()
{
    HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
    NOTIFYICONDATA	nid = { sizeof(NOTIFYICONDATA) };
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.hWnd = m_hWnd;
    nid.hIcon = hIconSmall;
    nid.uID = kTrayIconId;
    nid.uCallbackMessage = WM_TRAYICONNOTIFY;
    ::_tcscpy_s(nid.szTip, kAppName);
    if (::Shell_NotifyIcon(NIM_ADD, &nid) == FALSE && ::GetLastError() == ERROR_TIMEOUT) {
        do {
            ::Sleep(1000);
            if (::Shell_NotifyIcon(NIM_MODIFY, &nid))
                break;	// success
        } while (::Shell_NotifyIcon(NIM_ADD, &nid) == FALSE);
    }
    DestroyIcon(hIconSmall);
}