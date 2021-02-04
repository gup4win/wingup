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

#include "xmlTools.h"
#include <locale>
#include <codecvt>

using namespace std;


wstring s2ws(const string& str)
{
	using convert_typeX = codecvt_utf8<wchar_t>;
	wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.from_bytes(str);
}

string ws2s(const wstring& wstr)
{
	using convert_typeX = codecvt_utf8<wchar_t>;
	wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

GupParameters::GupParameters(const wchar_t * xmlFileName)
{
	_xmlDoc.LoadFile(ws2s(xmlFileName).c_str());

	TiXmlNode *root = _xmlDoc.FirstChild("GUPInput");
	if (!root)
		throw exception("It's not a valid GUP input xml.");

	TiXmlNode *versionNode = root->FirstChildElement("Version");
	if (versionNode)
	{
		TiXmlNode *n = versionNode->FirstChild();
		if (n)
		{
			const char *val = n->Value();
			if (val)
			{
				_currentVersion = s2ws(val);
			}
		}
	}

	TiXmlNode *paramNode = root->FirstChildElement("Param");
	if (paramNode)
	{
		TiXmlNode *n = paramNode->FirstChild();
		if (n)
		{
			const char *val = n->Value();
			if (val)
			{
				_param = s2ws(val);
			}
		}
	}
	
	TiXmlNode *infoURLNode = root->FirstChildElement("InfoUrl");
	if (!infoURLNode)
		throw exception("InfoUrl node is missed.");

	TiXmlNode *iu = infoURLNode->FirstChild();
	if (!iu)
		throw exception("InfoUrl is missed.");
		
	const char *iuVal = iu->Value();
	if (!iuVal || !(*iuVal))
		throw exception("InfoUrl is missed.");
	
	_infoUrl = s2ws(iuVal);

	TiXmlNode *classeNameNode = root->FirstChildElement("ClassName2Close");
	if (classeNameNode)
	{
		TiXmlNode *n = classeNameNode->FirstChild();
		if (n)
		{
			const char *val = n->Value();
			if (val)
			{
				_className2Close = s2ws(val);
			}
		}
	}

	TiXmlNode *progNameNode = root->FirstChildElement("MessageBoxTitle");
	if (progNameNode)
	{
		TiXmlNode *n = progNameNode->FirstChild();
        const char *valStr = NULL;

		if (n)
		{
			valStr = n->Value();
			if (valStr)
			{
				_messageBoxTitle = s2ws(valStr);
			}
		}

		valStr = (progNameNode->ToElement())->Attribute("isModal");
		if (valStr)
		{
			if (stricmp(valStr, "yes") == 0)
				_isMessageBoxModal = true;
			else if (stricmp(valStr, "no") == 0)
				_isMessageBoxModal = false;
			else
				throw exception("isModal value is incorrect (only \"yes\" or \"no\" is allowed).");
		}

        int val = 0;
		valStr = (progNameNode->ToElement())->Attribute("extraCmd", &val);
		if (valStr)
		{
			_3rdButton_wm_cmd = val;
		}
		
		valStr = (progNameNode->ToElement())->Attribute("ecWparam", &val);
		if (valStr)
		{
			_3rdButton_wParam = val;
		}
		
		valStr = (progNameNode->ToElement())->Attribute("ecLparam", &val);
		if (valStr)
		{
			_3rdButton_lParam = val;
		}

		const char * extraCmdLabel = (progNameNode->ToElement())->Attribute("extraCmdButtonLabel");
		if (extraCmdLabel != NULL)
		{
			_3rdButton_label = s2ws(extraCmdLabel);
		}
	}

	TiXmlNode *silentModeNode = root->FirstChildElement("SilentMode");
	if (silentModeNode)
	{
		TiXmlNode *smn = silentModeNode->FirstChild();
		if (smn)
		{
			const char *smnVal = smn->Value();
			if (smnVal && *smnVal)
			{
				if (stricmp(smnVal, "yes") == 0)
					_isSilentMode = true;
				else if (stricmp(smnVal, "no") == 0)
					_isSilentMode = false;
				else
					throw exception("SilentMode value is incorrect (only \"yes\" or \"no\" is allowed).");
			}
		}
	}

	
	//
	// Get optional parameters
	//
	TiXmlNode *userAgentNode = root->FirstChildElement("SoftwareName");
	if (userAgentNode)
	{
		TiXmlNode *un = userAgentNode->FirstChild();
		if (un)
		{
			const char *uaVal = un->Value();
			if (uaVal)
				_softwareName = s2ws(uaVal);
		}
	}
}

GupDownloadInfo::GupDownloadInfo(const char* xmlString)
{
	_xmlDoc.Parse(xmlString);

	TiXmlNode *root = _xmlDoc.FirstChild("GUP");
	if (!root)
		throw exception("It's not a valid GUP xml.");

	TiXmlNode *needUpdateNode = root->FirstChildElement("NeedToBeUpdated");
	if (!needUpdateNode)
		throw exception("NeedToBeUpdated node is missed.");

	TiXmlNode *nun = needUpdateNode->FirstChild();
	if (!nun)
		throw exception("NeedToBeUpdated is missed.");
		
	const char *nunVal = nun->Value();
	if (!nunVal || !(*nunVal))
		throw exception("NeedToBeUpdated is missed.");
	
	if (stricmp(nunVal, "yes") == 0)
		_need2BeUpdated = true;
	else if (stricmp(nunVal, "no") == 0)
		_need2BeUpdated = false;
	else
		throw exception("NeedToBeUpdated value is incorrect (only \"yes\" or \"no\" is allowed).");

	if (_need2BeUpdated)
	{
		//
		// Get mandatory parameters
		//
		TiXmlNode *versionNode = root->FirstChildElement("Version");
		if (versionNode)
		{
			TiXmlNode *n = versionNode->FirstChild();
			if (n)
			{
				const char *val = n->Value();
				if (val)
				{
					_updateVersion = s2ws(val);
				}
			}
		}
		
		TiXmlNode *locationNode = root->FirstChildElement("Location");
		if (!locationNode)
			throw exception("Location node is missed.");

		TiXmlNode *ln = locationNode->FirstChild();
		if (!ln)
			throw exception("Location is missed.");
			
		const char *locVal = ln->Value();
		if (!locVal || !(*locVal))
			throw exception("Location is missed.");
		
		_updateLocation = s2ws(locVal);
	}
}

GupExtraOptions::GupExtraOptions(const wchar_t * xmlFileName)
{
	_xmlDoc.LoadFile(ws2s(xmlFileName).c_str());

	TiXmlNode *root = _xmlDoc.FirstChild("GUPOptions");
	if (!root)
		return;
		
	TiXmlNode *proxyNode = root->FirstChildElement("Proxy");
	if (proxyNode)
	{
		TiXmlNode *serverNode = proxyNode->FirstChildElement("server");
		if (serverNode)
		{
			TiXmlNode *server = serverNode->FirstChild();
			if (server)
			{
				const char *val = server->Value();
				if (val)
					_proxyServer = s2ws(val);
			}
		}

		TiXmlNode *portNode = proxyNode->FirstChildElement("port");
		if (portNode)
		{
			TiXmlNode *port = portNode->FirstChild();
			if (port)
			{
				const char *val = port->Value();
				if (val)
					_port = atoi(val);
			}
		}
	}
}

void GupExtraOptions::writeProxyInfo(const wchar_t* fn, const wchar_t* proxySrv, long port)
{
	TiXmlDocument newProxySettings(ws2s(fn).c_str());
	TiXmlNode *root = newProxySettings.InsertEndChild(TiXmlElement("GUPOptions"));
	TiXmlNode *proxy = root->InsertEndChild(TiXmlElement("Proxy"));
	TiXmlNode *server = proxy->InsertEndChild(TiXmlElement("server"));
	server->InsertEndChild(TiXmlText(ws2s(proxySrv).c_str()));
	TiXmlNode *portNode = proxy->InsertEndChild(TiXmlElement("port"));
	char portStr[10];
	sprintf(portStr, "%d", port);
	portNode->InsertEndChild(TiXmlText(portStr));

	newProxySettings.SaveFile();
}

std::wstring GupNativeLang::getMessageString(std::string msgID)
{
	if (!_nativeLangRoot)
		return L"";

	TiXmlNode *popupMessagesNode = _nativeLangRoot->FirstChildElement("PopupMessages");
	if (!popupMessagesNode)
		return L"";

	TiXmlNode *node = popupMessagesNode->FirstChildElement(msgID.c_str());
	if (!node)
		return L"";

	TiXmlElement* element = node->ToElement();

	const char *content = element->Attribute("content");
	if (content)
		return s2ws(content);

	return L"";
}
