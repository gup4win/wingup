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

#include "../ZipLib/ZipFile.h"
#include "../ZipLib/utils/stream_utils.h"

#include <stdint.h>
#include <sys/stat.h>
#include <windows.h>
#include <fstream>
#include <string>
#include <commctrl.h>
#include "resource.h"
#include <shlwapi.h>
#include "xmlTools.h"
#include "sha-256.h"

#define CURL_STATICLIB
#include "../curl/include/curl/curl.h"

using namespace std;

typedef vector<wstring> ParamVector;

HINSTANCE hInst;
static HWND hProgressDlg;
static HWND hProgressBar;
static bool doAbort = false;
static bool stopDL = false;
static wstring msgBoxTitle = L"";
static wstring abortOrNot = L"";
static wstring proxySrv = L"0.0.0.0";
static long proxyPort  = 0;
static wstring winGupUserAgent = L"WinGup/";
static wstring dlFileName = L"";

const wchar_t FLAG_OPTIONS[] = L"-options";
const wchar_t FLAG_VERBOSE[] = L"-verbose";
const wchar_t FLAG_HELP[] = L"--help";
const wchar_t FLAG_UUZIP[] = L"-unzipTo";
const wchar_t FLAG_CLEANUP[] = L"-clean";

const wchar_t MSGID_UPDATEAVAILABLE[] = L"An update package is available, do you want to download it?";
const wchar_t MSGID_DOWNLOADSTOPPED[] = L"Download is stopped by user. Update is aborted.";
const wchar_t MSGID_CLOSEAPP[] = L" is opened.\rUpdater will close it in order to process the installation.\rContinue?";
const wchar_t MSGID_ABORTORNOT[] = L"Do you want to abort update download?";
const wchar_t MSGID_UNZIPFAILED[] = L"Can't unzip:\nOperation not permitted or decompression failed";
const wchar_t MSGID_HELP[] = L"Usage :\r\
\r\
gup --help\r\
gup -options\r\
gup [-verbose] [-vVERSION_VALUE] [-pCUSTOM_PARAM]\r\
gup -clean FOLDER_TO_ACTION\r\
gup -unzipTo [-clean] FOLDER_TO_ACTION ZIP_URL\r\
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
    -verbose : Show error/warning message if any.\r\
    -clean: Delete all files in FOLDER_TO_ACTION.\r\
    -unzipTo: Download zip file from ZIP_URL then unzip it into FOLDER_TO_ACTION.\r\
    ZIP_URL: The URL to download zip file.\r\
    FOLDER_TO_ACTION: The folder where we clean or/and unzip to.\r\
	";
std::wstring thirdDoUpdateDlgButtonLabel;

void writeLog(const wchar_t *logFileName, const wchar_t *logSuffix, const wchar_t *log2write)
{
	FILE *f = _wfopen(logFileName, L"a+");
	if (f)
	{
		wstring log = logSuffix;
		log += log2write;
		log += '\n';
		fwrite(log.c_str(), sizeof(log.c_str()[0]), log.length(), f);
		fflush(f);
		fclose(f);
	}
};

//commandLine should contain path to n++ executable running
void parseCommandLine(const wchar_t* commandLine, ParamVector& paramVector)
{
	if (!commandLine)
		return;

	wchar_t* cmdLine = new wchar_t[lstrlen(commandLine) + 1];
	lstrcpy(cmdLine, commandLine);

	wchar_t* cmdLinePtr = cmdLine;

	bool isInFile = false;
	bool isInWhiteSpace = true;
	size_t commandLength = lstrlen(cmdLinePtr);
	std::vector<wchar_t *> args;
	for (size_t i = 0; i < commandLength; ++i)
	{
		switch (cmdLinePtr[i])
		{
			case '\"': //quoted filename, ignore any following whitespace
			{
				if (!isInFile)	//" will always be treated as start or end of param, in case the user forgot to add an space
				{
					args.push_back(cmdLinePtr + i + 1);	//add next param(since zero terminated original, no overflow of +1)
				}
				isInFile = !isInFile;
				isInWhiteSpace = false;
				//because we dont want to leave in any quotes in the filename, remove them now (with zero terminator)
				cmdLinePtr[i] = 0;
			}
			break;

			case '\t': //also treat tab as whitespace
			case ' ':
			{
				isInWhiteSpace = true;
				if (!isInFile)
					cmdLinePtr[i] = 0;		//zap spaces into zero terminators, unless its part of a filename	
			}
			break;

			default: //default char, if beginning of word, add it
			{
				if (!isInFile && isInWhiteSpace)
				{
					args.push_back(cmdLinePtr + i);	//add next param
					isInWhiteSpace = false;
				}
			}
		}
	}
	paramVector.assign(args.begin(), args.end());
	delete[] cmdLine;
};

bool isInList(const wchar_t* token2Find, ParamVector & params)
{
	size_t nbItems = params.size();

	for (size_t i = 0; i < nbItems; ++i)
	{
		if (!lstrcmp(token2Find, params.at(i).c_str()))
		{
			params.erase(params.begin() + i);
			return true;
		}
	}
	return false;
};

bool getParamVal(wchar_t c, ParamVector & params, wstring & value)
{
	value = L"";
	size_t nbItems = params.size();

	for (size_t i = 0; i < nbItems; ++i)
	{
		const wchar_t* token = params.at(i).c_str();
		if (token[0] == '-' && lstrlen(token) >= 2 && token[1] == c) //dash, and enough chars
		{
			value = (token + 2);
			params.erase(params.begin() + i);
			return true;
		}
	}
	return false;
}

wstring PathAppend(wstring& strDest, const wstring& str2append)
{
	if (strDest.empty() && str2append.empty()) // "" + ""
	{
		strDest = L"\\";
		return strDest;
	}

	if (strDest.empty() && !str2append.empty()) // "" + titi
	{
		strDest = str2append;
		return strDest;
	}

	if (strDest[strDest.length() - 1] == '\\' && (!str2append.empty() && str2append[0] == '\\')) // toto\ + \titi
	{
		strDest.erase(strDest.length() - 1, 1);
		strDest += str2append;
		return strDest;
	}

	if ((strDest[strDest.length() - 1] == '\\' && (!str2append.empty() && str2append[0] != '\\')) // toto\ + titi
		|| (strDest[strDest.length() - 1] != '\\' && (!str2append.empty() && str2append[0] == '\\'))) // toto + \titi
	{
		strDest += str2append;
		return strDest;
	}

	// toto + titi
	strDest += L"\\";
	strDest += str2append;

	return strDest;
};

vector<wstring> tokenizeString(const wstring & tokenString, const char delim)
{
	//Vector is created on stack and copied on return
	std::vector<wstring> tokens;

	// Skip delimiters at beginning.
	string::size_type lastPos = tokenString.find_first_not_of(delim, 0);
	// Find first "non-delimiter".
	string::size_type pos = tokenString.find_first_of(delim, lastPos);

	while (pos != std::string::npos || lastPos != std::string::npos)
	{
		// Found a token, add it to the vector.
		tokens.push_back(tokenString.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = tokenString.find_first_not_of(delim, pos);
		// Find next "non-delimiter"
		pos = tokenString.find_first_of(delim, lastPos);
	}
	return tokens;
};

bool deleteFileOrFolder(const wstring& f2delete)
{
	auto len = f2delete.length();
	wchar_t* actionFolder = new wchar_t[len + 2];
	lstrcpy(actionFolder, f2delete.c_str());
	actionFolder[len] = 0;
	actionFolder[len + 1] = 0;

	SHFILEOPSTRUCT fileOpStruct = { 0 };
	fileOpStruct.hwnd = NULL;
	fileOpStruct.pFrom = actionFolder;
	fileOpStruct.pTo = NULL;
	fileOpStruct.wFunc = FO_DELETE;
	fileOpStruct.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_ALLOWUNDO;
	fileOpStruct.fAnyOperationsAborted = false;
	fileOpStruct.hNameMappings = NULL;
	fileOpStruct.lpszProgressTitle = NULL;

	int res = SHFileOperation(&fileOpStruct);

	delete[] actionFolder;
	return (res == 0);
};

std::wstring getFileContent(const wchar_t *file2read)
{
	if (!::PathFileExists(file2read))
		return L"";

	const size_t blockSize = 1024;
	wchar_t data[blockSize];
	std::wstring wholeFileContent = L"";
	FILE *fp = _wfopen(file2read, L"rb");
	if(!fp)
		return L"";

	size_t lenFile = 0;
	do
	{
		lenFile = fread(data, 1, blockSize, fp);
		if (lenFile <= 0) break;
		wholeFileContent.append(data, lenFile);
	} while (lenFile > 0);

	fclose(fp);
	return wholeFileContent;
};

// unzipDestTo should be plugin home root + plugin folder name
// ex: %APPDATA%\..\local\Notepad++\plugins\myAwesomePlugin
bool decompress(const wstring& zipFullFilePath, const wstring& unzipDestTo)
{
	// if destination folder doesn't exist, create it.
	if (!::PathFileExists(unzipDestTo.c_str()))
	{
		if (!::CreateDirectory(unzipDestTo.c_str(), NULL))
			return false;
	}

	string zipFullFilePathA = ws2s(zipFullFilePath);
	ZipArchive::Ptr archive = ZipFile::Open(zipFullFilePathA.c_str());

	std::istream* decompressStream = nullptr;
	auto count = archive->GetEntriesCount();

	if (!count) // wrong archive format
		return false;

	for (size_t i = 0; i < count; ++i)
	{
		ZipArchiveEntry::Ptr entry = archive->GetEntry(static_cast<int>(i));
		assert(entry != nullptr);

		//("[+] Trying no pass...\n");
		decompressStream = entry->GetDecompressionStream();
		assert(decompressStream != nullptr);

		wstring file2extrait = s2ws(entry->GetFullName());
		wstring extraitFullFilePath = unzipDestTo;
		PathAppend(extraitFullFilePath, file2extrait);


		// file2extrait be separated into an array
		vector<wstring> strArray = tokenizeString(file2extrait, '/');
		wstring folderPath = unzipDestTo;

		if (entry->IsDirectory())
		{
			// if folder doesn't exist, create it.
			if (!::PathFileExists(extraitFullFilePath.c_str()))
			{
				const size_t msgLen = 1024;
				wchar_t msg[msgLen];
				swprintf(msg, msgLen, L"[+] Create folder '%s'\n", file2extrait.c_str());
				OutputDebugString(msg);		

				for (size_t k = 0; k < strArray.size(); ++k)
				{
					PathAppend(folderPath, strArray[k]);

					if (!::PathFileExists(folderPath.c_str()))
					{
						::CreateDirectory(folderPath.c_str(), NULL);
					}
					else if (!::PathIsDirectory(folderPath.c_str())) // The unzip core component is not reliable for the file/directory detection 
					{                                                 // Hence such hack to make the result is as correct as possible
						// if it's a file, remove it
						deleteFileOrFolder(folderPath);

						// create it
						::CreateDirectory(folderPath.c_str(), NULL);
					}
				}
			}
		}
		else // it's a file
		{
			const size_t msgLen = 1024;
			wchar_t msg[msgLen];
			swprintf(msg, msgLen, L"[+] Extracting file '%s'\n", file2extrait.c_str());
			OutputDebugString(msg);

			for (size_t k = 0; k < strArray.size() - 1; ++k) // loop on only directory, not on file (which is the last element)
			{
				PathAppend(folderPath, strArray[k]);
				if (!::PathFileExists(folderPath.c_str()))
				{
					::CreateDirectory(folderPath.c_str(), NULL);
				}
				else if (!::PathIsDirectory(folderPath.c_str())) // The unzip core component is not reliable for the file/directory detection 
				{                                                 // Hence such hack to make the result is as correct as possible
					// if it's a file, remove it
					deleteFileOrFolder(folderPath);

					// create it
					::CreateDirectory(folderPath.c_str(), NULL);
				}
			}

			std::ofstream destFile;
			destFile.open(extraitFullFilePath, std::ios::binary | std::ios::trunc);
			
			utils::stream::copy(*decompressStream, destFile);

			destFile.flush();
			destFile.close();
		}
	}

	// check installed dll
	wstring pluginFolder = PathFindFileName(unzipDestTo.c_str());
	wstring installedPluginPath = unzipDestTo + L"\\" + pluginFolder + L".dll";
	
	if (::PathFileExists(installedPluginPath.c_str()))
	{
		// DLL is deployed correctly.
		// OK and nothing to do.
	}
	else
	{
		// Remove installed plugin
		MessageBox(NULL, TEXT("The plugin package is built wrongly. This plugin will be uninstalled."), TEXT("GUP"), MB_OK | MB_APPLMODAL);

		deleteFileOrFolder(unzipDestTo);
		return FALSE;
	}

	return true;
};

static void goToScreenCenter(HWND hwnd)
{
    RECT screenRc;
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRc, 0);

    POINT center;
	center.x = screenRc.left + (screenRc.right - screenRc.left) / 2;
    center.y = screenRc.top + (screenRc.bottom - screenRc.top)/2;

	RECT rc;
	::GetWindowRect(hwnd, &rc);
	int x = center.x - (rc.right - rc.left)/2;
	int y = center.y - (rc.bottom - rc.top)/2;

	::SetWindowPos(hwnd, HWND_TOP, x, y, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
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

static size_t setProgress(HWND, double dlTotal, double dlSoFar, double, double)
{
	while (stopDL)
		::Sleep(1000);

	size_t downloadedRatio = SendMessage(hProgressBar, PBM_DELTAPOS, 0, 0);

	size_t step = size_t(dlSoFar * 100.0 / dlTotal - downloadedRatio);

	SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)step, 0);
	SendMessage(hProgressBar, PBM_STEPIT, 0, 0);

	const size_t percentageLen = 1024;
	wchar_t percentage[percentageLen];
	swprintf(percentage, percentageLen, L"Downloading %s: %Iu %%", dlFileName.c_str(), downloadedRatio);
	::SetWindowText(hProgressDlg, percentage);
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
			goToScreenCenter(hWndDlg);
			return TRUE; 

		case WM_COMMAND:
			switch(wParam)
			{
			case IDOK:
				EndDialog(hWndDlg, 0);
				return TRUE;
			case IDCANCEL:
				stopDL = true;
				if (abortOrNot == L"")
					abortOrNot = MSGID_ABORTORNOT;
				int abortAnswer = ::MessageBox(hWndDlg, abortOrNot.c_str(), msgBoxTitle.c_str(), MB_YESNO);
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
			if (thirdDoUpdateDlgButtonLabel != L"")
				::SetDlgItemText(hWndDlg, IDCANCEL, thirdDoUpdateDlgButtonLabel.c_str());

			goToScreenCenter(hWndDlg);
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
			::SetDlgItemText(hWndDlg, IDC_PROXYSERVER_EDIT, proxySrv.c_str());
			::SetDlgItemInt(hWndDlg, IDC_PORT_EDIT, proxyPort, FALSE);
			goToScreenCenter(hWndDlg);
			return TRUE; 

		case WM_COMMAND:
			switch(wParam)
			{
				case IDOK:
				{
					wchar_t proxyServer[MAX_PATH];
					::GetDlgItemText(hWndDlg, IDC_PROXYSERVER_EDIT, proxyServer, MAX_PATH);
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

struct UpdateCheckParams
{
	GupNativeLang& _nativeLang;
	GupParameters& _gupParams;
};

LRESULT CALLBACK updateCheckDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
		{
			auto* params = reinterpret_cast<UpdateCheckParams*>(lParam);
			if (params)
			{
				const wstring& title = params->_gupParams.getMessageBoxTitle();
				if (!title.empty())
					::SetWindowText(hWndDlg, title.c_str());

				wstring textMsg = params->_nativeLang.getMessageString("MSGID_NOUPDATE");
				if (!textMsg.empty())
					::SetDlgItemText(hWndDlg, IDC_UPDATE_STATIC1, textMsg.c_str());

				wstring goToDlStr = params->_nativeLang.getMessageString("MSGID_DOWNLOADTEXT");
				if (!goToDlStr.empty())
				{
					wstring textLink = L"<a id=\"id_download\">";
					textLink += goToDlStr;
					textLink += L"</a>";
					::SetDlgItemText(hWndDlg, IDC_DOWNLOAD_LINK, textLink.c_str());
				}
			}
			goToScreenCenter(hWndDlg);
			return TRUE;
		}

		case WM_COMMAND:
		{
			switch LOWORD((wParam))
			{
				case IDOK:
				case IDYES:
				case IDNO:
				case IDCANCEL:
					EndDialog(hWndDlg, wParam);
					return TRUE;
				default:
					break;
			}
		}

		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code)
			{
				case NM_CLICK:
				case NM_RETURN:
				{
					PNMLINK pNMLink = (PNMLINK)lParam;
					LITEM item = pNMLink->item;
					if (lstrcmpW(item.szID, L"id_download") == 0)
					{
						::ShellExecute(NULL, TEXT("open"), TEXT("https://notepad-plus-plus.org/downloads/"), NULL, NULL, SW_SHOWNORMAL);
					}
					break;
				}
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

bool downloadBinary(const wstring& urlFrom, const wstring& destTo, const wstring& sha2HashToCheck, pair<wstring, int> proxyServerInfo, bool isSilentMode, const pair<wstring, wstring>& stoppedMessage)
{
	FILE* pFile = _wfopen(destTo.c_str(), L"wb");
	if (!pFile)
		return false;

	//  Download the install package from indicated location
	char errorBuffer[CURL_ERROR_SIZE] = { 0 };
	CURLcode res = CURLE_FAILED_INIT;
	CURL* curl = curl_easy_init();

	::CreateThread(NULL, 0, launchProgressBar, NULL, 0, NULL);
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, ws2s(urlFrom).c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, getDownloadData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, pFile);

		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, FALSE);
		curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, setProgress);
		curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, hProgressBar);

		curl_easy_setopt(curl, CURLOPT_USERAGENT, ws2s(winGupUserAgent).c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

		if (!proxyServerInfo.first.empty() && proxyServerInfo.second != -1)
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, ws2s(proxyServerInfo.first).c_str());
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
			::MessageBox(NULL, stoppedMessage.first.c_str(), stoppedMessage.second.c_str(), MB_OK);
		}
		doAbort = false;
		return false;
	}
	fflush(pFile);
	fclose(pFile);

	//
	// Check the hash if need
	//
	bool ok = true;
	if (!sha2HashToCheck.empty())
	{
		char sha2hashStr[65] = { '\0' };
		std::wstring content = getFileContent(destTo.c_str());
		if (content.empty())
		{
			// Remove installed plugin
			MessageBox(NULL, L"The plugin package is not found.", L"Plugin cannot be found", MB_OK | MB_APPLMODAL);
			ok = false;
		}
		else
		{
			uint8_t sha2hash[32];
			calc_sha_256(sha2hash, reinterpret_cast<const uint8_t*>(content.c_str()), content.length());

			for (size_t i = 0; i < 32; i++)
			{
				sprintf(sha2hashStr + i * 2, "%02x", sha2hash[i]);
			}
			wstring sha2HashToCheckLowerCase = sha2HashToCheck;
			std::transform(sha2HashToCheckLowerCase.begin(), sha2HashToCheckLowerCase.end(), sha2HashToCheckLowerCase.begin(),
				[](char c) { return static_cast<char>(::tolower(c)); });

			wstring sha2hashStrW = s2ws(sha2hashStr);
			if (sha2HashToCheckLowerCase != sha2hashStrW)
			{
				wstring pluginPackageName = ::PathFindFileName(destTo.c_str());
				wstring msg = L"The hash of plugin package \"";
				msg += pluginPackageName;
				msg += L"\" is not correct. Expected:\r";
				msg += sha2HashToCheckLowerCase;
				msg += L"\r<> Found:\r";
				msg += sha2hashStrW;
				msg += L"\rThis plugin won't be installed.";
				MessageBox(NULL, msg.c_str(), L"Plugin package hash mismatched", MB_OK | MB_APPLMODAL);
				ok = false;
			}
		}
	}

	if (!ok)
	{
		// Remove downloaded plugin package
		deleteFileOrFolder(destTo);
		return false;
	}

	return true;
}

bool getUpdateInfo(const string& info2get, const GupParameters& gupParams, const GupExtraOptions& proxyServer, const wstring& customParam, const wstring& version)
{
	char errorBuffer[CURL_ERROR_SIZE] = { 0 };

	// Check on the web the availibility of update
	// Get the update package's location
	CURL *curl;
	CURLcode res = CURLE_FAILED_INIT;

	curl = curl_easy_init();
	if (curl)
	{
		std::wstring urlComplete = gupParams.getInfoLocation() + L"?version=";
		if (!version.empty())
			urlComplete += version;
		else
			urlComplete += gupParams.getCurrentVersion();

		if (!customParam.empty())
		{
			wstring customParamPost = L"&param=";
			customParamPost += customParam;
			urlComplete += customParamPost;
		}
		else if (!gupParams.getParam().empty())
		{
			wstring customParamPost = L"&param=";
			customParamPost += gupParams.getParam();
			urlComplete += customParamPost;
		}

		curl_easy_setopt(curl, CURLOPT_URL, ws2s(urlComplete).c_str());


		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, TRUE);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, getUpdateInfoCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &info2get);

		wstring ua = gupParams.getSoftwareName();

		winGupUserAgent += VERSION_VALUE;
		if (ua != L"")
		{
			ua += L"/";
			ua += version;
			ua += L" (";
			ua += winGupUserAgent;
			ua += L")";

			winGupUserAgent = ua;
		}

		curl_easy_setopt(curl, CURLOPT_USERAGENT, ws2s(winGupUserAgent).c_str());
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

		if (proxyServer.hasProxySettings())
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, ws2s(proxyServer.getProxyServer()).c_str());
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

bool runInstaller(const wstring& app2runPath, const wstring& binWindowsClassName, const wstring& closeMsg, const wstring& closeMsgTitle)
{

	if (!binWindowsClassName.empty())
	{
		HWND h = ::FindWindowEx(NULL, NULL, binWindowsClassName.c_str(), NULL);

		if (h)
		{
			int installAnswer = ::MessageBox(NULL, closeMsg.c_str(), closeMsgTitle.c_str(), MB_YESNO);

			if (installAnswer == IDNO)
			{
				return 0;
			}
		}

		// kill all process of binary needs to be updated.
		while (h)
		{
			::SendMessage(h, WM_CLOSE, 0, 0);
			h = ::FindWindowEx(NULL, NULL, binWindowsClassName.c_str(), NULL);
		}
	}

	// execute the installer
	HINSTANCE result = ::ShellExecute(NULL, L"open", app2runPath.c_str(), L"", L".", SW_SHOW);

	if (result <= (HINSTANCE)32) // There's a problem (Don't ask me why, ask Microsoft)
	{
		return false;
	}

	return true;
}


#ifdef _DEBUG
#define WRITE_LOG(fn, suffix, log) writeLog(fn, suffix, log);
#else
#define WRITE_LOG(fn, suffix, log)
#endif

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpszCmdLine, int)
{
	/*
	{
		wstring destPath = L"C:\\tmp\\res\\TagsView";
		wstring dlDest = L"C:\\tmp\\pb\\TagsView_Npp_03beta.zip";
		bool isSuccessful = decompress(dlDest, destPath);
		if (isSuccessful)
		{
			return 0;
		}
	}
	*/
	// Debug use - stop here so we can attach this process for debugging
	//::MessageBox(NULL, L"And do something dirty to me ;)", L"Attach me!", MB_OK);

	bool isSilentMode = false;
	FILE *pFile = NULL;
	
	wstring version;
	wstring customParam;

	ParamVector params;
	parseCommandLine(lpszCmdLine, params);

	bool launchSettingsDlg = isInList(FLAG_OPTIONS, params);
	bool isVerbose = isInList(FLAG_VERBOSE, params);
	bool isHelp = isInList(FLAG_HELP, params);
	bool isCleanUp = isInList(FLAG_CLEANUP, params);
	bool isUnzip = isInList(FLAG_UUZIP, params);
	
	getParamVal('v', params, version);
	getParamVal('p', params, customParam);

	if (isHelp)
	{
		::MessageBox(NULL, MSGID_HELP, L"GUP Command Argument Help", MB_OK);
		return 0;
	}

	WRITE_LOG(L"c:\\tmp\\winup.log", L"lpszCmdLine: ", lpszCmdLine);
	
	GupExtraOptions extraOptions(L"gupOptions.xml");
	GupNativeLang nativeLang("nativeLang.xml");
	GupParameters gupParams(L"gup.xml");

	//
	// Plugins Updater
	//
	size_t nbParam = params.size();

	// uninstall plugin:
	// gup.exe -clean "appPath2Launch" "dest_folder" "fold1" "a fold2" "fold3"
	// gup.exe -clean "c:\npp\notepad++.exe" "c:\temp\" "toto" "ti ti" "tata"
	if (isCleanUp && !isUnzip) // remove only
	{
		if (nbParam < 3)
		{
			WRITE_LOG(L"c:\\tmp\\winup.log", L"-1 in plugin updater's part - if (isCleanUp && !isUnzip) // remove only: ", L"nbParam < 3");
			return -1;
		}
		wstring prog2Launch = params[0];
		wchar_t prog2LaunchDir[MAX_PATH];
		lstrcpy(prog2LaunchDir, prog2Launch.c_str());
		::PathRemoveFileSpec(prog2LaunchDir);
		wstring destPathRoot = params[1];

		// clean
		for (size_t i = 2; i < nbParam; ++i)
		{
			wstring destPath = destPathRoot;
			::PathAppend(destPath, params[i]);
			deleteFileOrFolder(destPath);
		}

		::ShellExecute(NULL, L"open", L"explorer.exe", prog2Launch.c_str(), prog2LaunchDir, SW_SHOWNORMAL);

		return 0;
	}
	
	// update:
	// gup.exe -unzip -clean "appPath2Launch" "dest_folder" "pluginFolderName1 http://pluginFolderName1/pluginFolderName1.zip sha256Hash1" "pluginFolderName2 http://pluginFolderName2/pluginFolderName2.zip sha256Hash2" "plugin Folder Name3 http://plugin_Folder_Name3/plugin_Folder_Name3.zip sha256Hash3"
	// gup.exe -unzip -clean "c:\npp\notepad++.exe" "c:\donho\notepad++\plugins" "toto http://toto/toto.zip 7c31a97b..." "ti et ti http://ti_ti/ti_ti.zip 087a0591..." "tata http://tata/tata.zip 2e9766c..."

	// Install:
	// gup.exe -unzip "appPath2Launch" "dest_folder" "pluginFolderName1 http://pluginFolderName1/pluginFolderName1.zip sha256Hash1" "pluginFolderName2 http://pluginFolderName2/pluginFolderName2.zip sha256Hash2" "plugin Folder Name3 http://plugin_Folder_Name3/plugin_Folder_Name3.zip sha256Hash3"
	// gup.exe -unzip "c:\npp\notepad++.exe" "c:\donho\notepad++\plugins" "toto http://toto/toto.zip 7c31a97b..." "ti et ti http://ti_ti/ti_ti.zip 087a0591..." "tata http://tata/tata.zip 2e9766c..."
	if (isUnzip) // update or install
	{
		if (nbParam < 3)
		{
			WRITE_LOG(L"c:\\tmp\\winup.log", L"-1 in plugin updater's part - if (isCleanUp && isUnzip) // update: ", L"nbParam < 3");
			return -1;
		}
		wstring prog2Launch = params[0];
		wchar_t prog2LaunchDir[MAX_PATH];
		lstrcpy(prog2LaunchDir, prog2Launch.c_str());
		::PathRemoveFileSpec(prog2LaunchDir);
		wstring destPathRoot = params[1];

		for (size_t i = 2; i < nbParam; ++i)
		{
			wstring destPath = destPathRoot;

			// break down param in dest folder name and download url
			auto pos = params[i].find_last_of(L" ");
			if (pos != wstring::npos && pos > 0)
			{
				wstring folder;
				wstring dlUrl;

				wstring tempStr = params[i].substr(0, pos);
				wstring sha256ToCheck = params[i].substr(pos + 1, params[i].length() - 1);
				if (sha256ToCheck.length() != 64)
					continue;

				auto pos2 = tempStr.find_last_of(L" ");
				if (pos2 != string::npos && pos2 > 0)
				{
					// 3 parts - OK
					dlUrl = tempStr.substr(pos2 + 1, tempStr.length() - 1);
					folder = tempStr.substr(0, pos2);
				}
				else
				{
					// 2 parts - error. Just pass to the next
					continue;
				}

				::PathAppend(destPath, folder);

				// Make a backup path
				wstring backup4RestoreInCaseOfFailedPath;
				if (isCleanUp) // Update
				{
					//deleteFileOrFolder(destPath);

					// check if renamed folder exist, if it does, delete it
					backup4RestoreInCaseOfFailedPath = destPath + L".backup4RestoreInCaseOfFailed";
					if (::PathFileExists(backup4RestoreInCaseOfFailedPath.c_str()))
						deleteFileOrFolder(backup4RestoreInCaseOfFailedPath);

					// rename the folder with suffix ".backup4RestoreInCaseOfFailed"
					::MoveFile(destPath.c_str(), backup4RestoreInCaseOfFailedPath.c_str());
				}

				// install
				std::wstring dlDest = _wgetenv(L"TEMP");
				dlDest += L"\\";
				dlDest += ::PathFindFileName(dlUrl.c_str());

				wchar_t *ext = ::PathFindExtension(dlDest.c_str());
				if (lstrcmp(ext, L".zip") != 0)
					dlDest += L".zip";

				dlFileName = ::PathFindFileName(dlUrl.c_str());

				wstring dlStopped = nativeLang.getMessageString("MSGID_DOWNLOADSTOPPED");
				if (dlStopped == L"")
					dlStopped = MSGID_DOWNLOADSTOPPED;

				bool isSuccessful = downloadBinary(dlUrl, dlDest, sha256ToCheck, pair<wstring, int>(extraOptions.getProxyServer(), extraOptions.getPort()), true, pair<wstring, wstring>(dlStopped, gupParams.getMessageBoxTitle()));
				if (isSuccessful)
				{
					isSuccessful = decompress(dlDest, destPath);
					if (!isSuccessful)
					{
						wstring unzipFailed = nativeLang.getMessageString("MSGID_UNZIPFAILED");
						if (unzipFailed == L"")
							unzipFailed = MSGID_UNZIPFAILED;

						::MessageBox(NULL, unzipFailed.c_str(), gupParams.getMessageBoxTitle().c_str(), MB_OK);

						// Delete incomplete unzipped folder
						deleteFileOrFolder(destPath);

						if (!backup4RestoreInCaseOfFailedPath.empty())
						{
							// rename back the folder
							::MoveFile(backup4RestoreInCaseOfFailedPath.c_str(), destPath.c_str());
						}
					}
					else
					{
						if (!backup4RestoreInCaseOfFailedPath.empty())
						{
							// delete the folder with suffix ".backup4RestoreInCaseOfFailed"
							deleteFileOrFolder(backup4RestoreInCaseOfFailedPath);
						}
					}

					// Remove downloaded zip from TEMP folder
					::DeleteFile(dlDest.c_str());
				}
				else
				{
					if (!backup4RestoreInCaseOfFailedPath.empty())
					{
						// delete the folder with suffix ".backup4RestoreInCaseOfFailed"
						::MoveFile(backup4RestoreInCaseOfFailedPath.c_str(), destPath.c_str());
					}
				}
			}
		}

		::ShellExecute(NULL, L"open", L"explorer.exe", prog2Launch.c_str(), prog2LaunchDir, SW_SHOWNORMAL);
		return 0;
	}

	//
	// Notepad++ Updater
	//
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
				extraOptions.writeProxyInfo(L"gupOptions.xml", proxySrv.c_str(), proxyPort);

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
		{
			WRITE_LOG(L"c:\\tmp\\winup.log", L"return -1 in Npp Updater part: ", L"getUpdateInfo func failed.");
			return -1;
		}

		HWND hApp = ::FindWindowEx(NULL, NULL, gupParams.getClassName().c_str(), NULL);
		bool isModal = gupParams.isMessageBoxModal();
		GupDownloadInfo gupDlInfo(updateInfo.c_str());

		if (!gupDlInfo.doesNeed2BeUpdated())
		{
			if (!isSilentMode)
			{
				UpdateCheckParams localParams{ nativeLang, gupParams };
				::DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_UPDATE_DLG), isModal ? hApp : NULL,
					reinterpret_cast<DLGPROC>(updateCheckDlgProc), reinterpret_cast<LPARAM>(&localParams));
			}
			return 0;
		}


		//
		// Process Update Info
		//

		// Ask user if he/she want to do update
		wstring updateAvailable = nativeLang.getMessageString("MSGID_UPDATEAVAILABLE");
		if (updateAvailable == L"")
			updateAvailable = MSGID_UPDATEAVAILABLE;
		
		int thirdButtonCmd = gupParams.get3rdButtonCmd();
		thirdDoUpdateDlgButtonLabel = gupParams.get3rdButtonLabel();

		int dlAnswer = 0;

		if (!thirdButtonCmd)
			dlAnswer = ::MessageBox(isModal ? hApp : NULL, updateAvailable.c_str(), gupParams.getMessageBoxTitle().c_str(), MB_YESNO);
		else
			dlAnswer = static_cast<int32_t>(::DialogBox(hInst, MAKEINTRESOURCE(IDD_YESNONEVERDLG), isModal ? hApp : NULL, reinterpret_cast<DLGPROC>(yesNoNeverDlgProc)));

		if (dlAnswer == IDNO)
		{
			return 0;
		}
		
		if (dlAnswer == IDCANCEL)
		{
			if (gupParams.getClassName() != L"")
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

		std::wstring dlDest = _wgetenv(L"TEMP");
		dlDest += L"\\";
		dlDest += ::PathFindFileName(gupDlInfo.getDownloadLocation().c_str());

        wchar_t *ext = ::PathFindExtension(gupDlInfo.getDownloadLocation().c_str());
        if (lstrcmpW(ext, L".exe") != 0)
            dlDest += L".exe";

		dlFileName = ::PathFindFileName(gupDlInfo.getDownloadLocation().c_str());


		wstring dlStopped = nativeLang.getMessageString("MSGID_DOWNLOADSTOPPED");
		if (dlStopped == L"")
			dlStopped = MSGID_DOWNLOADSTOPPED;

		bool dlSuccessful = downloadBinary(gupDlInfo.getDownloadLocation(), dlDest, L"", pair<wstring, int>(extraOptions.getProxyServer(), extraOptions.getPort()), isSilentMode, pair<wstring, wstring>(dlStopped, gupParams.getMessageBoxTitle()));

		if (!dlSuccessful)
		{
			WRITE_LOG(L"c:\\tmp\\winup.log", L"return -1 in Npp Updater part: ", L"downloadBinary func failed.");
			return -1;
		}

		//
		// Run executable bin
		//
		wstring msg = gupParams.getClassName();
		wstring closeApp = nativeLang.getMessageString("MSGID_CLOSEAPP");
		if (closeApp == L"")
			closeApp = MSGID_CLOSEAPP;
		msg += closeApp;

		runInstaller(dlDest, gupParams.getClassName(), msg, gupParams.getMessageBoxTitle().c_str());

		return 0;

	} catch (exception ex) {
		if (!isSilentMode)
			::MessageBoxA(NULL, ex.what(), "Xml Exception", MB_OK);

		if (pFile != NULL)
			fclose(pFile);

		WRITE_LOG(L"c:\\tmp\\winup.log", L"return -1 in Npp Updater part, exception: ", s2ws(ex.what()).c_str());
		return -1;
	}
	catch (...)
	{
		if (!isSilentMode)
			::MessageBoxA(NULL, "Unknown", "Unknown Exception", MB_OK);

		if (pFile != NULL)
			fclose(pFile);

		WRITE_LOG(L"c:\\tmp\\winup.log", L"return -1 in Npp Updater part, exception: ", L"Unknown Exception");
		return -1;
	}
}
