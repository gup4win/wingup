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

using namespace std;

class XMLTool {

protected:
	TiXmlDocument _xmlDoc;
};

class GupParameters : public XMLTool {
public:
	GupParameters() : _currentVersion(""), _className2Close(""), _isSilentMode(true) {};
	GupParameters(const char * xmlFileName);
	
	const string & getCurrentVersion() const { return _currentVersion;};
	const string & getInfoLocation() const {return _infoUrl;};
	const string & getClassName() const {return _className2Close;};
	const string & getMessageBoxTitle() const {return _messageBoxTitle;};
	int get3rdButtonCmd() const {return _3rdButton_wm_cmd;};
	int get3rdButtonWparam() const {return _3rdButton_wParam;};
	int get3rdButtonLparam() const {return _3rdButton_lParam;};

	void setCurrentVersion(const char *currentVersion) {_currentVersion = currentVersion;};
	bool setSilentMode(bool mode) {
		bool oldMode = _isSilentMode;
		_isSilentMode = mode;
		return mode;
	};
	bool isSilentMode() const {return _isSilentMode;};

private:
	string _currentVersion;
	string _infoUrl;
	string _className2Close;
	string _messageBoxTitle;
	int _3rdButton_wm_cmd;
	int _3rdButton_wParam;
	int _3rdButton_lParam;
	bool _isSilentMode;
};

class GupExtraOptions : public XMLTool {
public:
	GupExtraOptions(const char * xmlFileName);
	const string & getProxyServer() const { return _proxyServer;};
	long getPort() const { return _port;};
	bool hasProxySettings(){return ((_proxyServer != "") && (_port != -1));};
	void writeProxyInfo(const char *fn, const char *proxySrv, long port);

private:
	string _proxyServer;
	long _port;
	//bool _hasProxySettings;
};

class GupDownloadInfo : public XMLTool {
public:
	GupDownloadInfo() : _updateVersion(""), _updateLocation("") {};
	GupDownloadInfo(const char * xmlString);
	
	const string & getVersion() const { return _updateVersion;};
	const string & getDownloadLocation() const {return _updateLocation;};
	bool doesNeed2BeUpdated() const {return _need2BeUpdated;};

private:
	bool _need2BeUpdated;
	string _updateVersion;
	string _updateLocation;
};

class GupNativeLang : public XMLTool {
public:
	GupNativeLang(const char * xmlFileName) {
		_xmlDoc.LoadFile(xmlFileName);
		_nativeLangRoot = _xmlDoc.FirstChild("GUP_NativeLangue");
	};
	string getMessageString(string msgID);

protected:
	TiXmlNode *_nativeLangRoot;
};
