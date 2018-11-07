#pragma once
#include <string>
using namespace std;

class CParamRead
{
public:
	CParamRead(void);
public:
	~CParamRead(void);
public:
	bool InitParam();
	string GetDir();
	void ReadParameter(string sSection, string sEntry, string &sValue, string sFileName = "");
	void ReadParameter(string sSection, string sEntry, int &nValue, string sFileName = "");
	bool ReadParameter(string sSection, string sEntry, void * pValue, int nSize, string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, string sValue,  string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, int iValue,  string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, void * pValue, int nSize, string sFileName = "");
public:
    string m_sPath;         //程序路径
	string m_sProfileName;	//配置路径

	string m_sDBServer;		//数据库服务名
	string m_sDBName;		//数据库名
	string m_sDBUid;		//用户名
	string m_sDBPwd;		//用户密码
	int m_nDBDriver;		//数据库类型

    //龙岗数据库信息
    string m_sLGDBServer;	//数据库服务名
    string m_sLGDBName;		//数据库名
    string m_sLGDBUid;		//用户名
    string m_sLGDBPwd;		//用户密码
    //龙岗

    string m_sSIPServerID;
    string m_sLocalPassword;
    string m_sLocalIP;
    int m_nLocalPort;

	BOOL m_bAuthenticate;	//是否要认证
	BOOL m_bSubList;		//是否向下级订阅目录		

    int m_nSearchDevice;   //是否查询下级设备目录
    int m_nSearchPlat;     //是否查询下级平台目录
    int m_nWriteDB;        //是否将目录信息写入数据库

    int m_bFileStream;      //是否启用文件流媒体
    int m_bISINITIAL;       //是否启用守望功能
    int m_nInitialTime;     //守望定时时间(S)

    string m_sAssignLowServer;
    string m_sAssignRTPServer;
    int m_nNetType;

	string m_sSuperiorServer;		//SIP代理服务地址
    BOOL m_bRegSupServer;		//是否要向上级注册
    string m_sRegSupUsername;   //向上级注册用户名
    string m_sRegSupPassword;   //向上级注册密码


    

};
