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

#define PTZ_LOCK_SUCCESSED     100    //�����ɹ�
#define PTZ_UNLOCK_SUCCESSED   101    //�����ɹ�
#define PTZ_CONTROL_SUCCESSED  102    //���Ƴɹ�
#define PTZ_LOCK_FAILED_LOCKED 103    //����ʧ�ܣ���ǰ�û��Ѿ�������̨�������ٴ�����
#define PTZ_LOCK_FAILED_LOW    104    //����ʧ�ܣ���ǰ��̨�ѱ�������Ȩ���û��������޷�����
#define PTZ_UNLOCK_FAILED      105    //��ǰ�û�û��������̨������ʧ��
#define PTZ_CONTROL_FAILED     106

#define MAX_STRING_LEN			1024  //������󳤶�
#define KEEPALIVENOANS      5   //���ϼ�����������Ϣ�޻�Ӧ������, ����������ע��.

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

typedef struct _DVRInfo		//�豸��ַ��Ϣ
{
    int nNetType;       //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK����; 5: RTSP; 6: ONVIF; 7:���ſ�ȡ��
    int nTranscoding;   //0: ��ת��; 1: ת��
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
	time_t  tRegistTime;   //���ע��ʱ����յ�����ʱ��
    int nNoAnsKeepalive;
    bool bSendCatalog;      //�Ƿ���¼�����Ŀ¼������Ϣ, �ڷ�������ʱ��ȡ�����ļ�����������"M", �ڷ���һ�κ���false
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
    string sCameraID;   //��ͷ����
    string sCameraName; 
    int nCameraType;    //��ͷ��������, 1 HK, 2 DH, 20 SIP
    string sDVRKey;

    //���������Ϣ
    string sSIPIP;          //��ͷ�����豸��ƽ̨IP
    int nSIPPort;           //��ͷ�����豸��ƽ̨�˿�
    string sDeviceCode;     //��ͷ�����豸��ƽ̨����
    string sServerCode;     //��ͷ����SIP�������

    //SDK������Ϣ
    string sDeviceIP;   //�豸IP(IPC IP, ��ͷ���豸(DVR|NVR)IP
    int nDevicePort;    //�豸sdk���Ӷ˿�
    string sDeviceUsername;
    string sDevicePassword;
    int nChannel;

    int nNetType;           //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK����; 5: RTSP; 6: ONVIF; 7: ���ſ�ȡ��
    int nTranscoding;       //0: ��ת��; 1: ת��
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
	string sTitle;	//��Ϣ��
	string sAddr;	//��Դ��ַ����
	string dwFTag;	//��Դ��ʶ
	string dwTTag;	//Ŀ���ʶ
	string sDeviceID;
	string request;
	string response;
    bool bSendResponse;  //�Ƿ񼺷��ͻ�Ӧ, ���յ��ظ���Ϣʱ, ֱ���ٴη���response, �����ٴ���.
	int    nNum;        //��ͷĿ¼��¼����Ϣ��Ŀ����
	SOCKADDR_IN sockAddr;
    time_t    recvTime;
    string sSN;        //XML�ֶ��е�SN
    int nNetType;       //1: RTP/UDP; 2: TCP/RTP/AVP; 4: SDK����; 5: RTSP; 6: ONVIF
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
    string sLowServerIP;   //��ͷ�����ĵ�ǰSIP������¼�SIP�����IP
    int nLowServerPort;
    time_t tLastControlTime; // ���һ�ο�����̨��ʱ��
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

typedef struct _SmartUser  //����smart�����û���Ϣ
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
typedef map<string, LOCKINFO> LockMap; //����map
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
	string GetProxy(string sAddr);										//ͨ��SIP��ַ���SIP�����ַ
	void SetDestAddr(string sAddr, SOCKADDR_IN *addr);					//ͨ��SIP��ַ���Ŀ���׽���
	bool IsMatrix(string sMatrix);
	bool EraseSessionMap(string sID);
	bool FindSessionMap(string sID);
	void CheckDeviceStatus(void);

    bool IsSameNet(string sIP1, string sIP2);
	
//Э��������
public:
	int action(SOCKADDR_IN &addr, char *msg);
    int ServerQuertInfoRecv(SOCKADDR_IN &addr, char *msg);


	int SuperiorSendRegMsg(void);        //���ϼ�����ע����Ϣ
    int SuperiorRegAnswer(SOCKADDR_IN addr, char * sRegAnswer);    //���ϼ�ע��Ļ�Ӧ��Ϣ
    int SuperiorSendKeepalive(void);								//���ϼ���������
    int SuperiorKeepaliveAns(SOCKADDR_IN &addr, char *msg);

    int SuperiorCatalogAnswer(SOCKADDR_IN &addr, char *msg);	//�ϼ����͵�Ŀ¼��ѯ�����
    int SuperiorSubscribeAnswer(SOCKADDR_IN &addr, char *msg);  //�ϼ����͵�Ŀ¼������Ϣ����
    static DWORD WINAPI SuperiorCatalogThread(LPVOID lpParam);
    int SuperiorCatalogSend(string sAddr, string sSN, string sDevice, string ttag, string ftag, 
                            string callid, int nCount, string sTable, bool bNotify = false);
    static DWORD WINAPI SuperiorNotifyThread(LPVOID lpParam);
    int SuperiorNotifySend(string sAddr, string ttag, string ftag, string callid, int nCount);

    int DeviceRegAnswer(SOCKADDR_IN &addr, char *msg);      //�豸ע����Ϣ
    int DeviceKeepaliveRecv(SOCKADDR_IN &addr, char *msg);		//�豸������Ϣ

    int SendCatalogMsg(SOCKADDR_IN &addr, string uri);		//���豸����Ŀ¼��ѯ
    int DeviceCatalogAnswer(SOCKADDR_IN &addr, char *msg);	//�豸���͵�Ŀ¼����
    static DWORD WINAPI SaveCatalogInfoThread(LPVOID lpParam);      //���ʱ�䶨ʱ�����Դ�߳�
    void SaveCatalogInfoAction();
    
    int SaveCatalogToDB(SOCKADDR_IN &addr, const char *xml, const char *sServerID, char *pSn = NULL);
    int DeleteOvertimeChannel(string sDeviceID);

    int SendSubscribeMsg(SOCKADDR_IN &addr, char *msg);     //���豸����Ŀ¼����
    int DeviceNotifyAnswer(SOCKADDR_IN &addr, char *msg);   //Ŀ¼���Ľ������

    int RebootMsgRecv(SOCKADDR_IN &addr, char *msg);        //�յ��û����ϼ���������Ϣ.
	
	int InviteMsgRecv(SOCKADDR_IN &addr, char *msg);        //�յ�Invite��Ϣ����
	int InviteAnswerRecv(SOCKADDR_IN &addr, char *msg);     //����Invite��Ϣ����ý����豸��, ��Ӧ����Ϣ����
	int InviteAckRecv(char *msg);                           //�ͻ��˷��͵�ACK��Ϣ����.
    int InviteFailure(SOCKADDR_IN &addr, char *msg);

	int CancelMsgRecv(SOCKADDR_IN &addr, char *msg);         //Invite����δ��ͨʱ, ȡ�����λỰ
    int ByeMsgRecv(SOCKADDR_IN &addr, char *msg);      //�ͻ��˷��͵�Bye�����
	int ByeAnswerRecv(SOCKADDR_IN &addr, char *msg);   //����ý����豸���͵�bye��Ϣ��Ӧ����
    int SendByeMsg(PLAYINFO PlayInfo, bool deleteCam = false);

	int PTZMsgRecv(SOCKADDR_IN &addr, char *msg);   //��̨����
	int PTZControlSend(SOCKADDR_IN &addr, char *msg, string sDeviceAddr);					


	int ReplayControlMsgRecv(SOCKADDR_IN &addr, char *msg);
    int ReplayMediaStatusRecv(SOCKADDR_IN &addr, char *msg);

	int RecordInfoMsgRecv(SOCKADDR_IN &addr, char *msg);		//¼���ļ�����
	int RecordInfoAnswerRecv(SOCKADDR_IN &addr, char *msg);		//¼���ļ�����ת��

    int DeviceSmartMsgRecv(SOCKADDR_IN &addr, char *msg);       //�û����͵�Smart����
    int DeviceSmartAnswerRecv(SOCKADDR_IN &addr, char *msg);    //�豸���͵�Smart����
    int UserRecvSmartMsgRecv(SOCKADDR_IN &addr, char *msg);     //�ͻ��˽���Smart��Ϣ����
    int DBSaveUserToSmart(string sUserID);
    int DBSaveDeviceSmartSet(string sBody);

	int DeviceInfoMsgRecv(SOCKADDR_IN &addr, char *msg);			//�豸��Ϣ
	int DeviceInfoAnswerRecv(SOCKADDR_IN &addr, char *msg);			//�豸��Ϣת��
	int DeviceStatusMsgRecv(SOCKADDR_IN &addr, char *msg);			//�豸״̬
	int DeviceStatusAnswerRecv(SOCKADDR_IN &addr, char *msg);		//�豸״̬ת��
    int CommandResponse(SOCKADDR_IN &addr, char *msg);  //���ƻ�Ӧ��Ϣ����

	int DeviceGuardMsgRecv(SOCKADDR_IN &addr, char *msg);       //����
	int DeviceAlarmMsgRecv(SOCKADDR_IN &addr, char *msg);
	int DeviceAlarmMsgAnswer(SOCKADDR_IN &addr, char *msg);

    int DeviceRecordMsgRecv(SOCKADDR_IN &addr, char *msg);      //�豸�ֶ�¼����Ϣ
    int ResendDeviceRecordMsg(SOCKADDR_IN &addr, char *msg);

public:
	static DWORD WINAPI SuperiorKeepaliveThread(LPVOID lpParam);  //���ϼ����������߳�
	static DWORD WINAPI AlarmThread(LPVOID lpParm);
	static DWORD WINAPI DeviceStatusThread(LPVOID lpParam);      //����豸״̬�߳�
    static DWORD WINAPI CheckRTPStreamThread(LPVOID lpParam);      //���RTP��ý��״̬�߳�
    void CheckRTPStreamAction();
    bool SetRTPServerStatus(SOCKADDR_IN &addr, char *msg);
    static DWORD WINAPI CheckTimeThread(LPVOID lpParam);      //���ʱ�䶨ʱ�����Դ�߳�
    void CheckTimeAction();

    static DWORD WINAPI CheckIsInitialThread(LPVOID lpParam);      //��⾵ͷ�����߳�
    void CheckIsInitialAction();
    bool GetInitialCamera();

    static DWORD WINAPI ReadDBThread(LPVOID lpParam);      //��ʱ���¶�ȡ���ݿ�����
    void ReadDBAction();

    int ChangeTimeToSecond(string sTime);
    string ChangeSecondToTime(int nSecond);

//���ݿ�������
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
	string GetNowTime(int ch = 2); //0 �뼶 1 SIP��׼ 2 ʱ���׼ 3 У��ʱ�� 4 ʱ�䴮
	string GetFormatTime(const char *time, int md=0);//��ʱ���ʽתΪYYYY-MM-DDTHH:MM:SS
	void SetLocalTime(string sTime);
	string ReConver(const TiXmlElement *Element);
	string ReValue(const TiXmlElement *Element);
	int ReIntValue(const TiXmlElement *Element);
	double ReDoubleValuve(const TiXmlElement *Element);

	bool UpdateDeviceStatus(string sAddr, bool bStatus); //�����豸״̬
    bool UpdatePlatformStatus(string sAddr, bool bStatus);
public:
    CDataBaseMgr m_DBMgr;
    CDataBaseMgr m_LGDBMgr;
	//CWebWork m_web;
	CParamRead m_pam;
    CSIPProtocol * m_pSIPProtocol; 
    SYSTEMTIME m_StartUpTime;
public:
	DVRINFOMAP m_mapSuperior;	//�ϼ��б�
	DVRINFOMAP m_mapDevice;		//�¼��豸|ƽ̨�б�
    DVRINFOMAP m_mapUserInfo;

	SessionMap m_SessionMap;	//�Ự�б�
	LockMap    m_LockedDeviceMap;  //������̨Map
    SmartUserMap m_mapSmartUser;

    ListCatalogInfo m_listCatalogInfo;  //��ͷĿ¼��������

	HANDLE m_hAlarm;
	BOOL m_bAlarm;

	BOOL m_bStopSave;

	DWORD m_dwSn;
	DWORD m_dwSSRC;
	int m_bAuth;    //�¼�ע��ʱ, �Ƿ�����Ȩ��֤
	int m_bRegSupServer;
	int m_bRegisterOut;
	int m_bKeepalive;
	int m_bSub;       //�Ƿ����¼�����Ŀ¼֪ͨ��Ϣ
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
