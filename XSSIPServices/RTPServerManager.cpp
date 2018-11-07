#include "stdafx.h"
#include "RTPServerManager.h"
#include "LogRecorder.h"

extern CLogRecorder g_LogRecorder;
RTPServerManager::RTPServerManager()
{
    InitializeCriticalSection(&m_cs);
}

RTPServerManager::~RTPServerManager(void)
{
    RTPIT RTPIt = m_listRTPInfo.begin();
    while (RTPIt != m_listRTPInfo.end())
    {
        CAMIT CamIt = (*RTPIt)->listCameraToRTP.begin();
        while (CamIt != (*RTPIt)->listCameraToRTP.end())
        {
            LPCAMERATORTP pCamInfo = (*CamIt);
            delete pCamInfo;
            pCamInfo = NULL;

            CamIt = (*RTPIt)->listCameraToRTP.erase(CamIt);
        }

        LPRTP_INFO pRTPInfo = (*RTPIt);
        delete pRTPInfo;
        pRTPInfo = NULL;

        RTPIt = m_listRTPInfo.erase(RTPIt);
    }
    DeleteCriticalSection(&m_cs);
}
void RTPServerManager::InsertRTPInfo(string sLocalID, LPRTP_INFO pRTPInfo)
{
    m_sLocalServerID = sLocalID;

    list<LPRTP_INFO>::iterator RTPIt = m_listRTPInfo.begin();
    for (; RTPIt != m_listRTPInfo.end(); RTPIt++)
    {
        if ((*RTPIt)->sRTPID == pRTPInfo->sRTPID)
        {
            if ((*RTPIt)->sRTPIP != pRTPInfo->sRTPIP || (*RTPIt)->nRTPPort != pRTPInfo->nRTPPort)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "当前RTP流媒体[%s]己存在, 修改参数为[%s@%s:%d]!",
                    (*RTPIt)->sRTPUri.c_str(), pRTPInfo->sRTPID.c_str(), pRTPInfo->sRTPIP.c_str(), pRTPInfo->nRTPPort);
                (*RTPIt)->sRTPIP = pRTPInfo->sRTPIP;
                (*RTPIt)->nRTPPort = pRTPInfo->nRTPPort;
            }
            (*RTPIt)->tUpdateTime = pRTPInfo->tUpdateTime;
            delete pRTPInfo;
            return;
        }
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "增加流媒体[%s@%s:%d].", 
        pRTPInfo->sRTPID.c_str(), pRTPInfo->sRTPIP.c_str(), pRTPInfo->nRTPPort);
    m_listRTPInfo.push_back(pRTPInfo);
}
bool RTPServerManager::EraseRTPInfo(time_t tCurrent)
{
    EnterCriticalSection(&m_cs);
    list<LPRTP_INFO>::iterator RTPIt = m_listRTPInfo.begin();
    while(RTPIt != m_listRTPInfo.end())
    {
        if((*RTPIt)->tUpdateTime < tCurrent)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: 删除流媒体[%s@%s:%d].", 
                (*RTPIt)->sRTPID.c_str(), (*RTPIt)->sRTPIP.c_str(), (*RTPIt)->nRTPPort);
            RTPIt = m_listRTPInfo.erase(RTPIt);
        }
        else RTPIt++;
    }
    LeaveCriticalSection(&m_cs);

    return true;
}
bool RTPServerManager::GetPracticableRTPServer(string sInviteMsg1, string sCameraID, INVITE_TYPE InviteType, string sCallID1,
                    string & sRTPServerID, string & sRTPServerIP, int & nRTPServerPort, bool & bTransmit)
{
    EnterCriticalSection(&m_cs);

    if ( XSIP_INVITE_PLAY == InviteType)       //实时播放先查找是否有转发;
    {
        RTPIT RTPIt = m_listRTPInfo.begin();
        for (; RTPIt != m_listRTPInfo.end(); RTPIt++)
        {
            if (!(*RTPIt)->bOnLine || (*RTPIt)->nType == FILEMEDIASERVERTYPE)
            {
                continue;
            }

            CAMIT CamIt = (*RTPIt)->listCameraToRTP.begin();
            for (; CamIt != (*RTPIt)->listCameraToRTP.end(); CamIt++)
            {
                if ((*CamIt)->sCameraID == sCameraID && (*CamIt)->InviteType == XSIP_INVITE_PLAY )
                    //找到转发流;
                {
                    if((*CamIt)->listSendStreamCallID.size() == 0)
                    {
                        (*RTPIt)->listCameraToRTP.erase(CamIt);
                        break;
                    }
                    sRTPServerID = (*RTPIt)->sRTPID;
                    sRTPServerIP = (*RTPIt)->sRTPIP;
                    nRTPServerPort = (*RTPIt)->nRTPPort;
                    bTransmit = true;

                    (*RTPIt)->nTotal ++;
                    (*CamIt)->nTranmit ++;

                    LPINVITECALLID pInviteCallID = new INVITECALLID;
                    pInviteCallID->sInviteCallID1 = sCallID1;
                    pInviteCallID->sInviteMsg1 = sInviteMsg1;
                    (*CamIt)->listSendStreamCallID.push_back(pInviteCallID);

                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "分配RTP流媒体[%s@%s:%d], 当前总转发数[%d], 镜头[%s]是否转发[%d].",
                        sRTPServerID.c_str(), sRTPServerIP.c_str(), nRTPServerPort, (*RTPIt)->nTotal, sCameraID.c_str(), bTransmit);
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }
        }
    }
    
    //实时播放没有找到转发流, 或回放, 下载时;
    RTPIT RTPIt = m_listRTPInfo.begin();
    RTPIT RTPPos = m_listRTPInfo.end();

    int nTotalNum = -999;

    for (; RTPIt != m_listRTPInfo.end(); RTPIt++)   //动态分配RTP负载均衡
    {
        if (!(*RTPIt)->bOnLine || (*RTPIt)->nType == FILEMEDIASERVERTYPE)
        {         
            continue;
        }

        if (nTotalNum == -999)
        {
            RTPPos = RTPIt;
            nTotalNum = (*RTPIt)->nTotal;
        }
        else if (nTotalNum > (*RTPIt)->nTotal)
        {
            RTPPos = RTPIt;
            nTotalNum = (*RTPIt)->nTotal;
        }
    }

    if (RTPPos == m_listRTPInfo.end())
    {
        LeaveCriticalSection(&m_cs);
        return false;
    }
    
    sRTPServerID = (*RTPPos)->sRTPID;
    sRTPServerIP = (*RTPPos)->sRTPIP;
    nRTPServerPort = (*RTPPos)->nRTPPort;

    (*RTPPos)->nTotal++;


    LPCAMERATORTP pCamToRTP = new CameraToRTP;
    pCamToRTP->sCameraID = sCameraID;
    pCamToRTP->nTranmit = 1;
    pCamToRTP->InviteType = InviteType;

    LPINVITECALLID pInviteCallID = new INVITECALLID;
    pInviteCallID->sInviteMsg1 = sInviteMsg1;
    pInviteCallID->sInviteCallID1 = sCallID1;
    pCamToRTP->listSendStreamCallID.push_back(pInviteCallID);

    (*RTPPos)->listCameraToRTP.push_back(pCamToRTP);   
         
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "分配RTP流媒体[%s@%s:%d], 当前总转发数[%d], 镜头[%s]是否转发[%d].",
        sRTPServerID.c_str(), sRTPServerIP.c_str(), nRTPServerPort, (*RTPPos)->nTotal, sCameraID.c_str(), bTransmit);
    LeaveCriticalSection(&m_cs);
    return true;
}
bool RTPServerManager::PushRTPInfo(string sRTPID, string sCameraID, string sCameraUri, int nCameraType, string sSSRC, string sMediaRecvID, string sMediaRecvUri, string sCallID1, 
                                    string sCallID2, string sCallID4, string sInviteMsg1, string sInviteMsg2, string sInviteMsg4)
{
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        if ((*RtpIt)->sRTPID == sRTPID)
        {
            CAMIT CamIt = (*RtpIt)->listCameraToRTP.begin();
            for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
            {
                if ((*CamIt)->sCameraID == sCameraID)
                {
                    INVITEIT InviteIt = (*CamIt)->listSendStreamCallID.begin();
                    for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
                    {
                        if ((*InviteIt)->sInviteCallID1 == sCallID1)
                        {
                            if(sCallID2 != "")  (*CamIt)->sCallID2 = sCallID2;
                            if(sInviteMsg2 != "") (*CamIt)->sInviteMsg2 = sInviteMsg2;
                            (*CamIt)->sCameraUri = sCameraUri;
                            (*CamIt)->nCameraType = nCameraType;
                            (*CamIt)->sSSRC = sSSRC;
                            (*InviteIt)->sMediaRecvID = sMediaRecvID;
                            (*InviteIt)->sInviteCallID4 = sCallID4;
                            (*InviteIt)->sInviteMsg1 = sInviteMsg1;
                            (*InviteIt)->sInviteMsg4 = sInviteMsg4;
                            (*InviteIt)->sMediaRecvUri = sMediaRecvUri;

                            LeaveCriticalSection(&m_cs);
                            return true;
                        }
                    }
                    
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushRTPRecvAnswer(string sCallID2, string sInvite2Answer)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            if((*CamIt)->sCallID2 == sCallID2)
            {
                (*CamIt)->sInvite2Answer = sInvite2Answer;

                LeaveCriticalSection(&m_cs);
                return true;
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushCameraInfo(string sCameraID, string sCallID2, string sCallID3, string sInviteMsg3)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            if((*CamIt)->sCallID2 == sCallID2 && (*CamIt)->sCameraID == sCameraID)
            {
                (*CamIt)->sCallID3 = sCallID3;
                (*CamIt)->sInviteMsg3 = sInviteMsg3;
                LeaveCriticalSection(&m_cs);
                return true;
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushCameraAnswer(string sCallID3, string sInvite3Answer, string sSSRC)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            if((*CamIt)->sCallID3 == sCallID3)
            {
                (*CamIt)->sInvite3Answer = sInvite3Answer;
                (*CamIt)->sSSRC = sSSRC;
                LeaveCriticalSection(&m_cs);
                return true;
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushDecodeInfo(string sDecoderID, string sCallID5, string sInviteMsg5)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
            {
                if((*InviteIt)->sCallID5 == sCallID5 && (*InviteIt)->sMediaRecvID == sDecoderID)
                {
                    (*InviteIt)->sInviteMsg5 = sInviteMsg5;
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
                
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushDecodeAnswer(string sCallID5, string sInvite5Answer)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sCallID5 == sCallID5)
                {
                    (*InviteIt)->sInvite5Answer = sInvite5Answer;

                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }           
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushRTPSendInfo(string sCallID1, string sCallID4, string sInviteMsg4)
{
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;

    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
            {
                if((*InviteIt)->sInviteCallID1 == sCallID1 )
                {
                    (*InviteIt)->sInviteCallID4 = sCallID4;
                    (*InviteIt)->sInviteMsg4 = sInviteMsg4;
                    LeaveCriticalSection(&m_cs);
                    return true;
                }

            }
        }
    }

    LeaveCriticalSection(&m_cs);
    return false;;
}
bool RTPServerManager::PushRTPSendAnswer(string sCallID4, string sInvite4Answer)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sInviteCallID4 == sCallID4)
                {
                    (*InviteIt)->sInvite4Answer = sInvite4Answer;

                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::PushClientAnswer(string sCallID1, string sInvite1Answer)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sInviteCallID1 == sCallID1)
                {
                    (*InviteIt)->sInvite1Answer = sInvite1Answer;
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::SetRecvACK(string sCallID1)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sInviteCallID1 == sCallID1)
                {
                    (*InviteIt)->bRecvInvite1ACK = true;
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::GetRecvACK(string sCallID1)
{
    bool bRet = false;
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sInviteCallID1 == sCallID1)
                {
                    bRet = (*InviteIt)->bRecvInvite1ACK;
                    LeaveCriticalSection(&m_cs);
                    return bRet;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::AddPlaybackCseq(string sCallID1)
{
    EnterCriticalSection(&m_cs);
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for(; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt ++)
            {
                if((*InviteIt)->sInviteCallID1 == sCallID1)
                {
                    (*CamIt)->nCamCseq ++;
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
int RTPServerManager::ReduceRTPTotal(string sCallID1, string sCallID4, bool bDeleteCamera)
{  
    EnterCriticalSection(&m_cs);

    printf("\n---------------------------------\n");
    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;

    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
            {
                if ( (sCallID1 != "" && (*InviteIt)->sInviteCallID1 == sCallID1) || (sCallID4 != "" && (*InviteIt)->sInviteCallID4 == sCallID4))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#准备删除镜头[%s]相关转发信息, 当前转发[%d], DeleteCamera[%d].", 
                                                (*CamIt)->sCameraID.c_str(), (*CamIt)->nTranmit, bDeleteCamera);
                    if ((*CamIt)->nTranmit <= 1 || bDeleteCamera)
                    {
                        (*RtpIt)->nTotal -= (*CamIt)->nTranmit;
                        if((*RtpIt)->nTotal < 0)
                        {
                            (*RtpIt)->nTotal = 0;
                        }
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#删除镜头[%s]取流会话消息, CallID2[%s], CallID3[%s].",
                            (*CamIt)->sCameraID.c_str(), (*CamIt)->sCallID2.c_str(), (*CamIt)->sCallID3.c_str());                       

                        delete (*CamIt);
                        (*CamIt) = NULL;
                        (*RtpIt)->listCameraToRTP.erase(CamIt);

                        printf("---------------------------------\n");

                        LeaveCriticalSection(&m_cs);
                        return 0;
                    }
                    else
                    {
                        (*RtpIt)->nTotal -- ;
                        (*CamIt)->nTranmit--;
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#删除镜头[%s]发流会话消息, CallID1[%s], CallID4[%s].",
                            (*CamIt)->sCameraID.c_str(), (*InviteIt)->sInviteCallID1.c_str(), (*InviteIt)->sInviteCallID4.c_str());

                       
                        delete (*InviteIt);
                        (*InviteIt) = NULL;
                        (*CamIt)->listSendStreamCallID.erase(InviteIt);

                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#删除完成镜头[%s]相关转发信息, 删除后转发数[%d].", 
                            (*CamIt)->sCameraID.c_str(), (*CamIt)->nTranmit);
                        printf("---------------------------------\n");
                        LeaveCriticalSection(&m_cs);
                        return (*CamIt)->nTranmit;
                    }       

                    
                }

            }
        }
    }

    LeaveCriticalSection(&m_cs);
    return -1;
}
RESPONSETYPE RTPServerManager::GetAnswerType(string sCallID, LPPLAYINFO pPlayInfo)
{
    RESPONSETYPE rep = REPNONE;
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt = (*RtpIt)->listCameraToRTP.end();
    INVITEIT InviteIt;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {      
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {           
            InviteIt = (*CamIt)->listSendStreamCallID.end();
            if((*CamIt)->sCallID2 == sCallID)
            {
                rep = REPRTPRECVSTREAM;                
                break;
            }
            else if((*CamIt)->sCallID3 == sCallID) 
            {
                rep = REPCAMERASEND;
                break;
            }
            else
            {
                InviteIt = (*CamIt)->listSendStreamCallID.begin();
                for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
                {
                    if((*InviteIt)->sInviteCallID1 == sCallID)
                    {
                        rep = REPCLIENT;
                        break;
                    }
                    else if((*InviteIt)->sCallID5 == sCallID) 
                    {
                        rep = REPDECODERRECV;
                        break;
                    }
                    else if((*InviteIt)->sInviteCallID4 == sCallID)
                    {
                        rep = REPRTPSENDSTREAM;                       
                        break;
                    }                    
                }
                if(rep != REPNONE)
                {
                    break;
                }
            }            
        }
        if(rep != REPNONE)
        {
            break;
        }
    }

    if(rep != REPNONE)
    {
        pPlayInfo->sRTPServerID = (*RtpIt)->sRTPID;
        pPlayInfo->sRTPUri = (*RtpIt)->sRTPUri;
        pPlayInfo->sRTPServerIP = (*RtpIt)->sRTPIP;
        pPlayInfo->nRTPServerPort = (*RtpIt)->nRTPPort;

        if(CamIt != (*RtpIt)->listCameraToRTP.end())
        {
            pPlayInfo->sCameraID = (*CamIt)->sCameraID;
            pPlayInfo->nCameraType = (*CamIt)->nCameraType;
            pPlayInfo->sCameraUri = (*CamIt)->sCameraUri;
            pPlayInfo->nCamCseq = (*CamIt)->nCamCseq;
            pPlayInfo->nTranmit = (*CamIt)->nTranmit;
            pPlayInfo->sCallID2 = (*CamIt)->sCallID2;
            pPlayInfo->sCallID3 = (*CamIt)->sCallID3;
            pPlayInfo->InviteType = (*CamIt)->InviteType;
            pPlayInfo->sInviteMsg2 = (*CamIt)->sInviteMsg2;
            pPlayInfo->sInvite2Answer = (*CamIt)->sInvite2Answer;
            pPlayInfo->sInviteMsg3 = (*CamIt)->sInviteMsg3;
            pPlayInfo->sInvite3Answer = (*CamIt)->sInvite3Answer;
            pPlayInfo->sSSRC = (*CamIt)->sSSRC;
        }
        if(InviteIt == (*CamIt)->listSendStreamCallID.end())
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
        }
       
        if(InviteIt != (*CamIt)->listSendStreamCallID.end())
        {
            pPlayInfo->sCallID1 = (*InviteIt)->sInviteCallID1;
            pPlayInfo->sCallID5 = (*InviteIt)->sCallID5;
            pPlayInfo->sCallID4 = (*InviteIt)->sInviteCallID4;
            pPlayInfo->sMediaRecverID = (*InviteIt)->sMediaRecvID;
            pPlayInfo->sMediaRecvUri = (*InviteIt)->sMediaRecvUri;
            pPlayInfo->sInviteMsg1 = (*InviteIt)->sInviteMsg1;
            pPlayInfo->sInvite1Answer = (*InviteIt)->sInvite1Answer;
            pPlayInfo->sInviteMsg4 = (*InviteIt)->sInviteMsg4;
            pPlayInfo->sInvite4Answer = (*InviteIt)->sInvite4Answer;
            pPlayInfo->sInviteMsg5 = (*InviteIt)->sInviteMsg5;
            pPlayInfo->sInvite5Answer = (*InviteIt)->sInvite5Answer;

        }       
    }
    LeaveCriticalSection(&m_cs);
    return rep;
}
bool RTPServerManager::GetPlayInfo(string sRTPID, LPPLAYINFO pPlayInfo)
{
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt = (*RtpIt)->listCameraToRTP.end();
    INVITEIT InviteIt ;
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {      
        if((*RtpIt)->sRTPID == sRTPID)
        {
            CamIt = (*RtpIt)->listCameraToRTP.begin();
            for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
            {                             
                InviteIt = (*CamIt)->listSendStreamCallID.begin();
                for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
                {
                    pPlayInfo->sRTPServerID = (*RtpIt)->sRTPID;
                    pPlayInfo->sRTPUri = (*RtpIt)->sRTPUri;
                    pPlayInfo->sRTPServerIP = (*RtpIt)->sRTPIP;
                    pPlayInfo->nRTPServerPort = (*RtpIt)->nRTPPort;

                    pPlayInfo->sCameraID = (*CamIt)->sCameraID;
                    pPlayInfo->nCameraType = (*CamIt)->nCameraType;
                    pPlayInfo->sCameraUri = (*CamIt)->sCameraUri;
                    pPlayInfo->nCamCseq = (*CamIt)->nCamCseq;
                    pPlayInfo->nTranmit = (*CamIt)->nTranmit;
                    pPlayInfo->sCallID2 = (*CamIt)->sCallID2;
                    pPlayInfo->sCallID3 = (*CamIt)->sCallID3;
                    pPlayInfo->InviteType = (*CamIt)->InviteType;
                    pPlayInfo->sInviteMsg2 = (*CamIt)->sInviteMsg2;
                    pPlayInfo->sInvite2Answer = (*CamIt)->sInvite2Answer;
                    pPlayInfo->sInviteMsg3 = (*CamIt)->sInviteMsg3;
                    pPlayInfo->sInvite3Answer = (*CamIt)->sInvite3Answer;
                    pPlayInfo->sSSRC = (*CamIt)->sSSRC;


                    pPlayInfo->sCallID1 = (*InviteIt)->sInviteCallID1;
                    pPlayInfo->sCallID5 = (*InviteIt)->sCallID5;
                    pPlayInfo->sCallID4 = (*InviteIt)->sInviteCallID4;
                    pPlayInfo->sMediaRecverID = (*InviteIt)->sMediaRecvID;
                    pPlayInfo->sMediaRecvUri = (*InviteIt)->sMediaRecvUri;
                    pPlayInfo->sInviteMsg1 = (*InviteIt)->sInviteMsg1;
                    pPlayInfo->sInvite1Answer = (*InviteIt)->sInvite1Answer;
                    pPlayInfo->sInviteMsg4 = (*InviteIt)->sInviteMsg4;
                    pPlayInfo->sInvite4Answer = (*InviteIt)->sInvite4Answer;
                    pPlayInfo->sInviteMsg5 = (*InviteIt)->sInviteMsg5;
                    pPlayInfo->sInvite5Answer = (*InviteIt)->sInvite5Answer;     

                    LeaveCriticalSection(&m_cs);
                    return true;
                }         
            }
        }       
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
string RTPServerManager::GetCameraIDByCallID(string sCallID1)
{
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;

    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
            {
                if ((*InviteIt)->sInviteCallID1 == sCallID1)
                {
                    string sCameraID = (*CamIt)->sCameraID;
                    LeaveCriticalSection(&m_cs);
                    return sCameraID;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return "";
}
string RTPServerManager::GetCameraIDByMonitorID(string sMonitorID)
{
    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;

    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {
            InviteIt = (*CamIt)->listSendStreamCallID.begin();
            for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
            {
                if ((*InviteIt)->sMediaRecvID == sMonitorID)
                {
                    string sCameraID = (*CamIt)->sCameraID;
                    LeaveCriticalSection(&m_cs);
                    return sCameraID;
                }
            }
        }
    }
    LeaveCriticalSection(&m_cs);
    return "";
}
bool RTPServerManager::GetPlayInfoByID(string sID, bool bAll, LPPLAYINFO pPlayInfo)
{
    bool bUser = false;
    string sType(sID, 10, 3);
    if (sType[0] == '3' || sType == "133")
    {
        bUser = true;
    }

    EnterCriticalSection(&m_cs);

    RTPIT RtpIt = m_listRTPInfo.begin();
    CAMIT CamIt;
    INVITEIT InviteIt;

    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        CamIt = (*RtpIt)->listCameraToRTP.begin();
        for (; CamIt != (*RtpIt)->listCameraToRTP.end(); CamIt++)
        {           
            if (!bUser)          //如果是设备
            {
                //找到对应的设备, 根据bClearALL参数决定是否取只取实时播放信息
                if ((*CamIt)->sCameraID == sID && ((*CamIt)->InviteType ==  XSIP_INVITE_PLAY || bAll) )  
                {
                    InviteIt = (*CamIt)->listSendStreamCallID.begin();
                    if (InviteIt == (*CamIt)->listSendStreamCallID.end())
                    {
                        (*RtpIt)->listCameraToRTP.erase(CamIt);
                        LeaveCriticalSection(&m_cs);
                        return false;
                    }

                    pPlayInfo->sRTPServerID = (*RtpIt)->sRTPID;
                    pPlayInfo->sRTPUri = (*RtpIt)->sRTPUri;
                    pPlayInfo->sRTPServerIP = (*RtpIt)->sRTPIP;
                    pPlayInfo->nRTPServerPort = (*RtpIt)->nRTPPort;

                    pPlayInfo->sCameraID = (*CamIt)->sCameraID;
                    pPlayInfo->sCameraUri = (*CamIt)->sCameraUri;
                    pPlayInfo->nCameraType = (*CamIt)->nCameraType;
                    pPlayInfo->nCamCseq = (*CamIt)->nCamCseq;
                    pPlayInfo->nTranmit = (*CamIt)->nTranmit;
                    pPlayInfo->sCallID2 = (*CamIt)->sCallID2;
                    pPlayInfo->sCallID3 = (*CamIt)->sCallID3;
                    pPlayInfo->InviteType = (*CamIt)->InviteType;
                    pPlayInfo->sInviteMsg2 = (*CamIt)->sInviteMsg2;
                    pPlayInfo->sInvite2Answer = (*CamIt)->sInvite2Answer;
                    pPlayInfo->sInviteMsg3 = (*CamIt)->sInviteMsg3;
                    pPlayInfo->sInvite3Answer = (*CamIt)->sInvite3Answer;
                    pPlayInfo->sSSRC = (*CamIt)->sSSRC;


                    pPlayInfo->sCallID1 = (*InviteIt)->sInviteCallID1;
                    pPlayInfo->sCallID5 = (*InviteIt)->sCallID5;
                    pPlayInfo->sCallID4 = (*InviteIt)->sInviteCallID4;
                    pPlayInfo->sMediaRecverID = (*InviteIt)->sMediaRecvID;
                    pPlayInfo->sMediaRecvUri = (*InviteIt)->sMediaRecvUri;
                    pPlayInfo->sInviteMsg1 = (*InviteIt)->sInviteMsg1;
                    pPlayInfo->sInvite1Answer = (*InviteIt)->sInvite1Answer;
                    pPlayInfo->sInviteMsg4 = (*InviteIt)->sInviteMsg4;
                    pPlayInfo->sInvite4Answer = (*InviteIt)->sInvite4Answer;
                    pPlayInfo->sInviteMsg5 = (*InviteIt)->sInviteMsg5;
                    pPlayInfo->sInvite5Answer = (*InviteIt)->sInvite5Answer;   

                
                    LeaveCriticalSection(&m_cs);
                    return true;
                }
                else
                {
                    continue;
                }
            }
            else            //如果是媒体流接收者(解码器或用户)
            {
                InviteIt = (*CamIt)->listSendStreamCallID.begin();
                for (; InviteIt != (*CamIt)->listSendStreamCallID.end(); InviteIt++)
                {
                    if ((*InviteIt)->sMediaRecvID == sID)  
                    {
                        pPlayInfo->sRTPServerID = (*RtpIt)->sRTPID;
                        pPlayInfo->sRTPUri = (*RtpIt)->sRTPUri;
                        pPlayInfo->sRTPServerIP = (*RtpIt)->sRTPIP;
                        pPlayInfo->nRTPServerPort = (*RtpIt)->nRTPPort;

                        pPlayInfo->sCameraID = (*CamIt)->sCameraID;
                        pPlayInfo->sCameraUri = (*CamIt)->sCameraUri;
                        pPlayInfo->nCamCseq = (*CamIt)->nCamCseq;
                        pPlayInfo->nTranmit = (*CamIt)->nTranmit;
                        pPlayInfo->sCallID2 = (*CamIt)->sCallID2;
                        pPlayInfo->sCallID3 = (*CamIt)->sCallID3;
                        pPlayInfo->InviteType = (*CamIt)->InviteType;
                        pPlayInfo->sInviteMsg2 = (*CamIt)->sInviteMsg2;
                        pPlayInfo->sInvite2Answer = (*CamIt)->sInvite2Answer;
                        pPlayInfo->sInviteMsg3 = (*CamIt)->sInviteMsg3;
                        pPlayInfo->sInvite3Answer = (*CamIt)->sInvite3Answer;
                        pPlayInfo->sSSRC = (*CamIt)->sSSRC;


                        pPlayInfo->sCallID1 = (*InviteIt)->sInviteCallID1;
                        pPlayInfo->sCallID5 = (*InviteIt)->sCallID5;
                        pPlayInfo->sCallID4 = (*InviteIt)->sInviteCallID4;
                        pPlayInfo->sMediaRecverID = (*InviteIt)->sMediaRecvID;
                        pPlayInfo->sMediaRecvUri = (*InviteIt)->sMediaRecvUri;
                        pPlayInfo->sInviteMsg1 = (*InviteIt)->sInviteMsg1;
                        pPlayInfo->sInvite1Answer = (*InviteIt)->sInvite1Answer;
                        pPlayInfo->sInviteMsg4 = (*InviteIt)->sInviteMsg4;
                        pPlayInfo->sInvite4Answer = (*InviteIt)->sInvite4Answer;
                        pPlayInfo->sInviteMsg5 = (*InviteIt)->sInviteMsg5;
                        pPlayInfo->sInvite5Answer = (*InviteIt)->sInvite5Answer;   

                        LeaveCriticalSection(&m_cs);
                        return true;
                    }
                }
            }
            
        }
    }
    LeaveCriticalSection(&m_cs);
    return false;
}
bool RTPServerManager::GetFileStreamServerInfo(string sServerIP, string & sFileServerID, int & nFileServerPort)
{
    RTPIT RtpIt = m_listRTPInfo.begin();
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        if ((*RtpIt)->nType == FILEMEDIASERVERTYPE  && (*RtpIt)->sRTPIP == sServerIP)
        {
            sFileServerID = (*RtpIt)->sRTPID;
            nFileServerPort = (*RtpIt)->nRTPPort;

            return true;
        }
    }
    g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "未找到指定IP[%s]的文件流媒体!", sServerIP.c_str());
    return false;
}
int RTPServerManager::GetStreamNum()
{
    int nNum = 0;
    RTPIT RtpIt = m_listRTPInfo.begin();
    for (; RtpIt != m_listRTPInfo.end(); RtpIt++)
    {
        nNum += (*RtpIt)->nTotal;
    }
    return nNum;
}
