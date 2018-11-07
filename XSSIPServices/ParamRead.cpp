#include "StdAfx.h"
#include "ParamRead.h"
#include <Winbase.h>

CParamRead::CParamRead(void)
: m_sProfileName("")
, m_sDBServer("")
, m_sDBName("")
, m_sDBUid("")
, m_sDBPwd("")
, m_nDBDriver(0)
, m_sSIPServerID("")
, m_sLocalPassword("")
, m_sLocalIP("")
, m_nLocalPort(0)
, m_bRegSupServer(FALSE)
, m_bSubList(FALSE)
, m_bAuthenticate(FALSE)
, m_sSuperiorServer("")
,m_nSearchDevice(0)
,m_nSearchPlat(0)
,m_nWriteDB(0)
,m_bFileStream(0)
{
	GetDir();
}

CParamRead::~CParamRead(void)
{
}

string CParamRead::GetDir()
{
	DWORD nBufferLenth = MAX_PATH;
	char szBuffer[MAX_PATH] = {0};
	DWORD dwRet = GetModuleFileNameA(NULL, szBuffer, nBufferLenth);
	if (dwRet == 0)
	{
		return false;
	}
	char *sPath = strrchr(szBuffer, '\\');
	memset(sPath, 0, strlen(sPath));
    m_sPath = szBuffer;
	return m_sPath;
}

void CParamRead::ReadParameter(string sSection, string sEntry, string &sValue, string sFileName)
{
	char szReturnedString[2048] = {0};
	GetPrivateProfileStringA(sSection.c_str(), sEntry.c_str(), "", szReturnedString, sizeof(szReturnedString), m_sProfileName.c_str());
	sValue = szReturnedString;
}

void CParamRead::ReadParameter(string sSection, string sEntry, int &nValue, string sFileName)
{
	nValue = GetPrivateProfileIntA(sSection.c_str(), sEntry.c_str(), 0, m_sProfileName.c_str());
}

bool CParamRead::InitParam()
{
    m_sProfileName = m_sPath + "\\Config\\Profile.ini";
    ReadParameter("XSSIPSERVER", "DBServer", m_sDBServer);
	ReadParameter("XSSIPSERVER", "DBName", m_sDBName);
	ReadParameter("XSSIPSERVER", "DBUserID", m_sDBUid);
	ReadParameter("XSSIPSERVER", "DBPassWord", m_sDBPwd);
	ReadParameter("XSSIPSERVER", "DBDriver", m_nDBDriver);

    //龙岗数据库信息
    ReadParameter("XSSIPSERVER", "LGDBServer", m_sLGDBServer);
    ReadParameter("XSSIPSERVER", "LGDBName", m_sLGDBName);
    ReadParameter("XSSIPSERVER", "LGDBUserID", m_sLGDBUid);
    ReadParameter("XSSIPSERVER", "LGDBPassWord", m_sLGDBPwd);

	ReadParameter("XSSIPSERVER", "SIPServerID", m_sSIPServerID);
	ReadParameter("XSSIPSERVER", "Password", m_sLocalPassword);
	ReadParameter("XSSIPSERVER", "LocalIP", m_sLocalIP);
	ReadParameter("XSSIPSERVER", "LocalPort", m_nLocalPort);

	ReadParameter("XSSIPSERVER", "Authenticate", m_bAuthenticate);
	ReadParameter("XSSIPSERVER", "SubscribeList", m_bSubList);

    ReadParameter("XSSIPSERVER", "SearchDevice", m_nSearchDevice);
    ReadParameter("XSSIPSERVER", "SearchPlat", m_nSearchPlat);
    ReadParameter("XSSIPSERVER", "WriteDB", m_nWriteDB);

    ReadParameter("XSSIPSERVER", "FileStream", m_bFileStream);

    ReadParameter("XSSIPSERVER", "ISINITIAL", m_bISINITIAL);
    ReadParameter("XSSIPSERVER", "INITIALTIME", m_nInitialTime);

    ReadParameter("XSSIPSERVER", "AssignLowServer", m_sAssignLowServer);
    ReadParameter("XSSIPSERVER", "AssignRTPServer", m_sAssignRTPServer);
    ReadParameter("XSSIPSERVER", "NetType", m_nNetType);
    if(m_nNetType == 0)
    {
        m_nNetType = 1;
    }

    ReadParameter("XSSIPSUPERIORSERVER", "Register", m_bRegSupServer);
    ReadParameter("XSSIPSUPERIORSERVER", "SuperiorServer", m_sSuperiorServer);
    ReadParameter("XSSIPSUPERIORSERVER", "RegUsername", m_sRegSupUsername);
    if(m_sRegSupUsername == "")
    {
        m_sRegSupUsername = m_sSIPServerID;
    }
    ReadParameter("XSSIPSUPERIORSERVER", "RegPassword", m_sRegSupPassword);
    if(m_sRegSupPassword == "")
    {
        m_sRegSupPassword = m_sLocalPassword;
    }

	if (m_sDBServer=="" || m_sDBName=="" || m_sDBUid=="" || m_sDBPwd=="" || m_nDBDriver==0 ||
		m_sSIPServerID=="" || m_sLocalPassword=="" || m_sLocalIP=="" || m_nLocalPort==0 )
	{
		return false;
	}
	return true;
}
