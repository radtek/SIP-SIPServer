#pragma once
#include <string>
#include "XSIP/xsip.h"
#include <list>
using namespace std;

#define REALMEDIASERVERTYPE 202
#define FILEMEDIASERVERTYPE 224

typedef enum RESPONSETYPE
{
    REPNONE,
    REPRTPRECVSTREAM,
    REPCAMERASEND,
    REPRTPSENDSTREAM,
    REPDECODERRECV,
    REPCLIENT      //�ͻ��˻��ϼ����͵���Ϣ
};

typedef struct PlayInfo
{
    RESPONSETYPE rep;

    string sRTPServerID;
    string sRTPUri;
    string sRTPServerIP;
    int nRTPServerPort;

    string sCameraID;
    string sCameraUri;
    int nCameraType;
    int nCamCseq;
    int nTranmit;      
    string sCallID2;
    string sCallID3;
    string sInviteMsg2;    
    string sInvite2Answer;
    string sInviteMsg3;
    string sInvite3Answer;
    INVITE_TYPE InviteType;    //InviteType
    string sSSRC;

    string sMediaRecverID;
    string sMediaRecvUri;
    string sCallID1;
    string sCallID4;
    string sCallID5;
    string sInviteMsg1;    
    string sInvite1Answer;
    string sInviteMsg4;
    string sInvite4Answer;
    string sInviteMsg5;
    string sInvite5Answer;
    PlayInfo()
    {
        rep = REPNONE;
        nCameraType = 20;
        nCamCseq = 0;
    }
}PLAYINFO, *LPPLAYINFO;


typedef struct InviteCallID
{
    string sMediaRecvID;
    string sMediaRecvUri;
    string sInviteCallID1;
    string sInviteCallID4;
    string sCallID5;

    string sInviteMsg1;
    string sInvite1Answer;
    string sInviteMsg4;
    string sInvite4Answer;
    string sInviteMsg5;
    string sInvite5Answer;

    bool bRecvInvite1ACK;   //�Ƿ��յ��ͻ��˷��͵�ACK����
    InviteCallID()
    {
        bRecvInvite1ACK = false;
    }
}INVITECALLID, *LPINVITECALLID;
typedef list<LPINVITECALLID>::iterator INVITEIT;

typedef struct CameraToRTP
{
    string sCameraID;
    string sCameraUri;      //sip:ID@IP:Port
    int nCameraType;        //��ͷ����, 1: HK, 2: DH, 20: SIP
    int nCamCseq;
    string sSSRC;
    string sCallID2;
    string sCallID3;

    INVITE_TYPE InviteType;  //����������, 0ʵʱ, 1�ط�, 2����
    int nTranmit;           //ʵʱ���ž�ͷת������
    string sInviteMsg2;
    string sInvite2Answer;
    string sInviteMsg3;
    string sInvite3Answer;
    list<LPINVITECALLID> listSendStreamCallID;
    CameraToRTP()
    {
        InviteType =  XSIP_INVITE_PLAY;
        sCameraID = "";
        sCallID2 = "";
        sCallID3 = "";
        listSendStreamCallID.clear();
        nTranmit = 0;
        nCamCseq = 0;
        nCameraType = 20;
    }
    ~CameraToRTP()
    {
        for (INVITEIT it = listSendStreamCallID.begin(); it != listSendStreamCallID.end(); it++)
        {
            delete (*it);
        }
    }
}CAMERATORTP, *LPCAMERATORTP;
typedef list<LPCAMERATORTP>::iterator CAMIT;

typedef struct RTPInfo
{
    string sRTPID;         //RTP����ID
    string sRTPUri;         //sip:ID@ip:port
    string sRTPIP;         //RTP����IP
    unsigned int nRTPPort; //RTP����˿�
    int nTotal;   //ת������
    time_t tRepTime;   //���������Ӧʱ��
    bool bOnLine;           //�Ƿ��Ծ
    bool bReStart;          //RTPServer������ʾ
    int nType;              //��ý������, 202: ʵʱ��ý��; 224: �ļ���ý��
    list<LPCAMERATORTP> listCameraToRTP;

    time_t tUpdateTime;
    RTPInfo()
    {
        sRTPID = "";
        sRTPIP = "";
        nRTPPort = 0;
        nTotal = 0;
        time(&tRepTime);
        bOnLine = false;
        bReStart = false;
        time(&tUpdateTime);
    }
}RTP_INFO, *LPRTP_INFO;
typedef list<LPRTP_INFO>::iterator RTPIT;

class RTPServerManager
{
public:
    RTPServerManager();
public:
    ~RTPServerManager(void);
public:
    void InsertRTPInfo(string sLocalID, LPRTP_INFO pRTPInfo);
    bool EraseRTPInfo(time_t tCurrent);
public:

    //ȡRTP������Ϣ.
    bool GetPracticableRTPServer(string sInviteMsg1, string sCameraID, INVITE_TYPE InviteType, string sCallID1, string & sRTPServerID, 
        string & sRTPServerIP, int & nRTPServerPort, bool & bTransmit);
    
    //����ؾ�ͷ��Ӧ��RTP������Ϣ����
    bool PushRTPInfo(string sRTPID, string sCameraID, string sCameraUri, int nCameraType, string sSSRC, string sMediaRecvID, string sMediaRecvUri, string sCallID1, 
        string sCallID2, string sCallID4, string sInviteMsg1, string sInviteMsg2, string sInviteMsg4);
    bool PushRTPRecvAnswer(string sCallID2, string sInvite2Answer);
    bool PushCameraInfo(string sCameraID, string sCallID2, string sCallID3, string sInviteMsg3);
    bool PushCameraAnswer(string sCallID3, string sInvite3Answer, string sSSRC);
    bool PushDecodeInfo(string sDecoderID, string sCallID5, string sInviteMsg5);
    bool PushDecodeAnswer(string sCallID5, string sInvite5Answer);
    bool PushRTPSendInfo(string sCallID1, string sCallID4, string sInviteMsg4);
    bool PushRTPSendAnswer(string sCallID4, string sInvite4Answer);
    bool PushClientAnswer(string sCallID1, string sInvite1Answer);
    bool SetRecvACK(string sCallID1);
    bool GetRecvACK(string sCallID1);

    //ɾ����Ӧ�ľ�ͷ��ӦRTP�����ϵ.
    int ReduceRTPTotal(string sCallID1, string sCallID4 = "", bool bDeleteCamera = false);

    //����CallID1�ҵ��豸ID
    string GetCameraIDByCallID(string sCallID1);
    string GetCameraIDByMonitorID(string sMonitorID);

    //�����û�|�豸��ID, �ҵ�Info
    bool GetPlayInfoByID(string sID, bool bAll, LPPLAYINFO pPlayInfo);
    RESPONSETYPE GetAnswerType(string sCallID, LPPLAYINFO pPlayInfo);

    //����RTPID, �ҵ���Ӧ��Ϣ
    bool GetPlayInfo(string sRTPID, LPPLAYINFO pPlayInfo);
    bool AddPlaybackCseq(string sCallID1);

    //�ļ���ý�岥��ʱ, �����ļ���ý��IPȡ�����Ϣ
    bool GetFileStreamServerInfo(string sServerIP, string & sFileServerID, int & nFileServerPort);

    int GetStreamNum();
public:
    list<LPRTP_INFO> m_listRTPInfo;
    string m_sLocalServerID;

    CRITICAL_SECTION m_cs;

};          
