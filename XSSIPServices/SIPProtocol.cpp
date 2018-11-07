#include "StdAfx.h"
#include "SIPProtocol.h"
#include "LogRecorder.h"


#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)

extern CLogRecorder g_LogRecorder;
CSIPProtocol::CSIPProtocol(void)
{
    m_socketServer = INVALID_SOCKET;
    CompletionPort = NULL;
    InitializeCriticalSection(&m_sendLock);
    InitializeCriticalSection(&m_mapLock);
}

CSIPProtocol::~CSIPProtocol(void)
{
    for(DWORD i = 0; i<SystemInfo.dwNumberOfProcessors * 2; i++)
    {
        PostQueuedCompletionStatus(CompletionPort, 0, 0, NULL);
    }
    if (CompletionPort)
    {
        CloseHandle(CompletionPort);
        CompletionPort = NULL;	
    }
    if (m_socketServer != INVALID_SOCKET)
    {
        closesocket(m_socketServer);
    }
    DeleteCriticalSection(&m_sendLock);
    DeleteCriticalSection(&m_mapLock);
    WSACleanup();

}
bool CSIPProtocol::init(string sLocalIP, int nPort, LPMESSAGECALLBACK lpCallback)
{
    m_pCallback = lpCallback;
    m_nLocalPort = nPort;
    m_sLocalIP = sLocalIP;
    int Ret = 0;

    WSADATA wsaData;
    if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
    {
        printf("WSAStartup failed with error %d\n", Ret);
        return false;
    }
    if ((CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0)) == NULL)   //1----------
    {
        printf( "CreateIoCompletionPort failed with error: %d\n", GetLastError());
        return false;
    }

    // Determine how many processors are on the system.

    //DWORD ThreadID;
    GetSystemInfo(&SystemInfo);
    for(DWORD i = 0; i < SystemInfo.dwNumberOfProcessors * 2; i++)
    {
        HANDLE ThreadHandle;
        if ((ThreadHandle = CreateThread(NULL,0,ServerWorkerThread,this,0,NULL)) == NULL)
        {
            printf("CreateThread() failed with error %d\n", GetLastError());
            return false;
        }
        CloseHandle(ThreadHandle);
        //break;
    }

    if ((m_socketServer = WSASocket(AF_INET,SOCK_DGRAM,0,NULL,0,WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
    {
        printf("WSASocket() failed with error %d\n", WSAGetLastError());
        return false;
    } 

    //设置缓冲大小
    int nBufferSize = DATA_BUFSIZE;
    setsockopt(m_socketServer,SOL_SOCKET,SO_SNDBUF,(char*)&nBufferSize,sizeof(int));
    setsockopt(m_socketServer,SOL_SOCKET,SO_RCVBUF,(char*)&nBufferSize,sizeof(int)); 

    bool bBroadcast = TRUE;
    setsockopt(m_socketServer, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast, sizeof(BOOL));

    /*****************为避免winsock在udp时出现10054错误**************************/
    DWORD dwBytesReturned = 0;
    BOOL bNewBehavior = FALSE;
    DWORD status;
    status = WSAIoctl(m_socketServer,SIO_UDP_CONNRESET,&bNewBehavior,sizeof(bNewBehavior),NULL,0,&dwBytesReturned,NULL,NULL);
    if (SOCKET_ERROR == status)
    {
        DWORD dwErr = WSAGetLastError();
        if (WSAEWOULDBLOCK == dwErr)
        {
            // nothing to do
            return(FALSE);
        }
        else
        {
            printf("WSAIoctl(SIO_UDP_CONNRESET) Error: %d\n", dwErr);
            return(FALSE);
        }
    }
    //结束*****************************************************************************

    //绑定SOCKET到本机
    InternetAddr.sin_family = AF_INET;
    InternetAddr.sin_addr.S_un.S_addr = inet_addr(m_sLocalIP.c_str());//htonl(INADDR_ANY);
    InternetAddr.sin_port = htons(nPort);

    if (bind(m_socketServer, (PSOCKADDR) &InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
    {
        printf("绑定Socket失败[%s:%d][error: %d]\n", m_sLocalIP.c_str(), nPort, WSAGetLastError());
        return false;
    }

    PerHandleData = (LPPER_HANDLE_DATA) GlobalAlloc(GPTR, sizeof(PER_HANDLE_DATA))        ;                //分配内存
    PerHandleData->Socket = m_socketServer;
    CreateIoCompletionPort((HANDLE)m_socketServer, CompletionPort, (ULONG_PTR)PerHandleData,0);                //绑定

    if ((PerIoDataRecv = (LPPER_IO_OPERATION_DATA) GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL)
    {
        printf("GlobalAlloc() failed with error %d\n", GetLastError());
        return false;
    }   
    ZeroMemory(&(PerIoDataRecv->Overlapped), sizeof(OVERLAPPED));
    PerIoDataRecv->DataBuf.len = DATA_BUFSIZE;
    PerIoDataRecv->DataBuf.buf = PerIoDataRecv->Buffer;
    PerIoDataRecv->nType = 2;
    DWORD Flags = 0;
    DWORD RecvBytes = 0;
    int nAddr = sizeof(SOCKADDR_IN);
    if (WSARecvFrom(m_socketServer, &(PerIoDataRecv->DataBuf), 1, &RecvBytes, &Flags,
        (SOCKADDR*)&(PerIoDataRecv->addrClient), &nAddr, &(PerIoDataRecv->Overlapped), NULL) == SOCKET_ERROR)
    {
        int nError = WSAGetLastError();
        if (nError != ERROR_IO_PENDING)
        {
            printf("WSARecv() failed with error %d\n", nError);
            return false;
        }
    }



    if ((PerIoDataSend = (LPPER_IO_OPERATION_DATA) GlobalAlloc(GPTR, sizeof(PER_IO_OPERATION_DATA))) == NULL)
    {
        printf("GlobalAlloc() failed with error %d\n", GetLastError());
        return false;
    }
    ZeroMemory(&(PerIoDataSend->Overlapped), sizeof(OVERLAPPED));
    PerIoDataSend->nType = 1;

    CreateThread(NULL, 0, ResendMsgThread, this, 0, NULL);  //
    return true;
}
bool CSIPProtocol :: sendBuf(SOCKADDR_IN& remoteAddr, char* pSendBuf, DWORD dwDateLen, bool bResend)
{ 
    if(!bResend)
    {
        string sSendBuf(pSendBuf, dwDateLen);
        if(sSendBuf.find("INVITE") == 0 || sSendBuf.find("MESSAGE") == 0 || sSendBuf.find("SUBSCRIBE") == 0 ||
            sSendBuf.find("NOTIFY") == 0 || sSendBuf.find("CANCEL") == 0 || sSendBuf.find("INFO") == 0 ||
            sSendBuf.find("REGISTER") == 0 || sSendBuf.find("BYE") == 0 ||
            (sSendBuf.find("SIP/2.0 200 OK") == 0 && (int)sSendBuf.find("INVITE") > 0)
           )
        {
            RESENDMSG  ReMsg ;
            ReMsg.nSendTime = GetTickCount();
            ReMsg.nResendCount = 0;
            ReMsg.sMsg.assign(pSendBuf, dwDateLen);
            memcpy(&ReMsg.remoteAddr, &remoteAddr, sizeof(SOCKADDR));
            ReMsg.bRep = false;
            string sCallID = xsip_get_CallID(ReMsg.sMsg.c_str());
            EnterCriticalSection(&m_mapLock);
            map<string, RESENDMSG>::iterator it = m_mapResendMsgInfo.find(sCallID);
            if(it != m_mapResendMsgInfo.end())
            {
                m_mapResendMsgInfo.erase(it);
            }
            m_mapResendMsgInfo.insert(ResendMap::value_type(sCallID, ReMsg));	//保存会话信息
            //printf("Insert %s\n", sCallID.c_str());
            LeaveCriticalSection(&m_mapLock);
        }
    }


    EnterCriticalSection(&m_sendLock);
    memcpy_s(PerIoDataSend->Buffer, DATA_BUFSIZE, pSendBuf, dwDateLen);
    PerIoDataSend->DataBuf.buf = PerIoDataSend->Buffer;
    PerIoDataSend->DataBuf.len = dwDateLen;
    PerIoDataSend->nType = 1;

    if (WSASendTo(PerHandleData->Socket, &(PerIoDataSend->DataBuf), 1, NULL, 0,
        (SOCKADDR*)&(remoteAddr), sizeof(SOCKADDR), &(PerIoDataSend->Overlapped), NULL) == SOCKET_ERROR)
    {
        int nRet = WSAGetLastError();
        if (WSAGetLastError() != ERROR_IO_PENDING)
        {
            printf("WSASendTo() failed, socket [%d] ,error [%d]\n", PerHandleData->Socket ,WSAGetLastError());
            LeaveCriticalSection(&m_sendLock);


            //发送失败, 则从之前保存的重复发送map里删除相关信息
            EnterCriticalSection(&m_mapLock);
            string sCallID = xsip_get_CallID(pSendBuf);
            map<string, RESENDMSG>::iterator it = m_mapResendMsgInfo.find(sCallID);
            if(it != m_mapResendMsgInfo.end())
            {
                m_mapResendMsgInfo.erase(it);
            }
            LeaveCriticalSection(&m_mapLock);

            return false;
        }
    } 
    LeaveCriticalSection(&m_sendLock);

    return true;
}
DWORD CSIPProtocol::ResendMsgThread(LPVOID lpParam)
{
    CSIPProtocol * pThis = (CSIPProtocol * )lpParam;
    pThis->ResendMsgAction();
    return 0;
}
void CSIPProtocol::ResendMsgAction()
{
    while(true)
    {
        int nNowTime = GetTickCount();
        EnterCriticalSection(&m_mapLock);

        map<string, RESENDMSG>::iterator it = m_mapResendMsgInfo.begin();
        while(it != m_mapResendMsgInfo.end())
        {
            if(it->second.bRep || it->second.nResendCount >= 3)        //收到回应, 或重复发送次数大于3, 则删除保存结点
            {
                it = m_mapResendMsgInfo.erase(it);
                continue;
            }
            else      //未收到,重复发送
            {
                int nSec = 500 * pow(2, (double)it->second.nResendCount);  //上次发送后的等待时间, 500ms, 1000ms, 2000ms....
                if(nNowTime - it->second.nSendTime > nSec)  //当前时间减去上次发送时间大于等待时间, 则重复发送
                {
                    if(it->second.sMsg.find("SIP/2.0 200 OK") == 0 && (int)it->second.sMsg.find("INVITE") > 0)
                    {
                        string sCallID = xsip_get_CallID(it->second.sMsg);
                        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "Warning: 重复发送200 OK信令, CallID[%s].", sCallID.c_str());
                    }
                    sendBuf(it->second.remoteAddr, (char*)it->second.sMsg.c_str(), it->second.sMsg.size(), true);
                    it->second.nResendCount ++;
                    it->second.nSendTime = nNowTime;

                    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "-----重复发送Msg: [%s:%d], CallID[%s]\n", inet_ntoa(it->second.remoteAddr.sin_addr), 
                        ntohs(it->second.remoteAddr.sin_port), it->first.c_str());
                }
            }
            it ++;
        }

        LeaveCriticalSection(&m_mapLock);

        Sleep(500);
    }
    return;
}
string CSIPProtocol::xsip_get_CallID(string strMsg)
{
    char sEnd[3] = {13, 10, 0};
    char sBegin[] = "Call-ID: ";
    int nBegin = 0;
    int nEnd = 0;
    int nLen = 0;
    nBegin = strMsg.find(sBegin);
    if (nBegin != string::npos)
    {
        nBegin += strlen(sBegin);
        nEnd = strMsg.find(sEnd, nBegin);
        nLen = nEnd - nBegin;
        if (nLen <= 0 || (int)nLen > 256)
        {
            return "";
        }
        return strMsg.substr(nBegin, nLen);
    }
    return "";
}
void CSIPProtocol :: uninit()
{
    m_bRunning = false;
    for(DWORD i = 0; i<SystemInfo.dwNumberOfProcessors * 2; i++)
    {
        PostQueuedCompletionStatus(CompletionPort, 0, 0, NULL);
    }

    CloseHandle(CompletionPort);
    if (m_socketServer != INVALID_SOCKET)
    {
        closesocket(m_socketServer);
    }
    WSACleanup();
}
DWORD WINAPI CSIPProtocol :: ServerWorkerThread(LPVOID lParam)
{
    CSIPProtocol * pThis = (CSIPProtocol*)lParam;
    pThis->ServerWorker();
    return 0;
}
void CSIPProtocol :: ServerWorker()
{
    DWORD BytesTransferred;
    LPPER_HANDLE_DATA PerHandleData = NULL;
    LPPER_IO_OPERATION_DATA PerIoData = NULL;
    int nFind = 0;

    m_bRunning= true;
    while(m_bRunning)
    {
        int ret = GetQueuedCompletionStatus(CompletionPort, &BytesTransferred,
            (LPDWORD)&PerHandleData, (LPOVERLAPPED *) &PerIoData, INFINITE);

        if(NULL == PerIoData)
        {
            return;
        }
        string RecvBuf;
        DWORD Flags = 0;
        DWORD RecvBytes = 0;
        int nAddr;
        SOCKADDR_IN addrClient;
        switch(PerIoData->nType)
        {
        case 1:           //发送消息

            break;
        case 2:           //接收消息后: 
            addrClient = PerIoData->addrClient;  //Save Address
            RecvBuf = PerIoData->Buffer;
            //1. 重新投递接收消息请求
            ZeroMemory(&(PerIoData->Overlapped), sizeof(OVERLAPPED));
            ZeroMemory(PerIoData->Buffer, sizeof(PerIoData->Buffer));
            PerIoData->DataBuf.len = DATA_BUFSIZE;
            PerIoData->DataBuf.buf = PerIoData->Buffer;
            PerIoData->nType = 2;
            Flags = 0;
            RecvBytes = 0;
            nAddr = sizeof(SOCKADDR_IN);
            if (WSARecvFrom(m_socketServer, &(PerIoData->DataBuf), 1, &RecvBytes, &Flags,
                (SOCKADDR*)&(PerIoData->addrClient), &nAddr, &(PerIoData->Overlapped), NULL) == SOCKET_ERROR)
            {
                if (WSAGetLastError() != ERROR_IO_PENDING)
                {
                    printf("WSARecvFrom() failed with error %d\n", WSAGetLastError());
                    m_pCallback(addrClient, RecvBuf);
                    continue;
                }
            }

            nFind = RecvBuf.find("\r\n\r\n");
            if(nFind <= 0 && RecvBuf.find("KeepRTPStreamAlive") == string::npos)
            {
                break;
            }
            if(RecvBuf.find("SIP/2.0 1") != string::npos)
            {
                break;
            }
            if(RecvBuf.find("SIP/2.0") == 0 || RecvBuf.find("ACK") == 0 )
            {
                if( ((int)RecvBuf.find("INVITE") > 0 && (int)RecvBuf.find("INVITE", 10) < nFind)      ||
                    ((int)RecvBuf.find("MESSAGE") > 0 && (int)RecvBuf.find("MESSAGE", 10) < nFind)    ||
                    ((int)RecvBuf.find("NOTIFY") > 0 && (int)RecvBuf.find("NOTIFY", 10) < nFind)      ||
                    ((int)RecvBuf.find("REGISTER") > 0 && (int)RecvBuf.find("REGISTER", 10) < nFind)  ||
                    ((int)RecvBuf.find("BYE") > 0 && (int)RecvBuf.find("BYE", 10) < nFind)            ||
                    ((int)RecvBuf.find("CANCEL") > 0 && (int)RecvBuf.find("CANCEL", 10) < nFind)      ||
                    ((int)RecvBuf.find("SUBSCRIBE") > 0 && (int)RecvBuf.find("SUBSCRIBE", 10) < nFind)||
                    ((int)RecvBuf.find("INFO") > 0 && (int)RecvBuf.find("INFO", 10) < nFind)          ||
                     (int)RecvBuf.find("ACK") == 0)
                {
                    EnterCriticalSection(&m_mapLock);
                    
                    string sCallID = xsip_get_CallID(RecvBuf);
                    map<string, RESENDMSG>::iterator it = m_mapResendMsgInfo.find(sCallID);
                    if(it != m_mapResendMsgInfo.end())
                    {                   
                        it->second.bRep = true;
                        LeaveCriticalSection(&m_mapLock);
                        //把收到的消息回调出去处理
                        m_pCallback(addrClient, RecvBuf); 
                    }
                    else
                    {
                        //printf("Recv Repeat Message, CallID[%s].\n", sCallID.c_str());
                        LeaveCriticalSection(&m_mapLock);
                    }
                }       
                else
                {
                    m_pCallback(addrClient, RecvBuf); 
                }
            }                          
            else
            {
                m_pCallback(addrClient, RecvBuf); 
            }

            break;
        default:
            break;
        }
    }
    return;
}