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

using namespace std;

GupParameters::GupParameters(const char * xmlFileName)
{
	_xmlDoc.LoadFile(xmlFileName);

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
				_currentVersion = val;
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
				_param = val;
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
	
	_infoUrl = iuVal;

	TiXmlNode *classeNameNode = root->FirstChildElement("ClassName2Close");
	if (classeNameNode)
	{
		TiXmlNode *n = classeNameNode->FirstChild();
		if (n)
		{
			const char *val = n->Value();
			if (val)
			{
				_className2Close = val;
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
				_messageBoxTitle = valStr;
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
			_3rdButton_label = extraCmdLabel;
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
				_softwareName = uaVal;
		}
	}

	TiXmlNode *softwareIconNode = root->FirstChildElement("SoftwareIcon");
	if (softwareIconNode)
	{
		TiXmlNode *un = softwareIconNode->FirstChild();
		if (un)
		{
			const char *uaVal = un->Value();
			if (uaVal)
				_softwareIcon = uaVal;
		}
	}
}

GupDownloadInfo::GupDownloadInfo(const char * xmlString) : _updateVersion(""), _updateLocation("")
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
					_updateVersion = val;
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
		
		_updateLocation = locVal;
	}
}

GupExtraOptions::GupExtraOptions(const char * xmlFileName) : _proxyServer(""), _port(-1)//, _hasProxySettings(false)
{
	_xmlDoc.LoadFile(xmlFileName);

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
					_proxyServer = val;
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

void GupExtraOptions::writeProxyInfo(const char *fn, const char *proxySrv, long port)
{
	TiXmlDocument newProxySettings(fn);
	TiXmlNode *root = newProxySettings.InsertEndChild(TiXmlElement("GUPOptions"));
	TiXmlNode *proxy = root->InsertEndChild(TiXmlElement("Proxy"));
	TiXmlNode *server = proxy->InsertEndChild(TiXmlElement("server"));
	server->InsertEndChild(TiXmlText(proxySrv));
	TiXmlNode *portNode = proxy->InsertEndChild(TiXmlElement("port"));
	char portStr[10];
	sprintf(portStr, "%d", port);
	portNode->InsertEndChild(TiXmlText(portStr));

	newProxySettings.SaveFile();
}

std::string GupNativeLang::getMessageString(std::string msgID)
{
	if (!_nativeLangRoot)
		return "";

	TiXmlNode *popupMessagesNode = _nativeLangRoot->FirstChildElement("PopupMessages");
	if (!popupMessagesNode)
		return "";

	TiXmlNode *node = popupMessagesNode->FirstChildElement(msgID.c_str());
	if (!node)
		return "";

	TiXmlNode *sn = node->FirstChild();
	if (!sn)
		return "";
		
	const char *val = sn->Value();
	if (!val || !(*val))
		return "";
	
	return val;
}
