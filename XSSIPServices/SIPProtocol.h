#pragma once

#include <WinSock2.h>
#pragma comment( lib, "Ws2_32.lib" )
#include <map>
#include "XSIP/xsip.h"
#include <cmath>
#include <string>
using namespace std;

typedef  void(CALLBACK* LPMESSAGECALLBACK)(SOCKADDR_IN &addr, string RecvBuf);

#define DATA_BUFSIZE 8192


typedef struct
{
    OVERLAPPED Overlapped;
    WSABUF DataBuf;
    CHAR Buffer[DATA_BUFSIZE];
    SOCKADDR_IN addrClient;         //�����ߵ�ַ
    int nType;                      //��������, 1Ϊ����, 2Ϊ�յ�
} PER_IO_OPERATION_DATA, * LPPER_IO_OPERATION_DATA;


typedef struct 
{
    SOCKET Socket;                  //�ͻ� Socket
} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

typedef struct _ResendMsg
{
    SOCKADDR_IN remoteAddr;    //���͵�ַ
    int nSendTime; //����ʱ��
    string sMsg;    //����
    bool bRep;  //�Ƿ��յ���Ӧ
    int nResendCount;   //�ظ����ʹ���
    _ResendMsg()
    {
        bRep = false;
        nResendCount = 0;
    }
}RESENDMSG, *LPRESENDMSG;

typedef map<string, RESENDMSG> ResendMap;

class CSIPProtocol
{
public:
    CSIPProtocol(void);
public:
    ~CSIPProtocol(void);
public:
    bool m_bRunning;
    string m_sLocalIP;
    int m_nLocalPort;
    SOCKET m_socketServer;                  //������Socket
    SOCKADDR_IN   InternetAddr;             //��������ַ
    SOCKADDR_IN m_addrClient;
    HANDLE CompletionPort;                  //��ɶ˿ھ��
    SYSTEM_INFO SystemInfo;                 //ϵͳ��Ϣ, ������ȡCpu����

    LPMESSAGECALLBACK m_pCallback;          //����Ļص�����

    LPPER_IO_OPERATION_DATA PerIoDataRecv;
    LPPER_IO_OPERATION_DATA PerIoDataSend;
    LPPER_HANDLE_DATA  PerHandleData;
    CRITICAL_SECTION	m_sendLock;
    CRITICAL_SECTION	m_mapLock;

    map<string, RESENDMSG> m_mapResendMsgInfo;

public:
    static DWORD WINAPI ServerWorkerThread(LPVOID lParam);
    void ServerWorker();
    string xsip_get_CallID(string sMsg);

    static DWORD WINAPI ResendMsgThread(LPVOID lpParam);      //���RTP��ý��״̬�߳�
    void ResendMsgAction();

public:

    bool init(string sLocalIP, int nPort, LPMESSAGECALLBACK lpCallback);
    void uninit();
    bool sendBuf(SOCKADDR_IN& remoteAddr, char* pSendBuf, DWORD dwDateLen, bool bResend = false);
};

