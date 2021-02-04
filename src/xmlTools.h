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

#include "tinyxml.h"
#include <string>

std::wstring s2ws(const std::string& str);
std::string ws2s(const std::wstring& wstr);

class XMLTool {

protected:
	TiXmlDocument _xmlDoc;
};

class GupParameters : public XMLTool {
public:
	GupParameters() {};
	GupParameters(const wchar_t * xmlFileName);
	
	const std::wstring & getCurrentVersion() const { return _currentVersion;};
	const std::wstring & getParam() const { return _param; };
	const std::wstring & getInfoLocation() const {return _infoUrl;};
	const std::wstring & getClassName() const {return _className2Close;};
	const std::wstring & getMessageBoxTitle() const {return _messageBoxTitle;};
	const std::wstring & getSoftwareName() const {return _softwareName;};
	int get3rdButtonCmd() const {return _3rdButton_wm_cmd;};
	int get3rdButtonWparam() const {return _3rdButton_wParam;};
	int get3rdButtonLparam() const {return _3rdButton_lParam;};
	const std::wstring & get3rdButtonLabel() const { return _3rdButton_label; };

	void setCurrentVersion(const wchar_t *currentVersion) {_currentVersion = currentVersion;};
	bool setSilentMode(bool mode) {
		bool oldMode = _isSilentMode;
		_isSilentMode = mode;
		return oldMode;
	};
	bool isSilentMode() const {return _isSilentMode;};
	bool isMessageBoxModal() const { return _isMessageBoxModal; };

private:
	std::wstring _currentVersion;
	std::wstring _param;
	std::wstring _infoUrl;
	std::wstring _className2Close;
	std::wstring _messageBoxTitle;
	std::wstring _softwareName;
	bool _isMessageBoxModal = false;
	int _3rdButton_wm_cmd = 0;
	int _3rdButton_wParam = 0;
	int _3rdButton_lParam = 0;
	std::wstring _3rdButton_label;
	bool _isSilentMode = true;
};

class GupExtraOptions : public XMLTool {
public:
	GupExtraOptions(const wchar_t * xmlFileName);
	const std::wstring & getProxyServer() const { return _proxyServer;};
	long getPort() const { return _port;};
	bool hasProxySettings() const {return ((!_proxyServer.empty()) && (_port != -1));};
	void writeProxyInfo(const wchar_t *fn, const wchar_t *proxySrv, long port);

private:
	std::wstring _proxyServer;
	long _port = -1;
};

class GupDownloadInfo : public XMLTool {
public:
	GupDownloadInfo() {};
	GupDownloadInfo(const char* xmlString);
	
	const std::wstring & getVersion() const { return _updateVersion;};
	const std::wstring & getDownloadLocation() const {return _updateLocation;};
	bool doesNeed2BeUpdated() const {return _need2BeUpdated;};

private:
	bool _need2BeUpdated;
	std::wstring _updateVersion;
	std::wstring _updateLocation;
};

class GupNativeLang : public XMLTool {
public:
	GupNativeLang(const char * xmlFileName) {
		_xmlDoc.LoadFile(xmlFileName);
		_nativeLangRoot = _xmlDoc.FirstChild("GUP_NativeLangue");
	};
	std::wstring getMessageString(std::string msgID);

protected:
	TiXmlNode *_nativeLangRoot;
};
