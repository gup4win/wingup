/*
 Copyright 2007 Don HO <don.h@free.fr>

 This file is part of GUP.

 GUP is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 GUP is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with GUP.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <windows.h>
#include <string>
#include <commctrl.h>
#include "resource.h"
#include <shlwapi.h>
#include "xmlTools.h"
#include <Uxtheme.h>
#define CURL_STATICLIB
#include "../curl/include/curl/curl.h"

using namespace std;

HINSTANCE hInst;
HHOOK g_hMsgBoxHook;
static HWND hProgressDlg;
static HWND hProgressBar;
static bool doAbort = false;
static bool stopDL = false;
static string msgBoxTitle = "";
static string abortOrNot = "";
static string proxySrv = "0.0.0.0";
static long proxyPort  = 0;
static string winGupUserAgent = "WinGup/";
static string dlFileName = "";
static string appIconFile = "";

const char FLAG_OPTIONS[] = "-options";
const char FLAG_VERBOSE[] = "-verbose";
const char FLAG_HELP[] = "--help";

const char MSGID_NOUPDATE[] = "No update is available.";
const char MSGID_UPDATEAVAILABLE[] = "An update package is available, do you want to download it?";
const char MSGID_DOWNLOADSTOPPED[] = "Download is stopped by user. Update is aborted.";
const char MSGID_CLOSEAPP[] = " is opened.\rUpdater will close it in order to process the installation.\rContinue?";
const char MSGID_ABORTORNOT[] = "Do you want to abort update download?";
const char MSGID_HELP[] = "Usage :\r\
\r\
gup --help\r\
gup -options\r\
gup [-verbose] [-vVERSION_VALUE] [-pCUSTOM_PARAM]\r\
\r\
    --help : Show this help message (and quit program).\r\
    -options : Show the proxy configuration dialog (and quit program).\r\
    -v : Launch GUP with VERSION_VALUE.\r\
         VERSION_VALUE is the current version number of program to update.\r\
         If you pass the version number as the argument,\r\
         then the version set in the gup.xml will be overrided.\r\
	-p : Launch GUP with CUSTOM_PARAM.\r\
	     CUSTOM_PARAM will pass to destination by using GET method\r\
         with argument name \"param\"\r\
    -verbose : Show error/warning message if any.";

std::string thirdDoUpdateDlgButtonLabel;

class CUXHelper
{
public:
	CUXHelper()
	{
		_hUXTheme = ::LoadLibrary(TEXT("uxtheme.dll"));
		if (_hUXTheme)
			_enableThemeDialogTextureFuncAddr = reinterpret_cast<ETDTProc>(::GetProcAddress(_hUXTheme, "EnableThemeDialogTexture"));

		g_hMsgBoxHook = SetWindowsHookEx(WH_CALLWNDPROC, CallWndProc, NULL, GetCurrentThreadId());
	}

	~CUXHelper()
	{
		if (_hUXTheme)
			::FreeLibrary(_hUXTheme);

		UnhookWindowsHookEx(g_hMsgBoxHook);
	}

	void goToScreenCenter(HWND hwnd)
	{
		RECT screenRc;
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRc, 0);

		POINT center;
		center.x = screenRc.left + (screenRc.right - screenRc.left) / 2;
		center.y = screenRc.top + (screenRc.bottom - screenRc.top) / 2;

		RECT rc;
		::GetWindowRect(hwnd, &rc);
		int x = center.x - (rc.right - rc.left) / 2;
		int y = center.y - (rc.bottom - rc.top) / 2;

		::SetWindowPos(hwnd, HWND_TOP, x, y, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
	}

	void enableDlgTheme(HWND hwnd)
	{
		if (_enableThemeDialogTextureFuncAddr)
		{
			_enableThemeDialogTextureFuncAddr(hwnd, ETDT_ENABLETAB);
			redraw(hwnd);
		}
	}

	static LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
	{
		if (nCode == HC_ACTION)
		{
			CWPSTRUCT* pcwp = (CWPSTRUCT*)lParam;

			if (pcwp->message == WM_INITDIALOG)
			{
				setIcon(pcwp->hwnd, appIconFile);
			}
		}

		return CallNextHookEx(g_hMsgBoxHook, nCode, wParam, lParam);
	}

	static void setIcon(HWND hwnd, string iconFile)
	{
		if (!iconFile.empty())
		{
			HICON hIcon = nullptr, hIconSm = nullptr;

			hIcon = reinterpret_cast<HICON>(LoadImageA(NULL, iconFile.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
			hIconSm = reinterpret_cast<HICON>(LoadImageA(NULL, iconFile.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE));
			if (hIcon && hIconSm)
			{
				SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
				SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
			}
		}
	}

private:
	void redraw(HWND hwnd, bool forceUpdate = false) const
	{
		::InvalidateRect(hwnd, nullptr, TRUE);
		if (forceUpdate)
			::UpdateWindow(hwnd);
	}

private:
	HMODULE _hUXTheme = nullptr;
	using ETDTProc = HRESULT(WINAPI *) (HWND, DWORD);
	ETDTProc _enableThemeDialogTextureFuncAddr = nullptr;
};
CUXHelper uxHelper;

static bool isInList(const char *token2Find, char *list2Clean) {
	char word[1024];
	bool isFileNamePart = false;

	for (int i = 0, j = 0 ;  i <= int(strlen(list2Clean)) ; i++)
	{
		if ((list2Clean[i] == ' ') || (list2Clean[i] == '\0'))
		{
			if ((j) && (!isFileNamePart))
			{
				word[j] = '\0';
				j = 0;
				bool bingo = !strcmp(token2Find, word);

				if (bingo)
				{
					int wordLen = int(strlen(word));
					int prevPos = i - wordLen;

					for (i = i + 1 ;  i <= int(strlen(list2Clean)) ; i++, prevPos++)
						list2Clean[prevPos] = list2Clean[i];

					list2Clean[prevPos] = '\0';
					
					return true;
				}
			}
		}
		else if (list2Clean[i] == '"')
		{
			isFileNamePart = !isFileNamePart;
		}
		else
		{
			word[j++] = list2Clean[i];
		}
	}
	return false;
};

static string getParamVal(char c, char *list2Clean) {
	char word[1024];
	bool checkDash = true;
	bool checkCh = false;
	bool action = false;
	bool isFileNamePart = false;
	int pos2Erase = 0;

	for (int i = 0, j = 0 ;  i <= int(strlen(list2Clean)) ; i++)
	{
		if ((list2Clean[i] == ' ') || (list2Clean[i] == '\0'))
		{
			if (action)
			{
				word[j] = '\0';
				j = 0;
				action = false;

				for (i = i + 1 ;  i <= int(strlen(list2Clean)) ; i++, pos2Erase++)
					list2Clean[pos2Erase] = list2Clean[i];
						
				list2Clean[pos2Erase] = '\0';

				return word;
			}
			checkDash = true;
		}
		else if (list2Clean[i] == '"')
		{
			isFileNamePart = !isFileNamePart;
		}

		if (!isFileNamePart)
		{
			if (action)
			{
				word[j++] =  list2Clean[i];
			}
			else if (checkDash)
			{
				if (list2Clean[i] == '-')
					checkCh = true;
			            
				if (list2Clean[i] != ' ')
					checkDash = false;
			}
			else if (checkCh)
			{
				if (list2Clean[i] == c)
				{
					action = true;
					pos2Erase = i-1;
				}
				checkCh = false;
			}
		}
	}
	return "";
};

// This is the getUpdateInfo call back function used by curl
static size_t getUpdateInfoCallback(char *data, size_t size, size_t nmemb, std::string *updateInfo)
{
	// What we will return
	size_t len = size * nmemb;
	
	// Is there anything in the buffer?
	if (updateInfo != NULL)
	{
		// Append the data to the buffer
		updateInfo->append(data, len);
	}

	return len;
}

static size_t getDownloadData(unsigned char *data, size_t size, size_t nmemb, FILE *fp)
{
	if (doAbort)
		return 0;

	size_t len = size * nmemb;
	fwrite(data, len, 1, fp);
	return len;
};

static size_t ratio = 0;

static size_t setProgress(HWND, double t, double d, double, double)
{
	while (stopDL)
		::Sleep(1000);
	size_t step = size_t(d * 100.0 / t - ratio);
	ratio = size_t(d * 100.0 / t);

	SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)step, 0);
	SendMessage(hProgressBar, PBM_STEPIT, 0, 0);

	char percentage[128];
	sprintf(percentage, "Downloading %s: %Iu %%", dlFileName.c_str(), ratio);
	::SetWindowTextA(hProgressDlg, percentage);
	return 0;
};

LRESULT CALLBACK progressBarDlgProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM )
{
	INITCOMMONCONTROLSEX InitCtrlEx;

	InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
	InitCtrlEx.dwICC  = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&InitCtrlEx);

	switch(Msg)
	{
		case WM_INITDIALOG:
			hProgressDlg = hWndDlg;
			hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
										  20, 20, 280, 17,
										  hWndDlg, NULL, hInst, NULL);
			SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100)); 
			SendMessage(hProgressBar, PBM_SETSTEP, 1, 0);
			CUXHelper::setIcon(hProgressDlg, appIconFile);
			uxHelper.goToScreenCenter(hWndDlg);
			uxHelper.enableDlgTheme(hWndDlg);
			return TRUE; 

		case WM_COMMAND:
			switch(wParam)
			{
			case IDOK:
				EndDialog(hWndDlg, 0);
				return TRUE;
			case IDCANCEL:
				stopDL = true;
				if (abortOrNot == "")
					abortOrNot = MSGID_ABORTORNOT;
				int abortAnswer = ::MessageBoxA(hWndDlg, abortOrNot.c_str(), msgBoxTitle.c_str(), MB_YESNO);
				if (abortAnswer == IDYES)
				{
					doAbort = true;
					EndDialog(hWndDlg, 0);
				}
				stopDL = false;
				return TRUE;
			}
			break;
	}

	return FALSE;
}


LRESULT CALLBACK yesNoNeverDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			if (thirdDoUpdateDlgButtonLabel != "")
				::SetDlgItemTextA(hWndDlg, IDCANCEL, thirdDoUpdateDlgButtonLabel.c_str());

			uxHelper.goToScreenCenter(hWndDlg);
			uxHelper.enableDlgTheme(hWndDlg);
			return TRUE;
		}

		case WM_COMMAND:
		{
			switch (wParam)
			{
				case IDYES:
				case IDNO:
				case IDCANCEL:
					EndDialog(hWndDlg, wParam);
					return TRUE;

				default:
					break;
			}
		}

		case WM_DESTROY:
		{
			return TRUE;
		}
	}
	return FALSE;
}

LRESULT CALLBACK proxyDlgProc(HWND hWndDlg, UINT Msg, WPARAM wParam, LPARAM)
{

	switch(Msg)
	{
		case WM_INITDIALOG:
			::SetDlgItemTextA(hWndDlg, IDC_PROXYSERVER_EDIT, proxySrv.c_str());
			::SetDlgItemInt(hWndDlg, IDC_PORT_EDIT, proxyPort, FALSE);
			uxHelper.goToScreenCenter(hWndDlg);
			uxHelper.enableDlgTheme(hWndDlg);
			return TRUE; 

		case WM_COMMAND:
			switch(wParam)
			{
				case IDOK:
				{
					char proxyServer[MAX_PATH];
					::GetDlgItemTextA(hWndDlg, IDC_PROXYSERVER_EDIT, proxyServer, MAX_PATH);
					proxySrv = proxyServer;
					proxyPort = ::GetDlgItemInt(hWndDlg, IDC_PORT_EDIT, NULL, FALSE);
					EndDialog(hWndDlg, 1);
					return TRUE;
				}
				case IDCANCEL:
					EndDialog(hWndDlg, 0);
					return TRUE;
			}
			break;
	}

	return FALSE;
}

static DWORD WINAPI launchProgressBar(void *)
{
	::DialogBox(hInst, MAKEINTRESOURCE(IDD_PROGRESS_DLG), NULL, reinterpret_cast<DLGPROC>(progressBarDlgProc));
	return 0;
}

bool downloadBinary(string urlFrom, string destTo, pair<string, int> proxyServerInfo, bool isSilentMode, pair<string, string> stoppedMessage)
{
	FILE* pFile = fopen(destTo.c_str(), "wb");

	//  Download the install package from indicated location
	char errorBuffer[CURL_ERROR_SIZE] = { 0 };
	CURLcode res = CURLE_FAILED_INIT;
	CURL* curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, urlFrom.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, getDownloadData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, pFile);

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, setProgress);
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, hProgressBar);

		curl_easy_setopt(curl, CURLOPT_USERAGENT, winGupUserAgent.c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

		if (!proxyServerInfo.first.empty() && proxyServerInfo.second != -1)
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, proxyServerInfo.first.c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxyServerInfo.second);
		}
		curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST | CURLSSLOPT_NO_REVOKE);

		res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);
	}

	if (res != CURLE_OK)
	{
		if (!isSilentMode && doAbort == false)
			::MessageBoxA(NULL, errorBuffer, "curl error", MB_OK);
		if (doAbort)
		{
			::MessageBoxA(NULL, stoppedMessage.first.c_str(), stoppedMessage.second.c_str(), MB_OK);
		}
		doAbort = false;
		return false;
	}

	fflush(pFile);
	fclose(pFile);

	return true;
}

bool getUpdateInfo(string &info2get, const GupParameters& gupParams, const GupExtraOptions& proxyServer, const string& customParam, const string& version)
{
	char errorBuffer[CURL_ERROR_SIZE] = { 0 };

	// Check on the web the availibility of update
	// Get the update package's location
	CURL *curl;
	CURLcode res = CURLE_FAILED_INIT;

	curl = curl_easy_init();
	if (curl)
	{
		std::string urlComplete = gupParams.getInfoLocation() + "?version=";
		if (!version.empty())
			urlComplete += version;
		else
			urlComplete += gupParams.getCurrentVersion();

		if (!customParam.empty())
		{
			string customParamPost = "&param=";
			customParamPost += customParam;
			urlComplete += customParamPost;
		}
		else if (!gupParams.getParam().empty())
		{
			string customParamPost = "&param=";
			customParamPost += gupParams.getParam();
			urlComplete += customParamPost;
		}

		curl_easy_setopt(curl, CURLOPT_URL, urlComplete.c_str());


		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, getUpdateInfoCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &info2get);

		string ua = gupParams.getSoftwareName();

		winGupUserAgent += VERSION_VALUE;
		if (ua != "")
		{
			ua += "/";
			ua += version;
			ua += " (";
			ua += winGupUserAgent;
			ua += ")";

			winGupUserAgent = ua;
		}

		curl_easy_setopt(curl, CURLOPT_USERAGENT, winGupUserAgent.c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

		if (proxyServer.hasProxySettings())
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, proxyServer.getProxyServer().c_str());
			curl_easy_setopt(curl, CURLOPT_PROXYPORT, proxyServer.getPort());
		}

		curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST | CURLSSLOPT_NO_REVOKE);

		res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);
	}

	if (res != CURLE_OK)
	{
		if (!gupParams.isSilentMode())
			::MessageBoxA(NULL, errorBuffer, "curl error", MB_OK);
		return false;
	}
	return true;
}

bool runInstaller(const string& app2runPath, const string& binWindowsClassName, const string& closeMsg, const string& closeMsgTitle)
{

	if (!binWindowsClassName.empty())
	{
		HWND h = ::FindWindowExA(NULL, NULL, binWindowsClassName.c_str(), NULL);

		if (h)
		{
			int installAnswer = ::MessageBoxA(NULL, closeMsg.c_str(), closeMsgTitle.c_str(), MB_YESNO);

			if (installAnswer == IDNO)
			{
				return 0;
			}
		}

		// kill all process of binary needs to be updated.
		while (h)
		{
			::SendMessage(h, WM_CLOSE, 0, 0);
			h = ::FindWindowExA(NULL, NULL, binWindowsClassName.c_str(), NULL);
		}
	}

	// execute the installer
	HINSTANCE result = ::ShellExecuteA(NULL, "open", app2runPath.c_str(), "", ".", SW_SHOW);

	if (result <= (HINSTANCE)32) // There's a problem (Don't ask me why, ask Microsoft)
	{
		return false;
	}

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpszCmdLine, int)
{
	bool isSilentMode = false;
	FILE *pFile = NULL;

	bool launchSettingsDlg = false;
	bool isVerbose = false;
	bool isHelp = false;
	string version = "";
	string customParam = "";

	if (lpszCmdLine && lpszCmdLine[0])
	{
		launchSettingsDlg = isInList(FLAG_OPTIONS, lpszCmdLine);
		isVerbose = isInList(FLAG_VERBOSE, lpszCmdLine);
		isHelp = isInList(FLAG_HELP, lpszCmdLine);
		version = getParamVal('v', lpszCmdLine);
		customParam = getParamVal('p', lpszCmdLine);
	}

	// Object (gupParams) is moved here because we need app icon form configuration file
	GupParameters gupParams("gup.xml");
	appIconFile = gupParams.getSoftwareIcon();

	if (isHelp)
	{
		::MessageBoxA(NULL, MSGID_HELP, "GUP Command Argument Help", MB_OK);
		return 0;
	}

	GupExtraOptions extraOptions("gupOptions.xml");
	GupNativeLang nativeLang("nativeLang.xml");

	hInst = hInstance;
	try {
		if (launchSettingsDlg)
		{
			if (extraOptions.hasProxySettings())
			{
				proxySrv = extraOptions.getProxyServer();
				proxyPort = extraOptions.getPort();
			}
			if (::DialogBox(hInst, MAKEINTRESOURCE(IDD_PROXY_DLG), NULL, reinterpret_cast<DLGPROC>(proxyDlgProc)))
				extraOptions.writeProxyInfo("gupOptions.xml", proxySrv.c_str(), proxyPort);

			return 0;
		}

		msgBoxTitle = gupParams.getMessageBoxTitle();
		abortOrNot = nativeLang.getMessageString("MSGID_ABORTORNOT");

		//
		// Get update info
		//
		std::string updateInfo;

		// Get your software's current version.
		// If you pass the version number as the argument
		// then the version set in the gup.xml will be overrided
		if (!version.empty())
			gupParams.setCurrentVersion(version.c_str());

		// override silent mode if "-isVerbose" is passed as argument
		if (isVerbose)
			gupParams.setSilentMode(false);

		isSilentMode = gupParams.isSilentMode();

		bool getUpdateInfoSuccessful = getUpdateInfo(updateInfo, gupParams, extraOptions, customParam, version);

		if (!getUpdateInfoSuccessful)
			return -1;
		

		GupDownloadInfo gupDlInfo(updateInfo.c_str());

		if (!gupDlInfo.doesNeed2BeUpdated())
		{
			if (!isSilentMode)
			{
				string noUpdate = nativeLang.getMessageString("MSGID_NOUPDATE");
				if (noUpdate == "")
					noUpdate = MSGID_NOUPDATE;
				::MessageBoxA(NULL, noUpdate.c_str(), gupParams.getMessageBoxTitle().c_str(), MB_OK);
			}
			return 0;
		}


		//
		// Process Update Info
		//

		// Ask user if he/she want to do update
		string updateAvailable = nativeLang.getMessageString("MSGID_UPDATEAVAILABLE");
		if (updateAvailable == "")
			updateAvailable = MSGID_UPDATEAVAILABLE;
		
		int thirdButtonCmd = gupParams.get3rdButtonCmd();
		thirdDoUpdateDlgButtonLabel = gupParams.get3rdButtonLabel();

		int dlAnswer = 0;
		HWND hApp = ::FindWindowExA(NULL, NULL, gupParams.getClassName().c_str(), NULL);
		bool isModal = gupParams.isMessageBoxModal();

		if (!thirdButtonCmd)
			dlAnswer = ::MessageBoxA(isModal ? hApp : NULL, updateAvailable.c_str(), gupParams.getMessageBoxTitle().c_str(), MB_YESNO);
		else
			dlAnswer = static_cast<int32_t>(::DialogBox(hInst, MAKEINTRESOURCE(IDD_YESNONEVERDLG), isModal ? hApp : NULL, reinterpret_cast<DLGPROC>(yesNoNeverDlgProc)));

		if (dlAnswer == IDNO)
		{
			return 0;
		}
		
		if (dlAnswer == IDCANCEL)
		{
			if (gupParams.getClassName() != "")
			{
				if (hApp)
				{
					::SendMessage(hApp, thirdButtonCmd, gupParams.get3rdButtonWparam(), gupParams.get3rdButtonLparam());
				}
			}
			return 0;
		}

		//
		// Download executable bin
		//
		::CreateThread(NULL, 0, launchProgressBar, NULL, 0, NULL);
		
		std::string dlDest = std::getenv("TEMP");
		dlDest += "\\";
		dlDest += ::PathFindFileNameA(gupDlInfo.getDownloadLocation().c_str());

        char *ext = ::PathFindExtensionA(gupDlInfo.getDownloadLocation().c_str());
        if (strcmp(ext, ".exe") != 0)
            dlDest += ".exe";

		dlFileName = ::PathFindFileNameA(gupDlInfo.getDownloadLocation().c_str());


		string dlStopped = nativeLang.getMessageString("MSGID_DOWNLOADSTOPPED");
		if (dlStopped == "")
			dlStopped = MSGID_DOWNLOADSTOPPED;

		bool dlSuccessful = downloadBinary(gupDlInfo.getDownloadLocation(), dlDest, pair<string, int>(extraOptions.getProxyServer(), extraOptions.getPort()), isSilentMode, pair<string, string>(dlStopped, gupParams.getMessageBoxTitle()));

		if (!dlSuccessful)
			return -1;


		//
		// Run executable bin
		//
		string msg = gupParams.getClassName();
		string closeApp = nativeLang.getMessageString("MSGID_CLOSEAPP");
		if (closeApp == "")
			closeApp = MSGID_CLOSEAPP;
		msg += closeApp;

		runInstaller(dlDest, gupParams.getClassName(), msg, gupParams.getMessageBoxTitle().c_str());

		return 0;

	} catch (exception ex) {
		if (!isSilentMode)
			::MessageBoxA(NULL, ex.what(), "Xml Exception", MB_OK);

		if (pFile != NULL)
			fclose(pFile);

		return -1;
	}
	catch (...)
	{
		if (!isSilentMode)
			::MessageBoxA(NULL, "Unknown", "Unknown Exception", MB_OK);

		if (pFile != NULL)
			fclose(pFile);

		return -1;
	}
}
