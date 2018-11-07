#pragma once
#include "XSIP/xsip.h"
#include "ParamRead.h"
#include "DataBaseMgr.h"
#include "tinyxml\tinyxml.h"
#include "SIPProtocol.h"
#include "LogRecorder.h"
#include "RTPServerManager.h"
#include <map>
#include <list>
#include <algorithm>
#include <time.h>
#pragma warning (disable:4819)

#define PTZ_LOCK_SUCCESSED     100    //锁定成功
#define PTZ_UNLOCK_SUCCESSED   101    //解锁成功
#define PTZ_CONTROL_SUCCESSED  102    //控制成功
#define PTZ_LOCK_FAILED_LOCKED 103    //锁定失败：当前用户已经锁定云台，无需再次锁定
#define PTZ_LOCK_FAILED_LOW    104    //锁定失败：当前云台已被其他高权限用户锁定，无法锁定
#define PTZ_UNLOCK_FAILED      105    //当前用户没有锁定云台，解锁失败
#define PTZ_CONTROL_FAILED     106

#define MAX_STRING_LEN			1024  //数组最大长度
#define KEEPALIVENOANS      5   //向上级发送心跳信息无回应最大次数, 超过后重新注册.

enum CATLOGNOTIFYTYPE
{
    NONOCATALOG,
    ADDCATALOG,
    DELCATALOG,
    UPDATECATALOG
};

typedef struct _ChannelInfo
{
    int nChanID;        
    char pSIPCameraCode[64];
    char pChanno[64];
    char pName[64];
    int nChannel;
    bool bOnline;
    _ChannelInfo()
    {
        nChanID = 0;
        ZeroMemory(pSIPCameraCode, sizeof(pSIPCameraCode));
        ZeroMemory(pChanno, sizeof(pChanno));
        ZeroMemory(pName, sizeof(pName));
        nChannel = 0;
        bOnline = true;
    }
}CHANNELINFO, *LPCHANNELINFO;

typedef struct _DVRInfo		//设备地址信息
{
    int nNetType;       //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK接入; 5: RTSP; 6: ONVIF; 7:播放库取流
    int nTranscoding;   //0: 不转码; 1: 转码
    char pDVRIP[20];
    int nDVRSDKPort;
    int nDVRSIPPort;
    char pUserName[64];
    char pPassword[64];
    int nDVRType;
    char pDVRKey[20];

    int nSIPServerID;
    char pSIPServerCode[64];
    char pDeviceCode[64];
	char pDeviceUri[64];
	bool	bReg;
	time_t  tRegistTime;   //最后注册时间或收到心跳时间
    int nNoAnsKeepalive;
    bool bSendCatalog;      //是否给下级发送目录推送消息, 在服务启动时读取配置文件或输入命令"M", 在发送一次后置false
    map<string, LPCHANNELINFO> m_mapChannelInfo;
    _DVRInfo()
	{
        nNetType = 1;
        nTranscoding = 0;
        ZeroMemory(pDVRKey, sizeof(pDVRKey));
        ZeroMemory(pDVRIP, sizeof(pDVRIP));
        ZeroMemory(pUserName, sizeof(pUserName));
        ZeroMemory(pPassword, sizeof(pPassword));
        nDVRSDKPort = 0;
        nDVRSIPPort = 0;
        nDVRType = 1;

        nSIPServerID = 0;
        ZeroMemory(pSIPServerCode, sizeof(pSIPServerCode));
        ZeroMemory(pDeviceCode, sizeof(pDeviceCode));
        ZeroMemory(pDeviceUri, sizeof(pDeviceUri));
        bReg = false;
		tRegistTime = 0;
        nNoAnsKeepalive = 0;
        bSendCatalog = false;
	}
}DVRINFO, *LPDVRINFO;

typedef struct _CameraInfo
{
    int nChanID;
    string sCameraID;   //镜头编码
    string sCameraName; 
    int nCameraType;    //镜头接入类型, 1 HK, 2 DH, 20 SIP
    string sDVRKey;

    //国标接入信息
    string sSIPIP;          //镜头所属设备或平台IP
    int nSIPPort;           //镜头所属设备或平台端口
    string sDeviceCode;     //镜头所属设备或平台编码
    string sServerCode;     //镜头所属SIP服务编码

    //SDK接入信息
    string sDeviceIP;   //设备IP(IPC IP, 或镜头父设备(DVR|NVR)IP
    int nDevicePort;    //设备sdk连接端口
    string sDeviceUsername;
    string sDevicePassword;
    int nChannel;

    int nNetType;           //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK接入; 5: RTSP; 6: ONVIF; 7: 播放库取流
    int nTranscoding;       //0: 不转码; 1: 转码
    _CameraInfo()
    {
        nChanID = 0;
        nCameraType = 20;
        nDevicePort = 0;
        nChannel = 0;
        nNetType = 0;
    }
}CAMERAINFO, *LPCAMERAINFO;

typedef struct SessionInfo
{
	string sTitle;	//消息名
	string sAddr;	//来源地址编码
	string dwFTag;	//来源标识
	string dwTTag;	//目标标识
	string sDeviceID;
	string request;
	string response;
    bool bSendResponse;  //是否己发送回应, 在收到重复消息时, 直接再次发送response, 不用再处理.
	int    nNum;        //镜头目录或录像信息数目计数
	SOCKADDR_IN sockAddr;
    time_t    recvTime;
    string sSN;        //XML字段中的SN
    int nNetType;       //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK接入; 5: RTSP; 6: ONVIF
	SessionInfo()
	{
        nNetType = 1;
		nNum = 0;
        recvTime = 0;
        bSendResponse = false;
		memset(&sockAddr, 0, sizeof(SOCKADDR_IN));
        sSN = "";
	}
}SESSIONINFO, *LPSESSIONINFO;

typedef struct InitialCameraInfo
{
    string sLowServerIP;   //镜头所属的当前SIP服务的下级SIP服务的IP
    int nLowServerPort;
    time_t tLastControlTime; // 最后一次控制云台的时间
}INITIALCAMERAINFO;

struct LOCKINFO
{
	string sUserID;   
	int    nUserLevel;
};
struct SIPServerInfo
{
    int nServerID;
    string sSIPServerCode;
    string sParentCode;
    string sPoliceNetIP;
    int nPoliceNetPort;
    string sExteranlIP;
    int nExteranlPort;
    SIPServerInfo()
    {
        nServerID = 0;
        nPoliceNetPort  = 0;
        nExteranlPort = 0;
    }
};

typedef struct _SmartUser  //接收smart报警用户信息
{
    string sUserID;
    string sUserUri;
}SMARTUSER, *LPSMARTUSER;

typedef struct _CatalogInfo
{
    SOCKADDR_IN addr;
    string sBody;
    string sDeviceID;
}CATALOGINFO, *LPCATALOGINFO;

typedef map<string, LPCHANNELINFO> CHANNELINFOMAP;
typedef map<string, LPDVRINFO> DVRINFOMAP;
typedef map<string, SESSIONINFO> SessionMap;
typedef map<string, LOCKINFO> LockMap; //锁定map
typedef map<string , SIPServerInfo>SIPServerMap;
typedef map<string, SMARTUSER> SmartUserMap;
typedef list<LPCATALOGINFO> ListCatalogInfo;

class CSIPServer :public XSIP
{
public:
	CSIPServer(void);
public:
	~CSIPServer(void);
public:
    static void CALLBACK ParseMessage(SOCKADDR_IN &addr, string RecvBuf);
    bool Init(bool bUpdate = false);
	bool StartRun();
	void StopRun();
public:
    bool GetRTPStreamInfo();
    bool GetServerInfo();
    bool GetDeviceInfo(bool bUpdate = false);
    bool GetLGDeviceInfo(bool bUpdate = false);
    bool GetLGPrivateNetworkDeviceInfo(bool bUpdate = false);
    bool UpdateDeviceToSIPServer();
    bool GetLocalServerID(string sServerCode);

    bool SetAssignRTPServer(string sRTPServerURI);

	string GetRegisterAddr(SOCKADDR_IN &addr);
	string GetProxy(string sAddr);										//通过SIP地址获得SIP代理地址
	void SetDestAddr(string sAddr, SOCKADDR_IN *addr);					//通过SIP地址获得目标套接字
	bool IsMatrix(string sMatrix);
	bool EraseSessionMap(string sID);
	bool FindSessionMap(string sID);
	void CheckDeviceStatus(void);

    bool IsSameNet(string sIP1, string sIP2);
	
//协议操作相关
public:
	int action(SOCKADDR_IN &addr, char *msg);
    int ServerQuertInfoRecv(SOCKADDR_IN &addr, char *msg);


	int SuperiorSendRegMsg(void);        //向上级发送注册消息
    int SuperiorRegAnswer(SOCKADDR_IN addr, char * sRegAnswer);    //向上级注册的回应消息
    int SuperiorSendKeepalive(void);								//向上级发送心跳
    int SuperiorKeepaliveAns(SOCKADDR_IN &addr, char *msg);

    int SuperiorCatalogAnswer(SOCKADDR_IN &addr, char *msg);	//上级发送的目录查询命令处理
    int SuperiorSubscribeAnswer(SOCKADDR_IN &addr, char *msg);  //上级发送的目录订阅消息处理
    static DWORD WINAPI SuperiorCatalogThread(LPVOID lpParam);
    int SuperiorCatalogSend(string sAddr, string sSN, string sDevice, string ttag, string ftag, 
                            string callid, int nCount, string sTable, bool bNotify = false);
    static DWORD WINAPI SuperiorNotifyThread(LPVOID lpParam);
    int SuperiorNotifySend(string sAddr, string ttag, string ftag, string callid, int nCount);

    int DeviceRegAnswer(SOCKADDR_IN &addr, char *msg);      //设备注册消息
    int DeviceKeepaliveRecv(SOCKADDR_IN &addr, char *msg);		//设备心跳消息

    int SendCatalogMsg(SOCKADDR_IN &addr, string uri);		//向设备发送目录查询
    int DeviceCatalogAnswer(SOCKADDR_IN &addr, char *msg);	//设备发送的目录处理
    static DWORD WINAPI SaveCatalogInfoThread(LPVOID lpParam);      //检测时间定时清除资源线程
    void SaveCatalogInfoAction();
    
    int SaveCatalogToDB(SOCKADDR_IN &addr, const char *xml, const char *sServerID, char *pSn = NULL);
    int DeleteOvertimeChannel(string sDeviceID);

    int SendSubscribeMsg(SOCKADDR_IN &addr, char *msg);     //向设备发送目录订阅
    int DeviceNotifyAnswer(SOCKADDR_IN &addr, char *msg);   //目录订阅结果处理

    int RebootMsgRecv(SOCKADDR_IN &addr, char *msg);        //收到用户或上级的重启消息.
	
	int InviteMsgRecv(SOCKADDR_IN &addr, char *msg);        //收到Invite消息处理
	int InviteAnswerRecv(SOCKADDR_IN &addr, char *msg);     //发送Invite消息给流媒体或设备后, 回应的消息处理
	int InviteAckRecv(char *msg);                           //客户端发送的ACK消息处理.
    int InviteFailure(SOCKADDR_IN &addr, char *msg);

	int CancelMsgRecv(SOCKADDR_IN &addr, char *msg);         //Invite流程未走通时, 取消本次会话
    int ByeMsgRecv(SOCKADDR_IN &addr, char *msg);      //客户端发送的Bye信令处理
	int ByeAnswerRecv(SOCKADDR_IN &addr, char *msg);   //向流媒体或设备发送的bye消息回应处理
    int SendByeMsg(PLAYINFO PlayInfo, bool deleteCam = false);

	int PTZMsgRecv(SOCKADDR_IN &addr, char *msg);   //云台控制
	int PTZControlSend(SOCKADDR_IN &addr, char *msg, string sDeviceAddr);					


	int ReplayControlMsgRecv(SOCKADDR_IN &addr, char *msg);
    int ReplayMediaStatusRecv(SOCKADDR_IN &addr, char *msg);

	int RecordInfoMsgRecv(SOCKADDR_IN &addr, char *msg);		//录像文件检索
	int RecordInfoAnswerRecv(SOCKADDR_IN &addr, char *msg);		//录像文件检索转发

    int DeviceSmartMsgRecv(SOCKADDR_IN &addr, char *msg);       //用户发送的Smart设置
    int DeviceSmartAnswerRecv(SOCKADDR_IN &addr, char *msg);    //设备发送的Smart报警
    int UserRecvSmartMsgRecv(SOCKADDR_IN &addr, char *msg);     //客户端接收Smart消息设置
    int DBSaveUserToSmart(string sUserID);
    int DBSaveDeviceSmartSet(string sBody);

	int DeviceInfoMsgRecv(SOCKADDR_IN &addr, char *msg);			//设备信息
	int DeviceInfoAnswerRecv(SOCKADDR_IN &addr, char *msg);			//设备信息转发
	int DeviceStatusMsgRecv(SOCKADDR_IN &addr, char *msg);			//设备状态
	int DeviceStatusAnswerRecv(SOCKADDR_IN &addr, char *msg);		//设备状态转发
    int CommandResponse(SOCKADDR_IN &addr, char *msg);  //控制回应消息处理

	int DeviceGuardMsgRecv(SOCKADDR_IN &addr, char *msg);       //报警
	int DeviceAlarmMsgRecv(SOCKADDR_IN &addr, char *msg);
	int DeviceAlarmMsgAnswer(SOCKADDR_IN &addr, char *msg);

    int DeviceRecordMsgRecv(SOCKADDR_IN &addr, char *msg);      //设备手动录像消息
    int ResendDeviceRecordMsg(SOCKADDR_IN &addr, char *msg);

public:
	static DWORD WINAPI SuperiorKeepaliveThread(LPVOID lpParam);  //向上级发送心跳线程
	static DWORD WINAPI AlarmThread(LPVOID lpParm);
	static DWORD WINAPI DeviceStatusThread(LPVOID lpParam);      //检测设备状态线程
    static DWORD WINAPI CheckRTPStreamThread(LPVOID lpParam);      //检测RTP流媒体状态线程
    void CheckRTPStreamAction();
    bool SetRTPServerStatus(SOCKADDR_IN &addr, char *msg);
    static DWORD WINAPI CheckTimeThread(LPVOID lpParam);      //检测时间定时清除资源线程
    void CheckTimeAction();

    static DWORD WINAPI CheckIsInitialThread(LPVOID lpParam);      //检测镜头守望线程
    void CheckIsInitialAction();
    bool GetInitialCamera();

    static DWORD WINAPI ReadDBThread(LPVOID lpParam);      //定时重新读取数据库数据
    void ReadDBAction();

    int ChangeTimeToSecond(string sTime);
    string ChangeSecondToTime(int nSecond);

//数据库操作相关
public:
	bool IsCamera(string strID);
	bool IsDevice(string strID);
    bool IsDeviceInCharge(string sDeviceID);
    bool DelCameraInfo(string sDeviceID);
    bool CreateChanGroupInfo(string sDeviceID, string sDeviceName = "", string sParentID = "");
	bool IsMonitor(string strID);
	bool IsUser(string strID);
	bool IsPlatform(string strID);
    bool IsChanGroup(string strID);
    int SaveSubscribe(SOCKADDR_IN &addr, const char *xml, const char *sServerID);
	string GetChannelID(const char *sID, int no);
	string GetDeviceAddr(const char *sID, INVITE_TYPE InviteType = XSIP_INVITE_PLAY, LPCAMERAINFO pCameraInfo = NULL);
	string GetNowTime(int ch = 2); //0 秒级 1 SIP标准 2 时间标准 3 校验时间 4 时间串
	string GetFormatTime(const char *time, int md=0);//将时间格式转为YYYY-MM-DDTHH:MM:SS
	void SetLocalTime(string sTime);
	string ReConver(const TiXmlElement *Element);
	string ReValue(const TiXmlElement *Element);
	int ReIntValue(const TiXmlElement *Element);
	double ReDoubleValuve(const TiXmlElement *Element);

	bool UpdateDeviceStatus(string sAddr, bool bStatus); //更改设备状态
    bool UpdatePlatformStatus(string sAddr, bool bStatus);
public:
    CDataBaseMgr m_DBMgr;
    CDataBaseMgr m_LGDBMgr;
	//CWebWork m_web;
	CParamRead m_pam;
    CSIPProtocol * m_pSIPProtocol; 
    SYSTEMTIME m_StartUpTime;
public:
	DVRINFOMAP m_mapSuperior;	//上级列表
	DVRINFOMAP m_mapDevice;		//下级设备|平台列表
    DVRINFOMAP m_mapUserInfo;

	SessionMap m_SessionMap;	//会话列表
	LockMap    m_LockedDeviceMap;  //锁定云台Map
    SmartUserMap m_mapSmartUser;

    ListCatalogInfo m_listCatalogInfo;  //镜头目录保存链表

	HANDLE m_hAlarm;
	BOOL m_bAlarm;

	BOOL m_bStopSave;

	DWORD m_dwSn;
	DWORD m_dwSSRC;
	int m_bAuth;    //下级注册时, 是否开启鉴权认证
	int m_bRegSupServer;
	int m_bRegisterOut;
	int m_bKeepalive;
	int m_bSub;       //是否向下级发送目录通知消息
	bool m_bForWard;    
	int m_nTimeoutCount;
    int m_bNotResponse;

	int m_nCatalogNum;
	int m_nRecordNum;

	CRITICAL_SECTION m_vLock;
	CRITICAL_SECTION m_sLock;
    CRITICAL_SECTION m_csLock;
    CRITICAL_SECTION m_PTZLock;
    CRITICAL_SECTION m_SmartLock;

	HANDLE           m_hStopEvent;

    
    SIPServerMap m_mapSIPServer;
    string m_sSIPServerID;
    string m_sRealm;
    string m_sSIPServerIP;
    int m_nSIPServerPort;
    int m_nServerID;

    bool ClearUserStream(char * sDevID);

    RTPServerManager * m_pRTPManager;

    map<string, INITIALCAMERAINFO> m_mapInitialCamera;
    map<string, INITIALCAMERAINFO> m_mapPTZInitial;
    CRITICAL_SECTION m_csInitial;
    string m_sUpdateTime;
public:
    string m_sChannelInfoTable;
    string m_sDVRInfoTable;
    string m_sChanGroupInfoTable;
    string m_sChanGroupToChanTable;
    string m_sDataChangeTable;

    string m_sSIPChannelTable;
    string m_sSIPDVRTable;
    string m_sSIPDVRToServerTable;
};
