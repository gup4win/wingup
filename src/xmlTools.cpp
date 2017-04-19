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
	tinyxml2::XMLError ErrLoadFile = _xmlDoc.LoadFile(xmlFileName);
	if(ErrLoadFile != tinyxml2::XMLError::XML_SUCCESS)
	{
		throw exception("File gup.xml not found.");
	}

	const tinyxml2::XMLNode *root = _xmlDoc.FirstChildElement("GUPInput");
	if (!root)
		throw exception("It's not a valid GUP input xml.");

	const tinyxml2::XMLNode *versionNode = root->FirstChildElement("Version");
	if (versionNode)
	{
		const tinyxml2::XMLNode *n = versionNode->FirstChild();
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
	
	const tinyxml2::XMLNode *infoURLNode = root->FirstChildElement("InfoUrl");
	if (!infoURLNode)
		throw exception("InfoUrl node is missed.");

	const tinyxml2::XMLNode *iu = infoURLNode->FirstChild();
	if (!iu)
		throw exception("InfoUrl is missed.");
		
	const char *iuVal = iu->Value();
	if (!iuVal || !(*iuVal))
		throw exception("InfoUrl is missed.");
	
	_infoUrl = iuVal;

	const tinyxml2::XMLNode *classeNameNode = root->FirstChildElement("ClassName2Close");
	if (classeNameNode)
	{
		const tinyxml2::XMLNode *n = classeNameNode->FirstChild();
		if (n)
		{
			const char *val = n->Value();
			if (val)
			{
				_className2Close = val;
			}
		}
	}

	const tinyxml2::XMLNode *progNameNode = root->FirstChildElement("MessageBoxTitle");
	if (progNameNode)
	{
		const tinyxml2::XMLNode *n = progNameNode->FirstChild();
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
		tinyxml2::XMLError ErrReturn = (progNameNode->ToElement())->QueryIntAttribute("extraCmd", &val);
		if (ErrReturn == tinyxml2::XML_SUCCESS)
		{
			_3rdButton_wm_cmd = val;
		}
		
		ErrReturn = (progNameNode->ToElement())->QueryIntAttribute("ecWparam", &val);
		if (ErrReturn == tinyxml2::XML_SUCCESS)
		{
			_3rdButton_wParam = val;
		}
		
		ErrReturn = (progNameNode->ToElement())->QueryIntAttribute("ecLparam", &val);
		if (ErrReturn == tinyxml2::XML_SUCCESS)
		{
			_3rdButton_lParam = val;
		}

		const char * extraCmdLabel = (progNameNode->ToElement())->Attribute("extraCmdButtonLabel");
		if (extraCmdLabel != NULL)
		{
			_3rdButton_label = extraCmdLabel;
		}
	}

	const tinyxml2::XMLNode *silentModeNode = root->FirstChildElement("SilentMode");
	if (silentModeNode)
	{
		const tinyxml2::XMLNode *smn = silentModeNode->FirstChild();
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
	const tinyxml2::XMLNode *userAgentNode = root->FirstChildElement("SoftwareName");
	if (userAgentNode)
	{
		const tinyxml2::XMLNode *un = userAgentNode->FirstChild();
		if (un)
		{
			const char *uaVal = un->Value();
			if (uaVal)
				_softwareName = uaVal;
		}
	}
}

GupDownloadInfo::GupDownloadInfo(const char * xmlString) : _updateVersion(""), _updateLocation("")
{
	
	tinyxml2::XMLError ErrParse = _xmlDoc.Parse(xmlString);
	if (ErrParse != tinyxml2::XMLError::XML_SUCCESS)
	{
		throw exception("DownloadInfo xml parser error.");
	}

	const tinyxml2::XMLNode *root = _xmlDoc.FirstChildElement("GUP");
	if (!root)
		throw exception("It's not a valid GUP xml.");

	const tinyxml2::XMLNode *needUpdateNode = root->FirstChildElement("NeedToBeUpdated");
	if (!needUpdateNode)
		throw exception("NeedToBeUpdated node is missed.");

	const tinyxml2::XMLNode *nun = needUpdateNode->FirstChild();
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
		const tinyxml2::XMLNode *versionNode = root->FirstChildElement("Version");
		if (versionNode)
		{
			const tinyxml2::XMLNode *n = versionNode->FirstChild();
			if (n)
			{
				const char *val = n->Value();
				if (val)
				{
					_updateVersion = val;
				}
			}
		}
		
		const tinyxml2::XMLNode *locationNode = root->FirstChildElement("Location");
		if (!locationNode)
			throw exception("Location node is missed.");

		const tinyxml2::XMLNode *ln = locationNode->FirstChild();
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
	tinyxml2::XMLError ErrLoadFile = _xmlDoc.LoadFile(xmlFileName);
	if (ErrLoadFile != tinyxml2::XMLError::XML_SUCCESS)
	{
		//GUPOptions.xml is just an optional file, so this is not necessarily an error
		return;
	}

	const tinyxml2::XMLNode *root = _xmlDoc.FirstChildElement("GUPOptions");
	if (!root)
		return;
		
	const tinyxml2::XMLNode *proxyNode = root->FirstChildElement("Proxy");
	if (proxyNode)
	{
		const tinyxml2::XMLNode *serverNode = proxyNode->FirstChildElement("server");
		if (serverNode)
		{
			const tinyxml2::XMLNode *server = serverNode->FirstChild();
			if (server)
			{
				const char *val = server->Value();
				if (val)
					_proxyServer = val;
			}
		}

		const tinyxml2::XMLNode *portNode = proxyNode->FirstChildElement("port");
		if (portNode)
		{
			const tinyxml2::XMLNode *port = portNode->FirstChild();
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
	tinyxml2::XMLDocument newProxySettings;
	tinyxml2::XMLNode *root = newProxySettings.InsertEndChild(newProxySettings.NewElement( "GUPOptions" ));
	tinyxml2::XMLNode *proxy = root->InsertEndChild(newProxySettings.NewElement( "Proxy" ));
	tinyxml2::XMLNode *server = proxy->InsertEndChild(newProxySettings.NewElement( "server" ));
	server->InsertEndChild(newProxySettings.NewText(proxySrv));
	tinyxml2::XMLNode *portNode = proxy->InsertEndChild(newProxySettings.NewElement( "port" ));
	char portStr[10];
	sprintf(portStr, "%d", port);
	portNode->InsertEndChild(newProxySettings.NewText(portStr));

	tinyxml2::XMLError ErrSaveFile = newProxySettings.SaveFile(fn);
	if (ErrSaveFile != tinyxml2::XMLError::XML_SUCCESS)
	{
		throw exception("Proxy settings can't be saved to GUPOptions.xml .");
		return;
	}
}

std::string GupNativeLang::getMessageString(std::string msgID)
{
	if (!_nativeLangRoot)
		return "";

	const tinyxml2::XMLNode *popupMessagesNode = _nativeLangRoot->FirstChildElement("PopupMessages");
	if (!popupMessagesNode)
		return "";

	const tinyxml2::XMLNode *node = popupMessagesNode->FirstChildElement(msgID.c_str());
	if (!node)
		return "";

	const tinyxml2::XMLNode *sn = node->FirstChild();
	if (!sn)
		return "";
		
	const char *val = sn->Value();
	if (!val || !(*val))
		return "";
	
	return val;
}
