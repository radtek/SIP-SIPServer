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
    SOCKADDR_IN addrClient;         //发送者地址
    int nType;                      //操作类型, 1为发送, 2为收到
} PER_IO_OPERATION_DATA, * LPPER_IO_OPERATION_DATA;


typedef struct 
{
    SOCKET Socket;                  //客户 Socket
} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

typedef struct _ResendMsg
{
    SOCKADDR_IN remoteAddr;    //发送地址
    int nSendTime; //发送时间
    string sMsg;    //信令
    bool bRep;  //是否收到回应
    int nResendCount;   //重复发送次数
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
    SOCKET m_socketServer;                  //服务器Socket
    SOCKADDR_IN   InternetAddr;             //服务器地址
    SOCKADDR_IN m_addrClient;
    HANDLE CompletionPort;                  //完成端口句柄
    SYSTEM_INFO SystemInfo;                 //系统信息, 用来读取Cpu个数

    LPMESSAGECALLBACK m_pCallback;          //传入的回调函数

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

    static DWORD WINAPI ResendMsgThread(LPVOID lpParam);      //检测RTP流媒体状态线程
    void ResendMsgAction();

public:

    bool init(string sLocalIP, int nPort, LPMESSAGECALLBACK lpCallback);
    void uninit();
    bool sendBuf(SOCKADDR_IN& remoteAddr, char* pSendBuf, DWORD dwDateLen, bool bResend = false);
};

