#include "StdAfx.h"
#include "SipServer.h"
#include "NVSSDKFunctions.h"
#include <iostream>

CLogRecorder g_LogRecorder;
CSIPServer * m_pCSipServer;
CSIPServer::CSIPServer(void)
: m_bRegSupServer(0)
, m_bRegisterOut(0)
, m_bKeepalive(1)
, m_bSub(0)
, m_bAuth(0)
, m_dwSn(10000)
, m_dwSSRC(1)
, m_hAlarm(0)
, m_bAlarm(FALSE)
, m_bForWard(false)
, m_nCatalogNum(0)
, m_nRecordNum(0)
, m_nTimeoutCount(0)
, m_bStopSave(0)
, m_bNotResponse(0)
{
    m_dwSSRC = xsip_get_num()%10000;
	InitializeCriticalSection(&m_vLock);
	InitializeCriticalSection(&m_sLock);
    InitializeCriticalSection(&m_csLock);
	InitializeCriticalSection(&m_PTZLock);
    InitializeCriticalSection(&m_csInitial);
    InitializeCriticalSection(&m_SmartLock);
	m_hStopEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
    m_pSIPProtocol = new CSIPProtocol;
    m_pCSipServer = this;
    m_sUpdateTime = "";
    m_pRTPManager = new RTPServerManager;

    m_sChannelInfoTable = "xy_base.channelinfo";
    m_sDVRInfoTable = "xy_base.dvrinfo";
    m_sChanGroupInfoTable = "xy_base.ChanGroupInfo";
    m_sChanGroupToChanTable = "xy_base.changrouptochan";

    m_sSIPChannelTable = "xy_base.sipchannel";
    m_sSIPDVRTable = "xy_base.sipdvr";
    m_sSIPDVRToServerTable = "xy_base.sipdvrtoserver";

    m_sDataChangeTable = "XY_MONITOR.DATACHANGE";
    
}

CSIPServer::~CSIPServer(void)
{
	m_bStopSave = TRUE;
	m_bRegSupServer = FALSE;
	m_bKeepalive = FALSE;
	if (m_hStopEvent != NULL)
	{
		CloseHandle(m_hStopEvent);
		m_hStopEvent = NULL;
	}
	CoUninitialize();
	DeleteCriticalSection(&m_vLock);
	DeleteCriticalSection(&m_sLock);
    DeleteCriticalSection(&m_csLock);
	DeleteCriticalSection(&m_PTZLock);
    DeleteCriticalSection(&m_csInitial);
    DeleteCriticalSection(&m_SmartLock);
}

bool CSIPServer::Init(bool bUpdate)
{
    
    if (!m_pam.InitParam())
    {
        if(!bUpdate)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ȡ�����ļ�ʱ��������!");
        }
        return false;
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ȡ�����ļ��ɹ�!");
    }

    m_sSIPServerID = m_pam.m_sSIPServerID;
    int nPos = string(m_sSIPServerID, 0, 10).find_last_not_of("0");
    if(nPos % 2 == 0) nPos ++;
    m_sRealm.assign(m_sSIPServerID, 0, nPos + 1);
    m_sSIPServerIP =  m_pam.m_sLocalIP;
    m_nSIPServerPort =  m_pam.m_nLocalPort;
    xsip_init_t(m_sSIPServerID.c_str(), m_pam.m_sLocalPassword.c_str(), m_sSIPServerIP.c_str(), m_nSIPServerPort);  

    m_bSub = m_pam.m_bSubList;
    m_bAuth = m_pam.m_bAuthenticate;
    if(m_pam.m_sAssignLowServer == "")
    {
        m_DBMgr.SetConnectString(m_pam.m_nDBDriver, m_pam.m_sDBServer, m_pam.m_sDBName, m_pam.m_sDBUid, m_pam.m_sDBPwd);
        if (!m_DBMgr.ConnectDB())
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : �������ݿ�ʧ��!");
            return false;
        }

        if(m_pam.m_sLGDBServer != "")
        {
            m_LGDBMgr.SetConnectString(m_pam.m_nDBDriver, m_pam.m_sLGDBServer, m_pam.m_sLGDBName, m_pam.m_sLGDBUid, m_pam.m_sLGDBPwd);
            if (!m_LGDBMgr.ConnectDB())
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : �����������ݿ�ʧ��!");
                return false;
            }
        }

        if(!GetServerInfo())   //����SIPSERVERINFO���ȡsip���������Ϣ
        {
            return false;
        }
        if(!GetRTPStreamInfo())     //�����ݿ��SIPServerInfo��ȡ�õ�ǰSIP��������Ӧ����ý�������Ϣ
        {
            return false;
        }

        if(!GetDeviceInfo(bUpdate))
        {
            return false;
        }
        if(bUpdate)
        {
            if(!UpdateDeviceToSIPServer())
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning:  �����豸ָ��SIP������Ϣʧ��!");
                return false;
            }
        }

        m_DBMgr.DisconnectDB();
        if (m_pam.m_sLGDBServer != "")
        {
            m_LGDBMgr.DisconnectDB();
        }

       
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Info: ǿ��ָ���¼�SIP����[%s].", m_pam.m_sAssignLowServer.c_str());
        SetAssignRTPServer(m_pam.m_sAssignRTPServer);
    }


    //�ϼ�������Ϣ
    if(m_pam.m_bRegSupServer)
    {
        list<string> listSup;
        m_bRegSupServer = m_pam.m_bRegSupServer;
        char *result = strtok((char *)m_pam.m_sSuperiorServer.c_str(), ";");
        while (result)
        {
            string sSupID = xsip_get_url_username(result);
            listSup.push_back(sSupID);
            DVRINFOMAP::iterator itSup = m_mapSuperior.find(sSupID);
            if(itSup != m_mapSuperior.end())
            {
                strcpy_s(itSup->second->pDeviceUri, sizeof(itSup->second->pDeviceUri), result);
            }
            else
            {
                LPDVRINFO pRegInfo = new DVRINFO;
                strcpy_s(pRegInfo->pDeviceUri, sizeof(pRegInfo->pDeviceUri), result);
                m_mapSuperior.insert(make_pair(sSupID, pRegInfo));
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "...�����ϼ�[%s].", pRegInfo->pDeviceUri);
            }
            result = strtok(NULL, ";");
        }

        DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
        while(itSup != m_mapSuperior.end())
        {
            list<string>::iterator itList = listSup.begin();
            for(; itList != listSup.end(); itList ++)
            {
                if((*itList) == itSup->first)
                {
                    break;
                }
            }
            if(itList == listSup.end())
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "...ɾ���ϼ�[%s].", itSup->second->pDeviceUri);
                itSup = m_mapSuperior.erase(itSup);
            }
            else
            {
                itSup ++;
            }
        }
        listSup.clear();
    }
    

    

    string sNetType = m_pam.m_nNetType == 1 ? "RTP/UDP" : "RTP/TCP";
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "RTP��ý�����������[%s].", sNetType.c_str());

    return true;
}

bool CSIPServer::StartRun()
{
    string sFilePath = m_pam.GetDir();
    sFilePath += string("/Config/XSIPServer_config.properties");
#ifdef _DEBUG
    sFilePath = "./Config/XSIPServer_config.properties";
#endif
    g_LogRecorder.InitLogger(sFilePath.c_str(), "XSIPServerLogger", "XSIPServer");

   if(!Init())
   {
       g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: SIP�����ʼ��ʧ��!");
   }
    
    if(!m_pSIPProtocol->init(m_sSIPServerIP, m_nSIPServerPort, ParseMessage))
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��������ʧ��!");
        return false;
    }

    CreateThread(NULL, 0, ReadDBThread, this, 0, NULL);      //��ʱ��ȡ���ݿ���·�����Ϣ
    CreateThread(NULL, 0, DeviceStatusThread, this, 0, NULL);  //�����̼߳���豸|�û�����״̬, Ŀǰ�������ͻ�������״̬.
    CreateThread(NULL, 0, CheckTimeThread, this, 0, NULL);  //�����̼߳��¼���ѯ, Ŀ¼��ѯ������Ϣ.
    CreateThread(NULL, 0, CheckRTPStreamThread, this, 0, NULL); //����RTP��ý��״̬����̡߳�
    CreateThread(NULL, 0, SaveCatalogInfoThread, this, 0, NULL); //�����洢�¼��豸|ƽ̨��ͷĿ¼�̡߳�
    CreateThread(NULL, 0, SuperiorKeepaliveThread, this, 0, NULL);	//�������ϼ�ƽ̨������                       

    if(m_pam.m_bISINITIAL)
    {
        //CreateThread(NULL, 0, CheckIsInitialThread, this, 0, NULL);  //�����߳�
    }

    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n======== SIP���������ɹ�[%s@%s:%d] ========\n\n",  
        m_sSIPServerID.c_str(), m_pSIPProtocol->m_sLocalIP.c_str(), m_pSIPProtocol->m_nLocalPort);

    if (m_bRegSupServer)	//�Ƿ�Ҫ���ϼ�ƽ̨ע��
    {
        SuperiorSendRegMsg();
    }

    GetLocalTime(&m_StartUpTime);
    char pTime[32] = {0};
    sprintf_s(pTime, sizeof(pTime), 
        "%04d-%02d-%02d %02d:%02d:%02d", 
        m_StartUpTime.wYear, m_StartUpTime.wMonth, m_StartUpTime.wDay, 
        m_StartUpTime.wHour, m_StartUpTime.wMinute, m_StartUpTime.wSecond);

#ifdef _DEBUG
    char p;
    while(true)
    {
        p = getchar();
        int nNum = 1;
        switch(p)
        {
        case 'a': case 'A':
            {
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                printf("***[%s@%s:%d]***\n", m_sSIPServerID.c_str(), m_sSIPServerIP.c_str(), m_nSIPServerPort);
                printf("����ʱ��: %s\n", pTime);
                EnterCriticalSection(&m_vLock);
                printf("-------------------------\n�¼��豸|ƽ̨�б�:\n");
                DVRINFOMAP::iterator itDev = m_mapDevice.begin();
                for(; itDev!= m_mapDevice.end(); itDev ++)
                {
                    string sID = xsip_get_url_username((char*)itDev->second->pDeviceUri);
                    if(IsUser(sID.c_str()) || string(itDev->second->pSIPServerCode) != m_sSIPServerID)
                    {
                        continue;
                    }
                    string sReg = itDev->second->bReg ? "Reg" : "UnReg";
                    printf("  %s [%s]\n", itDev->second->pDeviceUri, sReg.c_str());
                }
                printf("-------------------------\nע���û��б�:\n");
                itDev = m_mapDevice.begin();
                for(; itDev != m_mapDevice.end(); itDev ++)
                {
                    string sID = xsip_get_url_username((char*)itDev->second->pDeviceUri);
                    if(IsUser(sID.c_str()))
                    {
                        string sReg = itDev->second->bReg ? "Reg" : "UnReg";
                        printf("  %s [%s]\n", itDev->second->pDeviceUri, sReg.c_str());
                    }

                }
                LeaveCriticalSection(&m_vLock);

                nNum = 1;
                printf("-------------------------\n������Ϣ:\n");
                RTPIT rtpit = m_pRTPManager->m_listRTPInfo.begin();
                for(; rtpit != m_pRTPManager->m_listRTPInfo.end(); rtpit++)
                {
                    string sOnline = (*rtpit)->bOnLine ? "Y" : "N";
                    printf("  RTP[%s], OnLine[%s], Total[%d]\n",
                        (*rtpit)->sRTPUri.c_str(), sOnline.c_str(), (*rtpit)->nTotal);
                    for(CAMIT CamIt = (*rtpit)->listCameraToRTP.begin(); CamIt != (*rtpit)->listCameraToRTP.end(); CamIt ++)
                    {
                        printf("     Camera%02d[%s], InviteType[%d]\n", nNum++, (*CamIt)->sCameraID.c_str(), (*CamIt)->InviteType);
                        for(INVITEIT InvIt = (*CamIt)->listSendStreamCallID.begin(); InvIt != (*CamIt)->listSendStreamCallID.end(); InvIt ++)
                        {
                            printf("        User[%s], CallID[%s]\n", (*InvIt)->sMediaRecvID.c_str(), (*InvIt)->sInviteCallID1.c_str());
                        }
                    }                   
                }

                printf("-------------------------\n\n����Ự��Ϣ:\n");
                EnterCriticalSection(&m_sLock);
                SessionMap::iterator itsm = m_SessionMap.begin();
                while(itsm != m_SessionMap.end())
                {
                    printf("\t��ͷ%02d[%s], Title[%s], CallID[%s], ������[%s]\n", nNum,
                        itsm->second.sDeviceID.c_str(), itsm->second.sTitle.c_str(), itsm->first.c_str(), itsm->second.sAddr.c_str());

                    nNum ++;
                    itsm ++;
                }
                LeaveCriticalSection(&m_sLock);
                printf("\n^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            }            

            break; 
        case 'm': case 'M':
            {
                char pDeviceID[1024] = {0};
                do 
                {
                    cout << "�����¼�ƽ̨���豸����: ";
                    cin >> pDeviceID;
                    if(pDeviceID[0] == 'b' || pDeviceID[0] == 'B')
                    {
                        break;
                    }
                    if(strlen(pDeviceID) != 20)
                    {
                        cout << "�����Ŵ���!" << endl;
                    }
                } while (strlen(pDeviceID) != 20);

                if(pDeviceID[0] == 'b' || pDeviceID[0] == 'B')
                {
                    break;
                }

                EnterCriticalSection(&m_vLock);
                DVRINFOMAP::iterator itDev = m_mapDevice.find(pDeviceID);
                SOCKADDR_IN sockin;
                if(itDev != m_mapDevice.end())
                {
                    if(itDev->second->bReg && !IsUser(pDeviceID))
                    {
                        SetDestAddr(itDev->second->pDeviceUri, &sockin);

                        if(!CreateChanGroupInfo(pDeviceID))
                        {
                            g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "��ChanGroupInfo�д�����ͷ��[%s]ʧ��.", pDeviceID);
                        }
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��[%s]����Ŀ¼��������..", itDev->second->pDeviceUri);
                        SendCatalogMsg(sockin, itDev->second->pDeviceUri);

                    }
                }
                LeaveCriticalSection(&m_vLock);
            }
            break;
        case 'r': case 'R':
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^^^���¶�ȡ����|�豸��Ϣ...");
            GetServerInfo();
            GetRTPStreamInfo();
            GetDeviceInfo();
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^^^��ȡ����.");
            break;
        default:
            break;
        }
    }

#else
    WaitForSingleObject(m_hStopEvent, INFINITE) == WAIT_OBJECT_0;
#endif
    return true;
}

void CSIPServer::StopRun()
{
	m_bStopSave = TRUE;
	m_bRegSupServer = FALSE;
	m_bKeepalive = FALSE;
	SetEvent(m_hStopEvent);
    
}

void CSIPServer::ParseMessage(SOCKADDR_IN &addr, string RecvBuf)
{
    char pMsg[DATA_BUFSIZE] = {0};
    memcpy(pMsg, RecvBuf.c_str(), RecvBuf.size());
    m_pCSipServer->action(addr, pMsg);
}
int CSIPServer::action(SOCKADDR_IN &addr, char *msg)
{
    string sMsg(msg);
    if(sMsg.find("KeepRTPStreamAlive") != string::npos)
    {
        SetRTPServerStatus(addr, msg);
        return 0;
    }

	xsip_event_type nEvent = xsip_get_event_type(msg);
	switch (nEvent)
	{
    case XSIP_DEVICE_QUERYINFO:
        {
            ServerQuertInfoRecv(addr, msg);
            break;
        }
    case XSIP_REGSUPSERVER_SUCCESS:	case XSIP_REGSUPSERVER_FAILURE:	//���ϼ� ע��/ע�� �Ļ�Ӧ��Ϣ����
	{
		SuperiorRegAnswer(addr, msg);
		break;
	}
    
	case XSIP_SUBSCRIPTION_NEW:			//�յ��ϼ�ƽ̨Ŀ¼������Ϣ
	{
        SuperiorSubscribeAnswer(addr, msg);
		break;
	}
    case XSIP_DEVICE_REG:   //�¼����͵����� ע��|ע�� ��Ϣ
    {
        EnterCriticalSection(&m_csLock);
        DeviceRegAnswer(addr, msg);
        LeaveCriticalSection(&m_csLock);
        break;
    }
    case XSIP_SUBSCRIPTION_ANSWERED:	//���ĳɹ�
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¼�ƽ̨|�豸����Ŀ¼�ɹ�!");
        break;
    }
    case XSIP_SUBSCRIPTION_FAILURE:		//����ʧ��
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¼�ƽ̨|�豸����Ŀ¼ʧ��!");
        break;
    }
    case XSIP_SUBSCRIPTION_NOTIFY:		//���¼�ƽ̨����Ŀ¼������Ϣ��, �յ���Ŀ¼��ͷ��Ϣ����
    {
        DeviceNotifyAnswer(addr, msg);	//���澵ͷĿ¼������Ϣ��xy_base.sipsubscribe��
        break;
    }	   
	case XSIP_CALL_INVITE:		//�յ��ͻ���|�ϼ�ƽ̨���ž�ͷ����
	{
        EnterCriticalSection(&m_csLock);
		InviteMsgRecv(addr, msg);		//�յ�Invite��Ϣ����
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CALL_INVITEANSWERED:    
	{
        EnterCriticalSection(&m_csLock);
        InviteAnswerRecv(addr, msg);	//����Invite��Ϣ����ý����豸��, ��Ӧ����Ϣ����
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CALL_FAILURE:
	{
        EnterCriticalSection(&m_csLock);
        InviteFailure(addr, msg);
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CALL_ACK:
	{           
        EnterCriticalSection(&m_csLock);
        InviteAckRecv(msg);
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CALL_CANCEL:
	{
        EnterCriticalSection(&m_csLock);
        CancelMsgRecv(addr, msg);
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CANCEL_ANSWERED:
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#CancelAns: ȡ����ͷ��������ɹ�!");
		break;
	}
	case XSIP_CANCEL_FAILURE:
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#CancelAns: ȡ����ͷ��������ʧ��!");
		break;
	}
	case XSIP_CALL_BYE:
	{
        EnterCriticalSection(&m_csLock);
        ByeMsgRecv(addr, msg);
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_BYE_ANSWERED:
	{
        ByeAnswerRecv(addr, msg);
		break;
	}
	case XSIP_BYE_FAILURE:
	{
		break;
	}
	case XSIP_INFO_PLAY:
	{
		ReplayControlMsgRecv(addr, msg);
		break;
	}
	case XSIP_INFO_ANSWERED:
	{
		break;
	}
	case XSIP_INFO_FAILURE:
	{
		break;
	}
	case XSIP_MESSAGE_NEW:				/***********************MESSAGE��Ϣ*************************************************************************************************/
	{
		xsip_message_type nMsg = xsip_get_message_type(msg);
		switch (nMsg)
		{
        case XSIP_MESSAGE_CATALOG_QUERY:	//�յ��ϼ�ƽ̨Ŀ¼��ѯ��Ϣ
        {
            SuperiorCatalogAnswer(addr, msg);
            break;
        }
        case XSIP_MESSAGE_AREADETECTION_QUERY: case XSIP_MESSAGE_CROSSDETECTION_QUERY:	//�յ��ϼ�ƽ̨���������Ϣ
        {
            DeviceSmartMsgRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_AREADETECTION_RESPONSE: case XSIP_MESSAGE_CROSSDETECTION_RESPONSE:	//�յ��ϼ�ƽ̨Ŀ¼��ѯ��Ϣ
        {
            DeviceSmartAnswerRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_RECVSMART:
        {
            UserRecvSmartMsgRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_CATALOG_RESPONSE:								//�豸Ŀ¼��ѯ��Ӧ
            {
                DeviceCatalogAnswer(addr, msg);
                break;
            }
		case XSIP_MESSAGE_KEEPALIVE:								//�յ�������Ϣ
			{
				DeviceKeepaliveRecv(addr, msg);
				break;
			}			
		case XSIP_MESSAGE_BOOT_CMD:									//�豸Զ������
			{
				RebootMsgRecv(addr, msg);
				break;
			}					
		case XSIP_MESSAGE_DVRINFO_QUERY:								//�豸��Ϣ��ѯ
			{
				DeviceInfoMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRINFO_RESPONSE:								//�豸��Ϣ��Ӧ
			{
				DeviceInfoAnswerRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRSTATUS_QUERY:								//�豸״̬��ѯ
			{
				DeviceStatusMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRSTATUS_RESPONSE:							//�豸״̬��Ӧ
			{
				DeviceStatusAnswerRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_GUARD_CMD:									//����/������Ϣ
			{
				DeviceGuardMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM:										//����֪ͨ��Ϣ
			{
				DeviceAlarmMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM_RESPONSE:										//����֪ͨ��Ӧ
			{
				CommandResponse(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM_CMD:											//������λ��Ϣ
			{	
				DeviceAlarmMsgAnswer(addr, msg);
				break;
			}
        case XSIP_MESSAGE_RECORD_CMD:
            {		
                DeviceRecordMsgRecv(addr, msg);
                break;
            }
		case XSIP_MESSAGE_PTZ_CMD:
			{
				EnterCriticalSection(&m_PTZLock);
				int nRet = PTZMsgRecv(addr, msg);
				LeaveCriticalSection(&m_PTZLock);
				break;
			}			
		case XSIP_MESSAGE_CMD_RESPONSE:
			{
				CommandResponse(addr, msg);
				break;
			}
		case XSIP_MESSAGE_RECORDINFO:
			{
				RecordInfoMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_RECORDINFO_RESPONSE:
			{
				RecordInfoAnswerRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_MEDIASTATUS:
			{
                ReplayMediaStatusRecv(addr, msg);					
				break;
			}
		}
		break;
	}
	case XSIP_MESSAGE_PROCEEDING:
	{
		break;
	}
	case XSIP_MESSAGE_ANSWERED:									//��ȷ��MESSAGE��Ӧ��Ϣ
	{
        SuperiorKeepaliveAns(addr, msg);
		break;
	}
	case XSIP_MESSAGE_FAILURE:									//�����MESSAGE��Ϣ
	{
		xsip_message_type nMsg = xsip_get_message_type(msg);
		if (nMsg == XSIP_MESSAGE_KEEPALIVE_RESPHONSE)		//��������ʧ��
		{
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ�403 Forbidden����.");
			//SuperiorKeepaliveFailed(addr, msg);
		}
		else
		{
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "Warning: δ֪������!");
			g_LogRecorder.WriteInfoLogEx(__FUNCTION__, msg);
		}
		break;
	}
	default:
	    break;
	}
	return 0;
}
bool CSIPServer::IsSameNet(string sIP1, string sIP2)
{
    if(sIP1.size() < 7 || sIP2.size() < 7)
    {
        return false;
    }
    string s1(sIP1, 0, 3);
    string s2(sIP2, 0, 3);
    if(s1 == s2)
    {
        return true;
    }
    return false;
}

bool CSIPServer::GetServerInfo()
{    
    char szSql[1024] = {0};
    sprintf_s(szSql, sizeof(szSql), "select a.parentcode, a.sipServerCode, b.id, b.ip, b.port, b.externalIP, b.externalPort "
        "from xy_base.sipServerInfo a, xy_base.serverInfo b where a.sipservercode = b.descrip "
        "and b.type = 300");
	otl_stream otlSelect;
	bool nExitSQL = m_DBMgr.ExecuteSQL(szSql, otlSelect);
	if(false == nExitSQL)
		return false;

    //printf("-------------------------------------------------------------------------------\n");

    while(!otlSelect.eof())
    {
        int nServerID = 0;
		char sParentCode[MAX_STRING_LEN] = {0};
		char sServerCode[MAX_STRING_LEN] = {0};
		char sPoliceNetIP[MAX_STRING_LEN] = {0};
		int  nPoliceNetPort = 0;
        char sExteranlIP[MAX_STRING_LEN] = {0};
        int  nExteranlPort = 0;

		otlSelect >> sParentCode >> sServerCode >>nServerID >> sPoliceNetIP >> nPoliceNetPort >> sExteranlIP >> nExteranlPort; 

        if(string(sServerCode).size() != 20)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Warning: SIP���������Ϣ[%s@%s:%d][%s:%d]����!", 
                sServerCode, sPoliceNetIP, nPoliceNetPort, sExteranlIP, nExteranlPort);
            continue;
        }
        if(string(sPoliceNetIP) == "" || nPoliceNetPort == 0 || string(sParentCode) == "" || string(sServerCode) == "")
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: SIP�������ݴ���[%s@%s:%d], Parent[%s].", 
                sServerCode, sPoliceNetIP, nPoliceNetPort, sParentCode);
            continue;
        }

        if(string(sServerCode) == m_sSIPServerID)
        {
            m_nServerID = nServerID;
        }

        SIPServerMap::iterator itServer = m_mapSIPServer.find(sServerCode);
        if(itServer != m_mapSIPServer.end())
        {
            itServer->second.nServerID = nServerID;
            itServer->second.sPoliceNetIP = sPoliceNetIP;
            itServer->second.nPoliceNetPort = nPoliceNetPort;
            itServer->second.sExteranlIP = sExteranlIP;
            itServer->second.nExteranlPort = nExteranlPort;
            itServer->second.sParentCode = sParentCode;
        }
        else
        {
            SIPServerInfo sipServerInfo;
            sipServerInfo.sSIPServerCode = sServerCode;
            sipServerInfo.nServerID = nServerID;
            sipServerInfo.sPoliceNetIP = sPoliceNetIP;
            sipServerInfo.nPoliceNetPort = nPoliceNetPort;
            sipServerInfo.sExteranlIP = sExteranlIP;
            sipServerInfo.nExteranlPort = nExteranlPort;
            sipServerInfo.sParentCode = sParentCode;

            m_mapSIPServer[sServerCode] = sipServerInfo;
        }

        itServer = m_mapSIPServer.find(sServerCode);
        if(itServer != m_mapSIPServer.end())
        {            
            if(itServer->second.sPoliceNetIP == itServer->second.sExteranlIP)  //�繫����IP==ר��IP, ˵���˷���û��ӳ�䵽������, ֻ�����ݿ�ǿ�Ʒǿ�, ���Ƶ�ר��IP
            {
                itServer->second.sPoliceNetIP = "";
                itServer->second.nPoliceNetPort = 0;
            }
        }
        else
        {
            continue;
        }

        //���¼�ƽ̨��Ϣ���뵽�豸����б�, ���Լ��״̬        
        if(("200" == itServer->second.sSIPServerCode.substr(10, 3) || "208" == itServer->second.sSIPServerCode.substr(10, 3) 
            || "216" == itServer->second.sSIPServerCode.substr(10, 3)) && itServer->second.sParentCode == m_sSIPServerID)
        {
            char pAddr[512] = {0};
            if(IsSameNet(m_sSIPServerIP, itServer->second.sExteranlIP))
            {
                sprintf_s(pAddr, sizeof(pAddr), "sip:%s@%s:%d", 
                    sServerCode, itServer->second.sExteranlIP.c_str(), itServer->second.nExteranlPort);
            }
            else if(IsSameNet(m_sSIPServerIP, itServer->second.sPoliceNetIP))
            {
                sprintf_s(pAddr, sizeof(pAddr), "sip:%s@%s:%d", 
                    sServerCode, itServer->second.sPoliceNetIP.c_str(), itServer->second.nPoliceNetPort);
            }
            else 
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "Error: �¼�ƽ̨[%s]��ƥ��IP, �޷�ͨ��.", itServer->second.sSIPServerCode.c_str());
                continue;
            }

            DVRINFOMAP::iterator itDev = m_mapDevice.find(sServerCode);
            if(itDev == m_mapDevice.end())
            {
                LPDVRINFO pRegInfo = new DVRINFO;
                strcpy_s(pRegInfo->pDeviceUri, sizeof(pRegInfo->pDeviceUri), pAddr);
                pRegInfo->bReg = true;
                pRegInfo->tRegistTime = time(&pRegInfo->tRegistTime);
                m_mapDevice.insert(make_pair(sServerCode, pRegInfo));
            }           
        }
            
    }
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "����SIPSERVERIFNO���ȡSIP���������Ϣ���");
    //printf("-------------------------------------------------------------------------------\n");

    if(m_nServerID == 0)
    {
        if(!GetLocalServerID(m_sSIPServerID.c_str()))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ȡ���ط���[%s]ServerInfo��IDʧ��!", m_sSIPServerID.c_str());
            return false;
        }
    }
    return true;
}
bool CSIPServer::GetRTPStreamInfo()
{
    time_t tCurrent = time(&tCurrent);
    char szSql[1024] = {0};
    sprintf_s(szSql, sizeof(szSql), "select sipServerCode, ip, port from xy_base.sipServerInfo where ParentCode = '%s' order by SIPServerCode",
                                    m_sSIPServerID.c_str());
	otl_stream otlSelect;
	bool nExitSQL = m_DBMgr.ExecuteSQL(szSql, otlSelect);
	if(false == nExitSQL)
		return false;

    while(!otlSelect.eof())
    {
		char sServerCode[MAX_STRING_LEN] = {0};
		char sStreamIP[MAX_STRING_LEN] = {0};
		int  nStreamPort = 0;
		string strServerCode;
		string strStreamPort;

		otlSelect >> sServerCode >> sStreamIP >> nStreamPort;      

		strServerCode = sServerCode;
		sprintf_s(sServerCode, MAX_STRING_LEN, "%d", nStreamPort);
		strStreamPort = sServerCode;

        if(strServerCode.size() != 20)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: RTP��ý�������Ϣ[%s@%s:%d]����!", 
                strServerCode.c_str(), sStreamIP, nStreamPort);
            continue;
        }
        else if("200" == strServerCode.substr(10, 3))
        {
            
        }
        else if("202" == strServerCode.substr(10, 3) || "224" == strServerCode.substr(10, 3))
        {
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "��ǰSIP�������RTP��ý��[%s@%s:%s].",
                strServerCode.c_str(), sStreamIP, strStreamPort.c_str());
            string sRTPUri = "sip:" + strServerCode + "@" + sStreamIP + ":" + strStreamPort; 

            LPRTP_INFO pRTPInfo = new RTP_INFO;
            pRTPInfo->sRTPUri = sRTPUri;
            pRTPInfo->sRTPID = strServerCode;
            pRTPInfo->sRTPIP = sStreamIP;
            pRTPInfo->nRTPPort = nStreamPort;
            pRTPInfo->nTotal = 0;
            pRTPInfo->nType = "202" == strServerCode.substr(10, 3) ? REALMEDIASERVERTYPE : FILEMEDIASERVERTYPE;
            pRTPInfo->tUpdateTime = tCurrent;

            m_pRTPManager->InsertRTPInfo(m_sSIPServerID, pRTPInfo);

            continue;           
        }
    }
    if(m_pRTPManager->m_listRTPInfo.size() == 0)
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "Error : ��ǰSIP[%s]����û�з���RTP��ý��.", m_sSIPServerID.c_str());
        return false;
    }

    m_pRTPManager->EraseRTPInfo(tCurrent);  //����ԭ������SIP����, �������ڵ�RTP����.
    return true;
}
bool CSIPServer::SetAssignRTPServer(string sRTPServerURI)
{
    char *result = strtok((char *)sRTPServerURI.c_str(), ";");
    while (result)
    {
        LPRTP_INFO pRTPInfo = new RTP_INFO;
        pRTPInfo->sRTPUri = sRTPServerURI;
        pRTPInfo->sRTPID = xsip_get_url_username(result);
        pRTPInfo->nRTPPort = xsip_get_url_port(result);
        pRTPInfo->sRTPIP = xsip_get_url_host(result);
        pRTPInfo->nTotal = 0;
        pRTPInfo->nType = "202" == pRTPInfo->sRTPID.substr(10, 3) ? REALMEDIASERVERTYPE : FILEMEDIASERVERTYPE;
        m_pRTPManager->InsertRTPInfo(m_sSIPServerID, pRTPInfo);
        result = strtok(NULL, ";");
    }


    return true;
}
bool CSIPServer::GetDeviceInfo(bool bUpdate)
{
    char szSql[4096] = {0};
    char pAddr[512] = {0}; 

    //��ȡ��ͷ��Ϣ
    if (bUpdate)
    {
        sprintf_s(szSql, sizeof(szSql), 
            "select a.deviceid, c.deviceid, d.id, b.nettype, b.transcoding, c.name, e.DVRIP, "
            "a.port, e.dvrport, d.chanindex, d.Channo, c.status, e.dvruser, e.dvrpasswd, f.dvrtype, f.dvrkey, "
            "b.serviceconfigid, g.descrip, to_char(d.UpdateTime,'yyyy-mm-dd hh24:mi:ss') "
            "from %s c, %s a, xy_base.sipdvrtoserver b, xy_base.channelinfo d, xy_base.dvrinfo e, xy_base.dvrtype f, xy_base.serverinfo g "
            "where a.id = b.sipdvrid and c.parentid = a.deviceid and g.id = b.serviceconfigid "
            "and c.id = d.id and d.dvrid = e.id and f.dvrtype = e.dvrtype and c.deviceid not like '440307%%' "
            "and d.updateTime > TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss') "
            "order by a.deviceid, a.updatetime", 
            m_sSIPChannelTable.c_str(), m_sSIPDVRTable.c_str(), m_sUpdateTime.c_str());
    } 
    else
    {
        sprintf_s(szSql, sizeof(szSql), 
            "select a.deviceid, c.deviceid,  d.id, b.nettype, b.transcoding, c.name, e.DVRIP, "
            "a.port, e.dvrport, d.chanindex, d.Channo, c.status, e.dvruser, e.dvrpasswd, f.dvrtype, f.dvrkey, "
            "b.serviceconfigid, g.descrip, to_char(d.UpdateTime,'yyyy-mm-dd hh24:mi:ss') "
            "from %s c, %s a, xy_base.sipdvrtoserver b, xy_base.channelinfo d, xy_base.dvrinfo e, xy_base.dvrtype f, xy_base.serverinfo g "
            "where a.id = b.sipdvrid and c.parentid = a.deviceid and g.id = b.serviceconfigid "
            "and c.id = d.id and d.dvrid = e.id and f.dvrtype = e.dvrtype and c.deviceid not like '440307%%' "
            "order by a.deviceid, a.updatetime", 
            m_sSIPChannelTable.c_str(), m_sSIPDVRTable.c_str());
    }
    

    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    char sDeviceID[MAX_STRING_LEN] = {0};
    char sCameraID[MAX_STRING_LEN] = {0};
    char sUpDateTime[MAX_STRING_LEN] = {0};
    char pStatus[10] = {0};
    EnterCriticalSection(&m_vLock);
    int nNum = 0;
    while(!otlSelect.eof())
    {                   
        otlSelect >> sDeviceID >> sCameraID; //sDVRIP >> nDVRPort >> sUpDateTime ;   
        LPDVRINFO pDVRInfo = NULL;
        DVRINFOMAP::iterator itDev = m_mapDevice.find(sDeviceID);
        if(itDev != m_mapDevice.end())
        {
            pDVRInfo = itDev->second;
        }
        else
        {
            pDVRInfo = new DVRINFO;
            m_mapDevice.insert(make_pair(sDeviceID, pDVRInfo));
            strcpy_s(pDVRInfo->pDeviceCode, sizeof(pDVRInfo->pDeviceCode), sDeviceID);
        }

        LPCHANNELINFO pChannelInfo = NULL;
        CHANNELINFOMAP::iterator itChannel = pDVRInfo->m_mapChannelInfo.find(sCameraID);
        if(itChannel != pDVRInfo->m_mapChannelInfo.end())
        {
            pChannelInfo = itChannel->second;
        }
        else
        {
            pChannelInfo = new CHANNELINFO;
            pDVRInfo->m_mapChannelInfo.insert(make_pair(sCameraID, pChannelInfo));
            strcpy_s(pChannelInfo->pSIPCameraCode , sizeof(pChannelInfo->pSIPCameraCode ), sCameraID);
        }

        otlSelect >> pChannelInfo->nChanID >> pDVRInfo->nNetType >> pDVRInfo->nTranscoding >> pChannelInfo->pName >> pDVRInfo->pDVRIP >> 
                    pDVRInfo->nDVRSIPPort >> pDVRInfo->nDVRSDKPort >>pChannelInfo->nChannel >> pChannelInfo->pChanno >> pStatus >>
                    pDVRInfo->pUserName >> pDVRInfo->pPassword >> pDVRInfo->nDVRType >> pDVRInfo->pDVRKey >>
                    pDVRInfo->nSIPServerID >> pDVRInfo->pSIPServerCode >> sUpDateTime;
        if(pDVRInfo->nDVRType == 8)
        {
            strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
        }
        if(string(pStatus) == "ON" || string(pStatus) == "on")
        {
            pChannelInfo->bOnline = true;
        }
        else
        {
            pChannelInfo->bOnline = false;
        }

        if(pDVRInfo->nNetType == 4)
        {
            sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", sDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSDKPort);
        }
        else
        {
            sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", sDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSIPPort);
        }
        pDVRInfo->bReg = true;
        if(m_pam.m_nSearchDevice && (IsDevice(sDeviceID) || IsCamera(sDeviceID))) pDVRInfo->bSendCatalog = true;
        if(m_pam.m_nSearchPlat && IsPlatform(sDeviceID)) pDVRInfo->bSendCatalog = true;
        pDVRInfo->tRegistTime = time(&pDVRInfo->tRegistTime);

        m_sUpdateTime = string(sUpDateTime) > m_sUpdateTime ? sUpDateTime : m_sUpdateTime;
        nNum ++;
        if(nNum % 5000 == 0)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "����ȡ��ͷĿ¼��[%d]...", nNum);
        }
    }
    otlSelect.close();
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "����ȡ��ͷĿ¼����[%d]...", nNum);
    LeaveCriticalSection(&m_vLock);

    //��ȡ�豸��Ϣ(��Щ�Խ��豸��ƽ̨��ʱ���޾�ͷ��Ϣ, �����ڶ�ȡ��, ���豸��ƽ̨ע������䷢��Ŀ¼������Ϣ
    sprintf_s(szSql, sizeof(szSql), 
        "select a.deviceid, b.nettype, b.transcoding, e.DVRIP, "
        "a.port, e.dvrport, e.dvruser, e.dvrpasswd, f.dvrtype, f.dvrkey, "
        "b.serviceconfigid, g.descrip "
        "from %s a, xy_base.sipdvrtoserver b, xy_base.dvrinfo e, xy_base.dvrtype f, xy_base.serverinfo g "
        "where a.id = b.sipdvrid and a.id = e.id and g.id = b.serviceconfigid "
        "and f.dvrtype = e.dvrtype "
        "order by a.deviceid, a.updatetime", 
        m_sSIPDVRTable.c_str());

    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    EnterCriticalSection(&m_vLock);
    while(!otlSelect.eof())
    {                   
        otlSelect >> sDeviceID;
        LPDVRINFO pDVRInfo = NULL;
        DVRINFOMAP::iterator itDev = m_mapDevice.find(sDeviceID);
        if(itDev != m_mapDevice.end())
        {
            pDVRInfo = itDev->second;
        }
        else
        {
            pDVRInfo = new DVRINFO;
            m_mapDevice.insert(make_pair(sDeviceID, pDVRInfo));
            strcpy_s(pDVRInfo->pDeviceCode, sizeof(pDVRInfo->pDeviceCode), sDeviceID);
        }

        otlSelect >> pDVRInfo->nNetType >> pDVRInfo->nTranscoding >> pDVRInfo->pDVRIP >> 
                     pDVRInfo->nDVRSIPPort >> pDVRInfo->nDVRSDKPort >> pDVRInfo->pUserName >> 
                     pDVRInfo->pPassword >> pDVRInfo->nDVRType >> pDVRInfo->pDVRKey >>
                     pDVRInfo->nSIPServerID >> pDVRInfo->pSIPServerCode;

        if(itDev != m_mapDevice.end())      //���豸�����漺����ȡ��, �о�ͷ��Ϣ
        {
            if(pDVRInfo->nDVRType == 8)
            {
                strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
            }
        }
        else                                //���豸��δ��ȡ, �޾�ͷ��Ϣ
        {
            if(pDVRInfo->nDVRType == 8)
            {
                strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
            }
            if(pDVRInfo->nNetType == 4)
            {
                sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", sDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSDKPort);
            }
            else
            {
                sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", sDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSIPPort);
            }
            pDVRInfo->bReg = true;
            if(m_pam.m_nSearchDevice && (IsDevice(sDeviceID) || IsCamera(sDeviceID))) pDVRInfo->bSendCatalog = true;
            if(m_pam.m_nSearchPlat && IsPlatform(sDeviceID)) pDVRInfo->bSendCatalog = true;
            pDVRInfo->tRegistTime = time(&pDVRInfo->tRegistTime);
        }
    }
    otlSelect.close();
    LeaveCriticalSection(&m_vLock);

    if (m_pam.m_sLGDBServer != "")
    {
        GetLGDeviceInfo(bUpdate);

        GetLGPrivateNetworkDeviceInfo(bUpdate);
    }
    

    if(!bUpdate)
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ȡ�豸��Ϣ���!",
            m_sSIPServerID.c_str(), m_sSIPServerIP.c_str(), m_nSIPServerPort);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�豸����: %d.\r\n�豸������ʱ��[%s]", m_mapDevice.size(), m_sUpdateTime.c_str());
    }
    
    return true;
}
bool CSIPServer::GetLGDeviceInfo(bool bUpdate)
{
    if(!bUpdate)
    {
        printf("��ȡ�����ڲ�����豸��Ϣ...\n");
    }
    int nCount = 0;

    char szSql[2048] = {0};
    char pAddr[512] = {0}; 

    //��ȡ��ͷ��Ϣ
    sprintf_s(szSql, sizeof(szSql), 
            "select a.channo, b.id, a.channame, b.dvrtype, c.dvrkey, b.dvrip, b.dvrport, b.dvruser, b.dvrpasswd, a.chanindex "
            "from channelinfo a, dvrinfo b, dvrtype c "
            "where a.dvrid = b.id and a.chanstrindex = '10' and b.dvrtype = c.dvrtype "
            "order by dvrip");
    otl_stream otlSelect;
    if(!m_LGDBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    int nDVRID = 0;
    char pChanno[MAX_STRING_LEN] = {0};
    char pDeviceID[MAX_STRING_LEN] = {0};
    string sCameraID = "";

    char pName[MAX_STRING_LEN] = {0};
    int nDVRType = 0;
    char pDVRKey[12] = {0};
    char pIP[MAX_STRING_LEN] = {0};
    int nDVRPort = 0;
    char pUser[MAX_STRING_LEN] = {0};
    char pPassword[MAX_STRING_LEN] = {0};
    int nChanIndex = 0;
    EnterCriticalSection(&m_vLock);
    int nNum = 0;
    while(!otlSelect.eof())
    {                   
        otlSelect >> pChanno >> nDVRID; 
        if(strlen(pChanno) == 12)
        {
            nCount ++;

            sprintf_s(pDeviceID, sizeof(pDeviceID), "4403070000111%07d", nDVRID);
            sCameraID = string(pChanno, 0, 8) + "001310" + string(pChanno, 6, 6);

            LPDVRINFO pDVRInfo = NULL;
            DVRINFOMAP::iterator itDev = m_mapDevice.find(pDeviceID);
            if(itDev != m_mapDevice.end())
            {
                pDVRInfo = itDev->second;
            }
            else
            {
                pDVRInfo = new DVRINFO;
                m_mapDevice.insert(make_pair(pDeviceID, pDVRInfo));
                strcpy_s(pDVRInfo->pDeviceCode, sizeof(pDVRInfo->pDeviceCode), pDeviceID);
            }

            LPCHANNELINFO pChannelInfo = NULL;
            CHANNELINFOMAP::iterator itChannel = pDVRInfo->m_mapChannelInfo.find(sCameraID);
            if(itChannel != pDVRInfo->m_mapChannelInfo.end())
            {
                pChannelInfo = itChannel->second;
            }
            else
            {
                pChannelInfo = new CHANNELINFO;
                pDVRInfo->m_mapChannelInfo.insert(make_pair(sCameraID, pChannelInfo));
                strcpy_s(pChannelInfo->pSIPCameraCode , sizeof(pChannelInfo->pSIPCameraCode ), sCameraID.c_str());
            }

            pDVRInfo->nNetType = 4;
            pDVRInfo->nTranscoding = 1;
            strcpy_s(pChannelInfo->pChanno, sizeof(pChannelInfo->pChanno), pChanno);
            otlSelect >> pChannelInfo->pName >> pDVRInfo->nDVRType >> pDVRInfo->pDVRKey >> pDVRInfo->pDVRIP >> 
                pDVRInfo->nDVRSDKPort >> pDVRInfo->pUserName >> pDVRInfo->pPassword >>pChannelInfo->nChannel;

            pDVRInfo->nSIPServerID = 1100;
            strcpy(pDVRInfo->pSIPServerCode, m_sSIPServerID.c_str());

            if(pDVRInfo->nDVRType == 8)
            {
                strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
            }
            pChannelInfo->bOnline = true;
            sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", pDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSDKPort);
            pDVRInfo->bReg = true;
        }
        else
        {
            otlSelect >> pName >> nDVRType >> pDVRKey >> pIP >> nDVRPort >> pUser >> pPassword >> nChanIndex;
        }

    }
    otlSelect.close();
    if(!bUpdate)
    {
        printf("��ȡ�����ڲ������Ϣ���, ��ͷ��[%d].\n", nCount);
    }
    LeaveCriticalSection(&m_vLock);
    return true;
}
bool CSIPServer::GetLGPrivateNetworkDeviceInfo(bool bUpdate)
{
    if(!bUpdate)
    {
        printf("��ȡ����ר����ͷ��Ϣ...\n");
    }
    int nCount = 0;

    char szSql[2048] = {0};
    char pAddr[512] = {0}; 

    //��ȡ��ͷ��Ϣ
    /*sprintf_s(szSql, sizeof(szSql), 
        "select a.chandesc, b.id, a.id, a.channame, b.dvrtype, c.dvrkey, b.dvrip, b.dvrport, b.dvruser, b.dvrpasswd, a.chanindex "
        "from channelinfo a, dvrinfo b, dvrtype c "
        "where a.dvrid = b.id and c.dvrtype in (47, 49) and a.chanstrindex != '10' and b.dvrtype = c.dvrtype and length(a.chandesc) = 12 "
        "order by dvrip");*/
    sprintf_s(szSql, sizeof(szSql), 
        "select d.channo, b.id, a.id, a.channame, b.dvrtype, c.dvrkey, "
        "b.dvrip, b.dvrport, b.dvruser, b.dvrpasswd, a.chanindex "
        "from channelinfo a , dvrinfo b, dvrtype c, xy_user01.channelinfoforhw d "
        "where  (a.chanstrindex = 1 or a.chanstrindex = 2 or a.chanstrindex = 3) and a.codetype = 49 "
        "and a.dvrid = b.id and b.dvrtype = c.dvrtype and a.chandesc = d.dvrip "
        "union all "
        "select a.channo, b.id, a.id, a.channame, b.dvrtype, c.dvrkey,  "
        "b.dvrip, b.dvrport, b.dvruser, b.dvrpasswd, a.chanindex "
        "from channelinfo a , dvrinfo b, dvrtype c "
        "where  (a.chanstrindex = 11 or a.chanstrindex = 12 or a.chanstrindex = 13) "
        "and a.dvrid = b.id and b.dvrtype = c.dvrtype");
    otl_stream otlSelect;
    if(!m_LGDBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    int nDVRID = 0;
    char pChanno[MAX_STRING_LEN] = {0};
    char pDeviceID[MAX_STRING_LEN] = {0};
    string sCameraID = "";

    int nChanID  = 0;
    char pName[MAX_STRING_LEN] = {0};
    int nDVRType = 0;
    char pDVRKey[12] = {0};
    char pIP[MAX_STRING_LEN] = {0};
    int nDVRPort = 0;
    char pUser[MAX_STRING_LEN] = {0};
    char pPassword[MAX_STRING_LEN] = {0};
    int nChanIndex = 0;
    EnterCriticalSection(&m_vLock);
    int nNum = 0;
    while(!otlSelect.eof())
    {                   
        otlSelect >> pChanno >> nDVRID; 
        if(strlen(pChanno) == 12)
        {
            nCount ++;

            sprintf_s(pDeviceID, sizeof(pDeviceID), "4403070000111%07d", nDVRID);
            sCameraID = string(pChanno, 0, 8) + "001310" + string(pChanno, 6, 6);

            LPDVRINFO pDVRInfo = NULL;
            DVRINFOMAP::iterator itDev = m_mapDevice.find(pDeviceID);
            if(itDev != m_mapDevice.end())
            {
                pDVRInfo = itDev->second;
            }
            else
            {
                pDVRInfo = new DVRINFO;
                m_mapDevice.insert(make_pair(pDeviceID, pDVRInfo));
                strcpy_s(pDVRInfo->pDeviceCode, sizeof(pDVRInfo->pDeviceCode), pDeviceID);
            }

            LPCHANNELINFO pChannelInfo = NULL;
            CHANNELINFOMAP::iterator itChannel = pDVRInfo->m_mapChannelInfo.find(sCameraID);
            if(itChannel != pDVRInfo->m_mapChannelInfo.end())
            {
                pChannelInfo = itChannel->second;
            }
            else
            {
                pChannelInfo = new CHANNELINFO;
                pDVRInfo->m_mapChannelInfo.insert(make_pair(sCameraID, pChannelInfo));
                strcpy_s(pChannelInfo->pSIPCameraCode , sizeof(pChannelInfo->pSIPCameraCode ), sCameraID.c_str());
            }

            pDVRInfo->nNetType = 7;
            pDVRInfo->nTranscoding = 1;
            strcpy_s(pChannelInfo->pChanno, sizeof(pChannelInfo->pChanno), pChanno);
            otlSelect >> pChannelInfo->nChanID >> pChannelInfo->pName >> pDVRInfo->nDVRType >> pDVRInfo->pDVRKey >> pDVRInfo->pDVRIP >> 
                pDVRInfo->nDVRSDKPort >> pDVRInfo->pUserName >> pDVRInfo->pPassword >>pChannelInfo->nChannel;

            if(pDVRInfo->nDVRType != 1)
            {
                pDVRInfo->nDVRType = 2;
                strcpy(pDVRInfo->pDVRKey, "DH");
            }
           
            pDVRInfo->nSIPServerID = 1100;
            strcpy(pDVRInfo->pSIPServerCode, m_sSIPServerID.c_str());

            if(pDVRInfo->nDVRType == 8)
            {
                strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
            }
            pChannelInfo->bOnline = true;
            sprintf_s(pDVRInfo->pDeviceUri, sizeof(pDVRInfo->pDeviceUri), "sip:%s@%s:%d", pDeviceID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSDKPort);
            pDVRInfo->bReg = true;
        }
        else
        {
            otlSelect >> nChanID >> pName >> nDVRType >> pDVRKey >> pIP >> nDVRPort >> pUser >> pPassword >> nChanIndex;
        }

    }
    otlSelect.close();
    if(!bUpdate)
    {
        printf("��ȡ����ר����ͷ��Ϣ���, ��ͷ��[%d].\n", nCount);
    }
    LeaveCriticalSection(&m_vLock);

    return true;
}
bool CSIPServer::UpdateDeviceToSIPServer()
{
    char szSql[1024] = {0};
    otl_stream otlSelect;
    sprintf_s(szSql, sizeof(szSql), 
        "select c.deviceid, a.serviceconfigid, b.descrip "
        "from xy_base.sipdvrtoserver a, xy_base.serverinfo b, xy_base.sipdvr c "
        "where a.serviceconfigid = b.id and c.id = a.sipdvrid");
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    int nSIPServerID = 0;
    char pDVRSIPCode[32] = {0};
    char pSIPServerCode[32] = {0};
    DVRINFOMAP::iterator itDVR = m_mapDevice.begin();
    while(!otlSelect.eof())
    {
        otlSelect >> pDVRSIPCode;
        itDVR = m_mapDevice.find(pDVRSIPCode);
        if(m_mapDevice.end() != itDVR)
        {
            otlSelect >> itDVR->second->nSIPServerID >> itDVR->second->pSIPServerCode;
        }
        else
        {
            otlSelect >> nSIPServerID >> pDVRSIPCode;
        }
    }
    otlSelect.close();

    return true;
}
bool CSIPServer::GetLocalServerID(string sServerCode)
{
    char szSql[1024] = {0};
    sprintf_s(szSql, sizeof(szSql), "select * from xy_base.serverinfo where descrip = '%s'", m_sSIPServerID.c_str());
    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    if(!otlSelect.eof())
    {
        otlSelect >> m_nServerID;
    }
    else
    {
        return false;
    }
    return true;
}
char *_atou(const char *cstr)
{
	int wclen = MultiByteToWideChar(CP_ACP, NULL, cstr, (int)strlen(cstr), NULL, 0);
	wchar_t *wcstr = new wchar_t[wclen+1];
	MultiByteToWideChar(CP_ACP, NULL, cstr, (int)strlen(cstr), wcstr, wclen);
	wcstr[wclen] = '\0';

	int u8len = WideCharToMultiByte(CP_UTF8, NULL, wcstr, (int)wcslen(wcstr), NULL, 0, NULL, NULL);
	char *utfstr = new char[u8len+1];
	WideCharToMultiByte(CP_UTF8, NULL, wcstr, (int)wcslen(wcstr), utfstr, u8len, NULL, NULL);
	utfstr[u8len] = '\0';
	delete[] wcstr;
	return utfstr;
}

char *_utoa(const char *ustr)
{
	int wclen = MultiByteToWideChar(CP_UTF8, NULL, ustr, (int)strlen(ustr), NULL, 0);
	wchar_t *wcstr = new wchar_t[wclen+1];
	MultiByteToWideChar(CP_UTF8, NULL, ustr, (int)strlen(ustr), wcstr, wclen);
	wcstr[wclen] = '\0';

	int clen = WideCharToMultiByte(CP_ACP, NULL, wcstr, wclen, NULL, 0, NULL, NULL);
	char *cstr = new char[clen+1];
	WideCharToMultiByte(CP_ACP, NULL, wcstr, wclen, cstr, clen, NULL, NULL);
	cstr[clen] = '\0';
	delete[] wcstr;
	return cstr;
}


string CSIPServer::GetProxy(string sAddr)
{
	string::size_type nBegin = 0;
	string::size_type nEnd = 0;
	nBegin = sAddr.find(':') + 1;
	if (nBegin != 0)
	{
		nEnd = sAddr.find('@') + 1;
		return sAddr.erase(nBegin, nEnd - nBegin);
	}
	return "";
}

int CSIPServer::ServerQuertInfoRecv(SOCKADDR_IN &addr, char *msg)
{
    char pTime[32] = {0};
    sprintf_s(pTime, sizeof(pTime), 
        "%04d-%02d-%02d %02d:%02d:%02d", 
        m_StartUpTime.wYear, m_StartUpTime.wMonth, m_StartUpTime.wDay, 
        m_StartUpTime.wHour, m_StartUpTime.wMinute, m_StartUpTime.wSecond);


    EnterCriticalSection(&m_vLock);
    char pXML[1024] = {0};
    sprintf_s(pXML, sizeof(pXML), 
        "<?xml version=\"1.0\"?>\r\n"
        "<Response>\r\n"
        "<CmdType>Info</CmdType>\r\n"
        "<SN>10001</SN>\r\n"
        "<DeviceID>%s</DeviceID>\r\n"
        "<StartUpTime>%s</StartUpTime>\r\n"
        "<RegDevice>%d</RegDevice>\r\n"         //�������豸|ƽ̨����
        "<RTPNum>%d</RTPNum>\r\n"               //RTP��ý������
        "<GetStream>%d</GetStream>\r\n"           //��ǰת����Ƶ������
        "</Response>\r\n",
        m_sSIPServerID.c_str(), pTime, m_mapDevice.size(), 
        m_pRTPManager->m_listRTPInfo.size(), m_pRTPManager->GetStreamNum());
    LeaveCriticalSection(&m_vLock);

    char szAnwser[MAX_CONTENT_LEN] = {0};
    xsip_call_build_answer(msg, 200, szAnwser);
    xsip_set_content_type(szAnwser, "Application/MANSCDP+xml");
    xsip_set_body(szAnwser, pXML, strlen(pXML));
    DWORD dwLen = (DWORD)strlen(szAnwser);
    szAnwser[dwLen] = '\0';

    m_pSIPProtocol->sendBuf(addr, szAnwser, dwLen);

    return 0;
}

string CSIPServer::GetRegisterAddr(SOCKADDR_IN &addr)
{
    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
	for (; itSup != m_mapSuperior.end(); itSup ++)
	{
		SOCKADDR_IN sour = {0};
		SetDestAddr(itSup->second->pDeviceUri, &sour);
		if (addr.sin_addr.S_un.S_addr == sour.sin_addr.S_un.S_addr)
		{
			return itSup->second->pDeviceUri;
		}
	}
	return "";
}

void CSIPServer::SetDestAddr(string sAddr, SOCKADDR_IN *addr)
{
	string sHost = xsip_get_url_host((char *)sAddr.c_str());
	int nPort = xsip_get_url_port((char *)sAddr.c_str());
	addr->sin_family = AF_INET;
	addr->sin_addr.S_un.S_addr = inet_addr(sHost.c_str());
	addr->sin_port = htons(nPort);
}


bool CSIPServer::IsMatrix(string sMatrix)
{
	string sMonitor = xsip_get_url_username((char *)sMatrix.c_str());
	string sType = sMonitor.substr(10, 3);
	if (sType == "133" || sType == "114")
	{
		return true;
	}
	return false;
}

bool CSIPServer::FindSessionMap(string sID)
{
	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sID);
	if (iter != m_SessionMap.end())
	{
		LeaveCriticalSection(&m_sLock);
		return true;
	}
	LeaveCriticalSection(&m_sLock);
	return false;
}

bool CSIPServer::EraseSessionMap(string sID)
{
	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sID);
	try
	{
		if (iter != m_SessionMap.end())
		{
			iter = m_SessionMap.erase(iter);
		}
	}
	catch (...)
	{
		LeaveCriticalSection(&m_sLock);
		return false;
	}
	LeaveCriticalSection(&m_sLock);
	return true;
}
int CSIPServer::DeviceRegAnswer(SOCKADDR_IN &addr, char *msg)
{
    char answer[DATA_BUFSIZE] = {0};
    DWORD len = 0;
    int expires = xsip_get_expires(msg);

    string sDevUri = xsip_get_via_addr(msg);
    string sDevID = xsip_get_url_username((char*)sDevUri.c_str());

    //����Ҫ��֤
    if (!m_bAuth)										
    {
        xsip_message_build_answer(msg, 200, answer, xsip_get_tag());
        string sTime = GetNowTime(3);
        xsip_set_date(answer, sTime.c_str());
        xsip_set_expires(answer, expires);

        m_pSIPProtocol->sendBuf(addr, answer, (DWORD)strlen(answer));
        if (expires != 0)								//ע��ɹ��������¼������ַ
        {            
            EnterCriticalSection(&m_vLock);
            DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
            if (itDev != m_mapDevice.end())
            {
                itDev->second->bReg = true;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�|�豸[%s]ע��ɹ�.", sDevID.c_str());
                LPDVRINFO pReginfo = new DVRINFO;
                strcpy_s(pReginfo->pDeviceUri, sizeof(pReginfo->pDeviceUri), sDevID.c_str());
                pReginfo->bReg = true;
                m_mapDevice.insert(make_pair(sDevID, pReginfo));
            }
            LeaveCriticalSection(&m_vLock);
        }
        else											//ע���ɹ���ɾ���¼������ַ
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�|�豸[%s]ע���ɹ�.", sDevID.c_str());
            EnterCriticalSection(&m_vLock);
            DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
            if (itDev != m_mapDevice.end())
            {
                itDev->second->bReg = false;
            }
            LeaveCriticalSection(&m_vLock);
        }
        return 0;
    }													//��Ҫ��֤����ȡ��֤����

    string sUsername = xsip_get_authorization_username(msg);
    string sRealm = xsip_get_authorization_realm(msg);
    string sNonce = xsip_get_authorization_nonce(msg);
    string sResponse = xsip_get_authorization_response(msg);
    string sUri = xsip_get_authorization_uri(msg);
    if (sUsername != "" && sNonce != "" && sResponse != "" && sUri != "")
    {
        bool bRet = xsip_check_auth_response(sResponse, sUsername, sRealm, m_pam.m_sLocalPassword, sNonce, sUri);
        if (bRet)
        {
            xsip_message_build_answer(msg, 200, answer, xsip_get_tag());
            string sTime = GetNowTime(3);
            string sContact = xsip_get_contact_url(msg);
            xsip_set_contact(answer, sContact.c_str());
            xsip_set_date(answer, sTime.c_str());
            xsip_set_expires(answer, expires);
            len = (DWORD)strlen(answer);
            m_pSIPProtocol->sendBuf(addr, answer, len);
            if (expires != 0)							//ע��ɹ������¼������ַ
            {
                EnterCriticalSection(&m_vLock);
                DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
                if (itDev != m_mapDevice.end())
                {                      
                    if (IsDevice(sDevID) || IsCamera(sDevID))
                    {
                        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "Reg: �豸[%s]ע��ɹ�.", sDevUri.c_str());
                        //�����Ҫ��ѯ�豸Ŀ¼(�����������һ��), �����豸�ɲ�����תΪ����״̬�������ļ���Ҫ��ѯ�豸Ŀ¼ʱ
                        if(itDev->second->bSendCatalog || (m_pam.m_nSearchDevice && !itDev->second->bReg))  
                        {
                            itDev->second->bSendCatalog = false;
                            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "���豸[%s]��ѯĿ¼...", sDevUri.c_str());
                            SendCatalogMsg(addr, sDevUri);
                        }
                        if (m_bSub)
                        {
                            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "���豸[%s]����Ŀ¼������Ϣ...", sDevUri.c_str());
                            SendSubscribeMsg(addr, msg);										//����Ŀ¼
                        }
                    }
                    else if (IsPlatform(sDevID))
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Reg: �¼�ƽ̨[%s]ע��ɹ�.", sDevUri.c_str());
                        //�����Ҫ��ѯƽ̨Ŀ¼(�����������һ��), ����ƽ̨�ɲ�����תΪ����״̬�������ļ���Ҫ��ѯƽ̨Ŀ¼ʱ
                        if(itDev->second->bSendCatalog || (m_pam.m_nSearchPlat && !itDev->second->bReg))
                        {
                            itDev->second->bSendCatalog = false;
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¼�ƽ̨[%s]��ѯĿ¼...", sDevUri.c_str());
                            if(!CreateChanGroupInfo(sDevID))
                            {
                                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "��ChanGroupInfo�д�����ͷ��[%s]ʧ��.", sDevID.c_str());
                            }
                            SendCatalogMsg(addr, sDevUri);
                        }
                        if (m_bSub)
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¼�ƽ̨[%s]����Ŀ¼������Ϣ...", sDevUri.c_str());
                            SendSubscribeMsg(addr, msg);										//����Ŀ¼
                        }
                    }

                    itDev->second->bReg = true;
                    itDev->second->tRegistTime = time(&itDev->second->tRegistTime);//����ע��ʱ��
                }
                else
                {
                    if(IsUser(sDevID))
                    {
                        LPDVRINFO pRegInfo = new DVRINFO;
                        strcpy_s(pRegInfo->pDeviceUri, sizeof(pRegInfo->pDeviceUri), sDevUri.c_str());
                        pRegInfo->bReg = true;
                        pRegInfo->tRegistTime = time(&pRegInfo->tRegistTime);

                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Reg: �û�[%s]ע��ɹ�.", sDevUri.c_str());
                        m_mapDevice.insert(make_pair(sDevID, pRegInfo));   //ֻ���û��ż����б�, �����豸��ƽ̨��Ϣ������ʱ�����ݿ�ֱ�Ӽ���                       
                    }
                }
                LeaveCriticalSection(&m_vLock);				
            }
            else										//ע���ɹ�ɾ���¼���ַ
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�|�豸|�¼�ƽ̨[%s]ע���ɹ�.", sDevUri.c_str());
                EnterCriticalSection(&m_vLock);
                DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
                if (itDev != m_mapDevice.end())
                {
                    if (IsUser(sDevID))
                    {
                        itDev->second->bReg = false;
                        ClearUserStream((char*)sDevID.c_str());
                        //������û����ж��û��Ƿ�������̨��������������
                        EnterCriticalSection(&m_PTZLock);
                        LockMap::iterator itMap = m_LockedDeviceMap.begin();
                        while (itMap != m_LockedDeviceMap.end())
                        {
                            if (itMap->second.sUserID == sDevID)
                            {
                                itMap = m_LockedDeviceMap.erase(itMap);
                            }
                            else
                            {
                                itMap++;
                            }
                        }
                        LeaveCriticalSection(&m_PTZLock);
                    }
                }
                LeaveCriticalSection(&m_vLock);
            }
        }
        else
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>�û���֤��Ϣ����!<\n\n");
            xsip_message_build_answer(msg, 400, answer, xsip_get_tag());
            xsip_set_expires(answer, expires);
            len = (DWORD)strlen(answer);
            m_pSIPProtocol->sendBuf(addr, answer, len);
        }
    }													
    else        //û����֤�������� 401 
    {
        char szAuth[512] = {0};
        sprintf_s(szAuth, sizeof(szAuth), "Digest realm=\"%s\",nonce=\"%s\"", XSIP::m_sRealm.c_str(), xsip_get_nonce().c_str());
        xsip_message_build_answer(msg, 401, answer, xsip_get_tag());
        xsip_set_www_authenticate(answer, szAuth);
        len = (DWORD)strlen(answer);
        m_pSIPProtocol->sendBuf(addr, answer, len);	
    }	
    return 0;
}

bool CSIPServer::IsCamera(string strID)
{
	if (strID.size() < 20)
	{
		return false;
	}
	string strType = strID.substr(10, 3);
	int nType = atoi(strType.c_str());
	if (nType >= 130 && nType < 140)
	{
		return true;
	}
	return false;
}

bool CSIPServer::IsDevice(string strID)
{
	if (strID.size() < 20)
	{
		return false;
	}
	string strType = strID.substr(10, 3);
	int nType = atoi(strType.c_str());
	if ( (nType >= 110 && nType < 120))
	{
		return true;
	}
	return false;
}
bool CSIPServer::IsDeviceInCharge(string sDeviceID)
{
    char szSql[2048] = {0};
    char pAddr[512] = {0}; 

    sprintf_s(szSql, sizeof(szSql), "select b.serviceconfigID "
            "from %s a, xy_base.sipdvrtoserver b "
            "where a.id = b.sipdvrid and a.deviceid = '%s'", 
            m_sSIPDVRTable.c_str(), sDeviceID.c_str());

    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;
   
    int  nServerID = 0;
    while(!otlSelect.eof())
    {                   
        otlSelect >> nServerID;  
        break;
    }
    if(nServerID == m_nServerID)
    {
        return true;
    }

    return false;
}
bool CSIPServer::CreateChanGroupInfo(string sDeviceID, string sDeviceName, string sParentID)
{
    otl_stream otlSelect;
    char sSql[2048] = {0};

    //��ѯ�Ƿ񼺴��ڴ˾�ͷ��
    sprintf_s(sSql, sizeof(sSql), "select ParentID from xy_base.changroupInfo where CopyParentID = '%s'", sDeviceID.c_str());
    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯchangroupInfo���Ƿ���ھ�ͷ��[%s]����.", sDeviceID.c_str());
        return false;
    }
    if(!otlSelect.eof())     //��ͷ�鼺����
    {        
        if(sParentID != "")  //�縸ID��Ϊ��, ����¸������Ϣ
        {
            int nDeviceParentID = -1;
            otlSelect >> nDeviceParentID;
            otlSelect.close();

            sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯchangroupInfo��ͷ��[%s]��Ϣ����.", sParentID.c_str());
                return false;
            }
            if(!otlSelect.eof())
            {
                int nParentID = -1;
                otlSelect >> nParentID;
                otlSelect.close();
                if(nParentID == nDeviceParentID)     //��ͬ���ø���
                {
                    
                }
                else
                {
                    sprintf_s(sSql, sizeof(sSql), "Update xy_base.changroupInfo set ParentID = %d, NodeName = '%s' where CopyParentID = '%s'",
                                                    nParentID, sDeviceName.c_str(), sDeviceID.c_str());
                    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect, true))
                        return false;
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¾�ͷ��[%s][%s]��Ϣ, �����[%s][ID = %d].", 
                        sDeviceName.c_str(), sDeviceID.c_str(), sParentID.c_str(), nParentID);
                    otlSelect.close();
                }
            }
        }
        else
        {
            otlSelect.close();
            string sTemp = string(sDeviceID, 0, 4);
            if(sDeviceID.size() == 8 && sTemp == "4403")
            {
                int nPID = 19215;

                sprintf_s(sSql, sizeof(sSql), "Update xy_base.changroupInfo set ParentID = %d, NodeName = '%s' where CopyParentID = '%s'",
                    nPID, sDeviceName.c_str(), sDeviceID.c_str());
                if(!m_DBMgr.ExecuteSQL(sSql, otlSelect, true))
                    return false;
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���¾�ͷ��[%s][%s]��Ϣ, �����[%s][ID = %d].", 
                    sDeviceName.c_str(), sDeviceID.c_str(), sParentID.c_str(), nPID);
                otlSelect.close();
            }
            

        }
        return true;
    }
    otlSelect.close();

    //��ȡ��ͷ�鸸���ID
    int nParentID = -1;
    if(sParentID != "")
    {
        //sParentID��Ϊ����ֱ�Ӳ��Ҵ˽��ID
        sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯchangroupInfo��ͷ��[%s]��Ϣ����.", sParentID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> nParentID;
            otlSelect.close();
        }
        else
        {
            otlSelect.close();
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Warning: ����changroupInfo��[%s][%s]����ͷ��[%s]�޼�¼.", sDeviceName.c_str(), sDeviceID.c_str(), sParentID.c_str());
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���������[%s].", sParentID.c_str());

            sprintf_s(sSql, sizeof(sSql), "Insert into xy_base.changroupInfo"
                "(ID, OrganID, NodeName, ParentID, IsChild, CopyParentID) "
                "Values (xy_base.seq_changroupinfo_id.nextval, %d, '%s', %d, %d, '%s')",
                1, "", -1, 1, sParentID.c_str());

            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect, true))
            {
                return false;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��������ͷ��[%s]�ɹ�.", sParentID.c_str());
            }
            otlSelect.close();

            sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯchangroupInfo��ͷ��[%s]��Ϣ����.", sParentID.c_str());
                return false;
            }
            if(!otlSelect.eof())
            {
                otlSelect >> nParentID;
                otlSelect.close();
            }
            else
            {
                otlSelect.close();
                return false;
            }
        }
    }
    else
    {
        //sParentIDΪ������ݹ���20λ������������Ӧ�����
        char pParentCode[32] = "";
        ZeroMemory(sSql, sizeof(sSql));
        sprintf_s(sSql, sizeof(sSql), 
            "select ID, CopyParentID from xy_base.changroupInfo where CopyParentid != '-1'  order by id");
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯchangroupInfo��ID, CopyParentID����.");
            return false;
        }
        while(!otlSelect.eof())
        {
            otlSelect >> nParentID >> pParentCode;
            if(string(sDeviceID, 0, 6) == string(pParentCode, 0, 6))
            {
                break;
            }
            nParentID = -1;
        }
        otlSelect.close();
        if(nParentID == -1)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: ��changroupInfo��δ���ҵ�ƽ̨[%s]��Ӧ����ͷ��.", sDeviceID.c_str());
            return false;
        }

        /*if(sDeviceID.size() == 8 && string(sDeviceID, 0, 4) == "4403")
        {
            int nParentID = 19215;
        }*/
    }
    
    char pDeviceName[128] = {0};
    if(sDeviceName == "")
    {
        ZeroMemory(sSql, sizeof(sSql));
        sprintf_s(sSql, sizeof(sSql), "select name from xy_base.sipdvr where deviceid = '%s'", sDeviceID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯsipdvr��[%s] name����.", sDeviceID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> pDeviceName;
            sDeviceName = pDeviceName;
        }
        else
        {
            sDeviceName = sDeviceID;
        }
        otlSelect.close();
    }
    sprintf_s(sSql, sizeof(sSql), "Insert into xy_base.changroupInfo"
        "(ID, OrganID, NodeName, ParentID, IsChild, CopyParentID) "
        "Values (xy_base.seq_changroupinfo_id.nextval, %d, '%s', %d, %d, '%s')",
        1, sDeviceName.c_str(), nParentID, 1, sDeviceID.c_str());

    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect, true))
    {
        return false;
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "������ͷ��[%s][%s]�ɹ�.", sDeviceName.c_str(), sDeviceID.c_str());
    }
    otlSelect.close();

    return true;
}
bool CSIPServer::DelCameraInfo(string sDeviceID)
{
    otl_stream otlSelect;
    char szSql[2048] = {0};
    sprintf_s(szSql, sizeof(szSql), "delete %s where deviceid = '%s'", m_sSIPChannelTable.c_str(), sDeviceID.c_str());

    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect, true))
        return false;
    otlSelect.close();

    ZeroMemory(szSql, sizeof(szSql));
    sprintf_s(szSql, sizeof(szSql), "delete %s where nodeno = '%s'", m_sChanGroupToChanTable.c_str(), sDeviceID.c_str());

    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect, true))
        return false;
    otlSelect.close();

    /*int nChanID = 0;
    ZeroMemory(szSql, sizeof(szSql));
    sprintf_s(szSql, sizeof(szSql), "select id from %s where channo = '%s'", m_sChannelInfoTable.c_str(), sDeviceID.c_str());
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;
    if(!otlSelect.eof())
    {
        otlSelect >> nChanID;
    }
    otlSelect.close();*/

    ZeroMemory(szSql, sizeof(szSql));
    sprintf_s(szSql, sizeof(szSql), "delete %s where channo = '%s'",  m_sChannelInfoTable.c_str(), sDeviceID.c_str());

    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect, true))
    {
        otlSelect.close();    
        return false;
    }
    otlSelect.close();    

    /*SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    char pUpdateTime[32] = {0};
    sprintf_s(pUpdateTime, sizeof(pUpdateTime), "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear, sysTime.wMonth, sysTime.wDay, 
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
    ZeroMemory(szSql, sizeof(szSql));
    sprintf_s(szSql, sizeof(szSql), "Insert Into XY_MONITOR.DATACHANGE(ID, TableName, ChangeID, ChangeType, ChangeTime) "
                                "Values (XY_MONITOR.seq_datachange_id.nextval, 'ChannelInfo', %d, 1, TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'))",
                                nChanID, pUpdateTime);
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect, true))
        return false;
    otlSelect.close();    */

    return true;
}
bool CSIPServer::IsUser(string strID)
{
	//�����û�
	if (strID.size() < 20)
	{
		return false;
	}
	string strType = strID.substr(10, 3);
	int nType = atoi(strType.c_str());
	if (nType >= 300 && nType < 400)
	{
		return true;
	}
	return false;
}

bool CSIPServer::IsPlatform(string strID)
{
	//ƽ̨
	if (strID.size() < 20)
	{
		return false;
	}
	string strType = strID.substr(10 ,3);
	int nType = atoi(strType.c_str());
	if (nType >= 200 && nType < 300)
	{
		return true;
	}
	return false;
}
bool CSIPServer::IsChanGroup(string strID)
{
    //ƽ̨
    if (strID.size() < 20)
    {
        return false;
    }
    string strType = strID.substr(10 ,3);
    int nType = atoi(strType.c_str());
    if (nType >= 500 && nType < 600)
    {
        return true;
    }
    return false;
}

bool CSIPServer::IsMonitor(string strID)
{
	if (strID.size() < 20)
	{
		return false;
	}
	string strType = strID.substr(10, 3);
	int nType = atoi(strType.c_str());
	if (nType == 133)
	{
		return true;
	}
	return false;
}



bool CSIPServer::UpdateDeviceStatus(string sAddr, bool bStatus)
{
    if(!m_pam.m_nWriteDB)
    {
        return true;
    }

	string sIPAddress = xsip_get_url_host((char *)sAddr.c_str());
	int nPort = xsip_get_url_port((char *)sAddr.c_str());

	//��ȡ�豸״̬  
    string sStatus = bStatus ? "ON" : "OFF";

	char szSql[512] = {0};
	sprintf_s(szSql,sizeof(szSql),"update %s set Status = '%s' where IPAddress = '%s' and Port = %d", m_sSIPChannelTable.c_str(),sStatus.c_str(),sIPAddress.c_str(),nPort);
	otl_stream otlUpdate;
	return m_DBMgr.ExecuteSQL(szSql, otlUpdate, true);
}
bool CSIPServer::UpdatePlatformStatus(string sAddr, bool bStatus)
{
    if(!m_pam.m_nWriteDB)
    {
        return true;
    }
    string sPlatCode = xsip_get_url_username((char *)sAddr.c_str());

    //��ȡ�豸״̬
    string sStatus = bStatus ? "ON" : "OFF";

    char szExc[1024] = {0};
    char szSql[1024] = {0};
    bool bDevicePlat = false;   //True: �¼���˾ƽ̨, ���豸����; False:�¼���˾ƽ̨
    
    sprintf_s(szSql, sizeof(szSql), "select * from xy_base.sipserverinfo where sipservercode = '%s'", sPlatCode.c_str());
    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    if(!otlSelect.eof())
    {
        bDevicePlat = true;
    }
    otlSelect.close();

    if(bDevicePlat)
    {
        UpdateDeviceStatus(sAddr, bStatus);
    }
    else
    {
        sprintf_s(szSql, sizeof(szSql), 
            "select a.deviceid "
            "from xy_base.sipchannel a, xy_base.sipdvr b, xy_base.sipdvrtoserver c, xy_base.serverinfo d "
            "where d.descrip = '%s' and d.id = c.serviceconfigid and b.id = c.sipdvrid "
            " and a.serverid = b.deviceid", sPlatCode.c_str());
        otl_stream otlSelect;
        if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
            return false;

        while(!otlSelect.eof())
        {   
            char pChanno[128] = {0};
            otlSelect >> pChanno;

            sprintf_s(szExc,sizeof(szExc),"update xy_base.sipchannel set Status = '%s' where deviceid = '%s'", sStatus.c_str(), pChanno);
            otl_stream otlSelect2;
            if(!m_DBMgr.ExecuteSQL(szExc, otlSelect2))
                return false;  
            otlSelect2.close();
        }
        otlSelect.close();
    }

   
   
    return true;
}

string CSIPServer::GetChannelID(const char *sID, int no)
{
	string sChannelID = sID;
	sChannelID.replace(10, 3, "131");
	char szNo[21] = {0};
	sprintf_s(szNo, sizeof(szNo), "%s%06d", sChannelID.substr(0, 14).c_str(), no);
	return szNo;
}

string CSIPServer::GetDeviceAddr(const char *sID, INVITE_TYPE InviteType, LPCAMERAINFO pCameraInfo)
{
    if(m_pam.m_sAssignLowServer != "")     //�鿴�����ļ����Ƿ�ָ��ת���¼�SIP����
    {
        string sServerIP = xsip_get_url_host((char*)m_pam.m_sAssignLowServer.c_str());
        int nServerPort = xsip_get_url_port((char*)m_pam.m_sAssignLowServer.c_str());
        if(sServerIP == "" || nServerPort == 0)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "ָ���¼�SIP������Ϣ����[%s].", m_pam.m_sAssignLowServer.c_str());
            return "";
        }
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�ҵ�ָ���¼�SIP����[%s@%s:%d].", sID, sServerIP.c_str(), nServerPort);

        if(pCameraInfo != NULL)
        {
            pCameraInfo->nNetType = m_pam.m_nNetType;
        }
        return xsip_to_init(sID, sServerIP.c_str(), nServerPort);
    }


    LPDVRINFO pDVRInfo = NULL;
    LPCHANNELINFO pChannelInfo = NULL;
    DVRINFOMAP::iterator itDVR = m_mapDevice.begin();
    for(; itDVR != m_mapDevice.end(); itDVR ++)
    {
        CHANNELINFOMAP::iterator itChannel = itDVR->second->m_mapChannelInfo.find(sID);
        if(itChannel != itDVR->second->m_mapChannelInfo.end())
        {
            pDVRInfo = itDVR->second;
            pChannelInfo = itChannel->second;
            break;
        }
    }

	if(pDVRInfo != NULL && pChannelInfo != NULL)
	{
        int nDVRPort = (pDVRInfo->nNetType == 4 || pDVRInfo->nNetType == 7) ? pDVRInfo->nDVRSDKPort : pDVRInfo->nDVRSIPPort;
        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "��ͷ[%s@%s:%d]�ҵ�����sip����[%s].", 
            sID, pDVRInfo->pDVRIP, nDVRPort, pDVRInfo->pSIPServerCode);

        if(!pChannelInfo->bOnline && InviteType == XSIP_INVITE_PLAY)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ��ͷ[%s@%s:%d]������.", 
                sID, pDVRInfo->pDVRIP, nDVRPort);
            return "";
        }

        if(string(pDVRInfo->pSIPServerCode) == m_sSIPServerID)   //�ȽϹ���SIP�����Ƿ�Ϊ��ǰSIP����
		{
            if(pCameraInfo != NULL)
            {
                pCameraInfo->nNetType = pDVRInfo->nNetType;
            }

            if(XSIP_INVITE_PLAY != InviteType && m_pam.m_bFileStream)    //���ļ���ý��ʱ, ���Ҷ�Ӧ�ļ���ý���ַ.
            {
                char szSql[1024] = {0};
                otl_stream otlSelect;
                sprintf_s(szSql, sizeof(szSql), "select ServerIP from xy_monitor.mediarecchantime a, xy_base.channelinfo b where a.chanid = b.ID and channo = '%s'", sID);
                if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
                {
                    return "";
                }

                string sFileMeidaIP = "";
                if(!otlSelect.eof())
                {
                    char strServerIP[MAX_STRING_LEN] = {0};
                    otlSelect >> strServerIP;      
                    otlSelect.close();
                    sFileMeidaIP = strServerIP;
                    if (sFileMeidaIP == "")
                    {
                        return "";
                    }
                    int nPort = -1;
                    string sFileMediaID = "";
                    if(m_pRTPManager->GetFileStreamServerInfo(sFileMeidaIP, sFileMediaID, nPort))
                    {
                        return xsip_to_init(sID, sFileMeidaIP.c_str(), nPort);
                    }
                    else
                    {
                        return "";
                    }
                }
            }

            
            switch(pDVRInfo->nNetType)
            {
            case 4: case 7:     //sdk�����豸ȡ��
                if(pCameraInfo != NULL)
                {
                    pCameraInfo->nChanID = pChannelInfo->nChanID;
                    pCameraInfo->sCameraName = pChannelInfo->pName;
                    pCameraInfo->sCameraID = pChannelInfo->pChanno;
                    pCameraInfo->nCameraType = pDVRInfo->nDVRType;
                    pCameraInfo->sDVRKey = pDVRInfo->pDVRKey;
                    pCameraInfo->sDeviceIP = pDVRInfo->pDVRIP;
                    pCameraInfo->nDevicePort = pDVRInfo->nDVRSDKPort;
                    pCameraInfo->sDeviceUsername = pDVRInfo->pUserName;
                    pCameraInfo->sDevicePassword = pDVRInfo->pPassword;
                    pCameraInfo->nChannel = pChannelInfo->nChannel;
                    pCameraInfo->nTranscoding = pDVRInfo->nTranscoding;

                    return xsip_to_init(sID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSDKPort);
                }
            default:
                return xsip_to_init(sID, pDVRInfo->pDVRIP, pDVRInfo->nDVRSIPPort);
            }
		}
		else  //����, ��˵������SIP����Ϊ��ǰSIP�����һ���ӷ�������ӷ�������, ����Ĵ�����ϵ��Ҫ�ٲ���
		{
            if(pCameraInfo != NULL)
            {
                pCameraInfo->nTranscoding = 0;
                pCameraInfo->nNetType = m_pam.m_nNetType;
            }
			bool bFindParent = false;
			string sParentSIPCode = pDVRInfo->pSIPServerCode;
			while(true)
			{          
				if(sParentSIPCode == "")
				{
					g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
                        "***Warning: ��ǰ���ž�ͷ[%s]�����ڵ�ǰSIP[%s]����!", 
                        sID, m_sSIPServerID.c_str());
					break;
				}
				if(m_mapSIPServer[sParentSIPCode].sParentCode == m_sSIPServerID)
				{
					bFindParent = true;
					g_LogRecorder.WriteInfoLogEx(__FUNCTION__, 
                        "�ҵ���ǰSIP����[%s]�¼�SIP����[%s].", m_sSIPServerID.c_str(), 
                        sParentSIPCode.c_str());
					break;
				}

                if(sParentSIPCode == m_mapSIPServer[sParentSIPCode].sParentCode)
                {
                    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, 
                        "***Warning: SIP[%s]���ϼ�����Ϊ�Լ�����!\n����ͻ��������ļ�, ����SIP���������Ϣ!\n", 
                        sParentSIPCode.c_str());
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__,
                        "***Warning: ��ͷ[%s]����SIP����[%s]�뵱ǰSIP����[%s]�޹���!", 
                        sID, pDVRInfo->pSIPServerCode, m_sSIPServerID.c_str());
                    break;
                }

				sParentSIPCode = m_mapSIPServer[sParentSIPCode].sParentCode;
			}
            
            if(bFindParent)
            {
                string sLowSIPServerUri = "";
                if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[sParentSIPCode].sExteranlIP))
                {
                    sLowSIPServerUri = xsip_to_init(sID, (const char *)m_mapSIPServer[sParentSIPCode].sExteranlIP.c_str(), m_mapSIPServer[sParentSIPCode].nExteranlPort);
                }
                else if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[sParentSIPCode].sPoliceNetIP))
                {
                    sLowSIPServerUri = xsip_to_init(sID, (const char *)m_mapSIPServer[sParentSIPCode].sPoliceNetIP.c_str(), m_mapSIPServer[sParentSIPCode].nPoliceNetPort);
                }
                else
                {
                    sLowSIPServerUri = xsip_to_init(sID, (const char *)m_mapSIPServer[sParentSIPCode].sExteranlIP.c_str(), m_mapSIPServer[sParentSIPCode].nExteranlPort);
                }
                return sLowSIPServerUri;
            }
            else      //��ǰ���ž�ͷ�ǵ�ǰSIP�����¼�, ��ͨ��IP�ж��Ƿ��ֱ��ͨ��
			{               
				g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "****Warning: ��ͷ[%s]û���ҵ���ǰת���¼�SIP����!", sID);

                if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[pDVRInfo->pSIPServerCode].sExteranlIP))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "==ֱ����SIP����[%s]����ͷ[%s]��Ƶ...", pDVRInfo->pSIPServerCode, sID);
                    return xsip_to_init(sID, (const char *)m_mapSIPServer[pDVRInfo->pSIPServerCode].sExteranlIP.c_str(), m_mapSIPServer[pDVRInfo->pSIPServerCode].nExteranlPort);
                }
                else if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[pDVRInfo->pSIPServerCode].sPoliceNetIP))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "==ֱ����SIP����[%s]����ͷ[%s]��Ƶ...", pDVRInfo->pSIPServerCode, sID);
                    return xsip_to_init(sID, (const char *)m_mapSIPServer[pDVRInfo->pSIPServerCode].sPoliceNetIP.c_str(),
                        m_mapSIPServer[pDVRInfo->pSIPServerCode].nPoliceNetPort);
                }
                else 
                {                   
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__,
                        "**Warning: �޷�ֱ��ͨ�������豸[%s]��Ƶ, ����SIP����[%s]", sID, pDVRInfo->pSIPServerCode);
                    return "";
                }
			}
		}
	}
   
	else
	{
		g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ��ͷ[%s]�����ڻ�δ����SIP����!", sID);
		return "";
	}
    return "";
}

string CSIPServer::GetNowTime(int ch)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	char szUpdateTime[32] = {0};
	if (ch == 0)
	{
		time_t t = time(NULL);
		sprintf_s(szUpdateTime, sizeof(szUpdateTime), "%lld", t);
	}
	else if (ch == 1)
	{
		sprintf_s(szUpdateTime, sizeof(szUpdateTime), "%4d%02d%02dT%02d%02d%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
	}
	else if (ch == 2)
	{
		sprintf_s(szUpdateTime, sizeof(szUpdateTime), "%4d-%02d-%02d %02d:%02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
	}
	else if (ch == 3)
	{
		sprintf_s(szUpdateTime, sizeof(szUpdateTime), "%4d-%02d-%02dT%02d:%02d:%02d.%03d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,st.wMilliseconds);
	}
	else if (ch == 4)
	{
		sprintf_s(szUpdateTime, sizeof(szUpdateTime), "%4d%02d02d%02d%02d", st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute);
	}
	return szUpdateTime;
}

string CSIPServer::GetFormatTime(const char *time, int md)
{
	int nYear, nMon, nDay, nHour, nMin, nSec;
    string sTime(time);
    if(sTime.find("-") != -1)
    {
        sscanf_s(time, "%d-%d-%d %d:%d:%d", &nYear,&nMon,&nDay,&nHour,&nMin,&nSec);  //32λ����
    }else
    {
	
        sscanf_s(time, "%d/%d/%d %d:%d:%d", &nYear,&nMon,&nDay,&nHour,&nMin,&nSec);   //64λ����
    }
	char szTime[20] = {0};
	if (md == 0)
	{
		sprintf_s(szTime, sizeof(szTime), "%04d-%02d-%02dT%02d:%02d:%02d", nYear,nMon,nDay,nHour,nMin,nSec);
	}
	else if (md == 1)
	{
		sprintf_s(szTime, sizeof(szTime), "%04d-%02d-%02d %02d:%02d:%02d", nYear,nMon,nDay,nHour,nMin,nSec);
	}
	return szTime;
}

void CSIPServer::SetLocalTime(string sTime)
{
	if (sTime == "")
	{
		return;
	}
	int nYear,nMon,nDay,nHour,nMin,nSec,nMis;
	sscanf_s(sTime.c_str(), "%4d-%02d-%02dT%02d:%02d:%02d.%03d", &nYear,&nMon,&nDay,&nHour,&nMin,&nSec,&nMis);
	nMis += 200;
	if (nMis >= 1000)
	{
		nMis = 999;
	}
	SYSTEMTIME st;
	st.wYear = nYear;
	st.wMonth = nMon;
	st.wDay = nDay;
	st.wHour = nHour;
	st.wMinute = nMin;
	st.wSecond = nSec;
	st.wMilliseconds = nMis;
	::SetLocalTime(&st);
}

string CSIPServer::ReConver(const TiXmlElement *Element)
{
	if (Element != NULL)
	{
		if (Element->FirstChild() != NULL)
		{
			char *pValue = _utoa(Element->FirstChild()->Value());
			string sValue = pValue;
			delete []pValue;
			return sValue;
			
		}
		return "";
	}
	return "";
}

string CSIPServer::ReValue(const TiXmlElement *Element)
{
	if (Element != NULL)
	{
		if (Element->FirstChild() != NULL)
		{
			return Element->FirstChild()->Value();
		}
		return "";
	}
	return "";
}

int CSIPServer::ReIntValue(const TiXmlElement *Element)
{
	if (Element != NULL)
	{
		if (Element->FirstChild() != NULL)
		{
			return atoi(Element->FirstChild()->Value());
		}
		return 0;
	}
	return 0;
}

double CSIPServer::ReDoubleValuve(const TiXmlElement *Element)
{
	if (Element != NULL)
	{
		if (Element->FirstChild() != NULL)
		{
			return strtod(Element->FirstChild()->Value(), NULL);
		}
		return 0;
	}
	return 0;
}



int CSIPServer::SuperiorSendRegMsg(void)
{
	SOCKADDR_IN addr;
	char szReg[DATA_BUFSIZE] = {0};
	int expires = 3600;
	int len = 0;

    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
    for (; itSup != m_mapSuperior.end(); itSup ++)
    {
		memset(&addr, 0, sizeof(SOCKADDR_IN));
		memset(szReg, 0, DATA_BUFSIZE);
		string sProxy = GetProxy(itSup->second->pDeviceUri);
		SetDestAddr(itSup->second->pDeviceUri, &addr);
		xsip_register_build_initial(szReg, sProxy.c_str(), itSup->second->pDeviceUri);

		len = (int)strlen(szReg);
		szReg[len] = '\0';
		m_pSIPProtocol->sendBuf(addr, szReg, len);
	}
	return 0;
}
int CSIPServer::SuperiorRegAnswer(SOCKADDR_IN addr, char * sRegAnswer)            //���ϼ�ע��Ļ�Ӧ��Ϣ
{
    SOCKADDR_IN sour = {0};
    int nExp = xsip_get_expires(sRegAnswer);
    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();

    xsip_event_type nEvent = xsip_get_event_type(sRegAnswer);
    if(nEvent == XSIP_REGSUPSERVER_SUCCESS)
    {
        for (; itSup != m_mapSuperior.end(); itSup ++)
        {
            memset(&sour, 0, sizeof(sour));
            SetDestAddr(itSup->second->pDeviceUri, &sour);
            if (addr.sin_addr.S_un.S_addr == sour.sin_addr.S_un.S_addr)
            {
                if (nExp != 0)
                {
                    if (itSup->second->bReg == false)
                    {
                        itSup->second->bReg = true;
                        string sTime = xsip_get_date(sRegAnswer);
                        if (sTime == "")
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���ϼ�����[%s]ע��ɹ�!", itSup->second->pDeviceUri);
                        }
                        else
                        {
                            SetLocalTime(sTime);
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���ϼ�����[%s]ע��ɹ�.\nͬ��УʱΪ:%s", itSup->second->pDeviceUri,sTime.c_str());
                        }
                    }	
                }
                else
                {
                    m_bRegisterOut = false;
                    if (itSup->second->bReg == true)
                    {
                        itSup->second->bReg = false;
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���ϼ�����[%s]ע���ɹ�!", itSup->second->pDeviceUri);

                        //��ע��
                        char szReg[DATA_BUFSIZE] = {0};
                        string sProxy = GetProxy(itSup->second->pDeviceUri);
                        xsip_register_build_initial(szReg, sProxy.c_str(), itSup->second->pDeviceUri);

                        DWORD len = (DWORD)strlen(szReg);
                        szReg[len] = '\0';
                        m_pSIPProtocol->sendBuf(addr, szReg, len);
                    }
                }
            }
        }
    }
    else if(nEvent == XSIP_REGSUPSERVER_FAILURE)
    {
        if (string(sRegAnswer).find("401") != string::npos)	//��Ҫ����
        {
            string sRealm = xsip_get_www_authenticate_realm(sRegAnswer);
            string sNonce = xsip_get_www_authenticate_nonce(sRegAnswer);
            if (sRealm == "" || sNonce == "")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ע��ʧ��,Realm=\"%s\",sNonce=\"%s\"!<\n\n", sRealm.c_str(), sNonce.c_str());
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, sRegAnswer);
                return -1;
            }
            string sUri = GetRegisterAddr(addr);
            string sResponse = xsip_build_auth_response(m_pam.m_sRegSupUsername, sRealm, m_pam.m_sRegSupPassword, sNonce, sUri);
            char szAuth[512] = {0};
            sprintf_s(szAuth, sizeof(szAuth), 
                "Digest username=\"%s\",realm=\"%s\","
                "nonce=\"%s\",uri=\"%s\","
                "response=\"%s\",algorithm=MD5",
                m_pam.m_sRegSupUsername.c_str(), sRealm.c_str(), sNonce.c_str(), sUri.c_str(), sResponse.c_str());
            char szReg[DATA_BUFSIZE] = {0};
            string ftag = xsip_get_from_tag(sRegAnswer);
            string callid = xsip_get_call_id(sRegAnswer);
            string sProxy = GetProxy(sUri);
            if (!m_bRegisterOut)				//����֤����ע��
            {
                xsip_register_build_initial(szReg, sProxy.c_str(), sUri.c_str(), callid.c_str(), 2, ftag);
            }
            else								//����֤����ע��
            {
                xsip_register_build_initial(szReg, sProxy.c_str(), sUri.c_str(), callid.c_str(), 2, ftag, 0);
            }
            xsip_set_authorization(szReg, szAuth);
            DWORD dwLen = (DWORD)strlen(szReg);
            m_pSIPProtocol->sendBuf(addr, szReg, dwLen);
        }
        else
        {
            string sUri = GetRegisterAddr(addr);
            if (!m_bRegisterOut)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>���ϼ�[%s]ע��ʧ��,����ϼ�����!<\n\n", sUri.c_str());
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>���ϼ�[%s]ע��ʧ��,����ϼ�����!<\n\n", sUri.c_str());
            }
        }
    }

    
    return 0;
}

int CSIPServer::RebootMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��豸������Ϣ!");
    bool bSup = false;
    string sAddr = xsip_get_via_addr(msg);
    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
    for (; itSup != m_mapSuperior.end(); itSup++)
    {
        if (sAddr == itSup->second->pDeviceUri)		//�ϼ����豸�����������ע��
        {
            string sDevice = xsip_get_uri(msg, "<DeviceID>", "</DeviceID>", MAX_USERNAME_LEN);
            if (sDevice == XSIP::m_sUsername)
            {
                bSup = true;
                char szAns[DATA_BUFSIZE] = {0};
                string dwttag = xsip_get_to_tag(msg);
                if (dwttag == "")
                {
                    dwttag = xsip_get_tag();
                }
                xsip_message_build_answer(msg,200,szAns,dwttag);
                DWORD dwLen = (DWORD)strlen(szAns);
                szAns[dwLen] = '\0';
                m_pSIPProtocol->sendBuf(addr, szAns, dwLen);

                char szReg[DATA_BUFSIZE] = {0};
                string sProxy = GetProxy(itSup->second->pDeviceUri);
                //string sProxy = m_mapSuperior[i].addr;
                xsip_register_build_initial(szReg, sProxy.c_str(), itSup->second->pDeviceUri, "", 1, "", 0);

                int len = (int)strlen(szReg);
                szReg[len] = '\0';
                m_bRegisterOut = true;
                m_pSIPProtocol->sendBuf(addr, szReg, len);
            }
            else
            {
                bSup = false;
            }
            break;
        }
    }
    if (!bSup)					//�ϼ����豸�������ת���¼�
    {
        char szAns[DATA_BUFSIZE] = {0};
        string dwttag = xsip_get_to_tag(msg);
        if (dwttag == "")
        {
            dwttag = xsip_get_tag();
        }
        xsip_message_build_answer(msg,200,szAns,dwttag);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);

        string sDevice = xsip_get_uri(msg, "<DeviceID>", "</DeviceID>", MAX_USERNAME_LEN);
        string sUri = GetDeviceAddr(sDevice.c_str());
        if (sUri != "LOCAL" && sUri != "")
        {
            string dwftag = xsip_get_from_tag(msg);
            int nceq = xsip_get_cseq_number(msg);
            string sCallid = xsip_get_call_id(msg);
            string sBody = xsip_get_body(msg);
            memset(szAns, 0, DATA_BUFSIZE);
            xsip_message_build_request(szAns,"MESSAGE",NULL,sUri.c_str(),XSIP::m_sFrom.c_str(),sCallid.c_str(),nceq,dwttag,dwftag);
            xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
            xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
            SOCKADDR_IN sockin = {0};
            SetDestAddr(sUri, &sockin);
            dwLen = (DWORD)strlen(szAns);
            szAns[dwLen] = '\0';
            m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//ת���¼�
        }
    }
	return 0;
}



int CSIPServer::DeviceKeepaliveRecv(SOCKADDR_IN &addr, char *msg)
{
    string sBody = xsip_get_body(msg);
    if (sBody == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Warning: Keepalive��Ϣû��XML��Ϣ!");
        return -1;
    }
    string sDeviceID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);

	char szMsg[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
    string dwToTag = xsip_get_to_tag(msg);
    if (dwToTag == "")
    {
        dwToTag = xsip_get_tag();
    }

    xsip_message_build_answer(msg, 200, szMsg, dwToTag);
    dwLen = (DWORD)strlen(szMsg);
    szMsg[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);
    
    EnterCriticalSection(&m_vLock);
    DVRINFOMAP::iterator itDev = m_mapDevice.find(sDeviceID);
    if (itDev != m_mapDevice.end())
    {
        itDev->second->bReg = true;
        itDev->second->tRegistTime = time(&itDev->second->tRegistTime);
    }
    LeaveCriticalSection(&m_vLock);
    
	return 0;
}

int CSIPServer::SendCatalogMsg(SOCKADDR_IN &addr, string uri)
{
	string sDeviceID = xsip_get_url_username((char*)uri.c_str());
	char szSN[12] = {0};
	sprintf_s(szSN, sizeof(szSN), "%u", m_dwSn++);
	char szCatalog[DATA_BUFSIZE] = {0};
	xsip_message_build_request(szCatalog,"MESSAGE",NULL,uri.c_str(),NULL,NULL,20);
	char szBody[MAX_CONTENT_LEN] = {0};
	sprintf_s(szBody, sizeof(szBody),
		"<?xml version=\"1.0\"?>\r\n"
		"<Query>\r\n"
		"<CmdType>Catalog</CmdType>\r\n"
		"<SN>%s</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"</Query>\r\n",
		szSN, sDeviceID.c_str());
	xsip_set_content_type(szCatalog, XSIP_APPLICATION_XML);
	xsip_set_body(szCatalog, szBody, (int)strlen(szBody));
	DWORD dwLen = (DWORD)strlen(szCatalog);
	szCatalog[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szCatalog, dwLen);	

	SESSIONINFO Info;
	Info.sTitle = "CATALOGSUB";
    time(&Info.recvTime);
	EnterCriticalSection(&m_sLock);
	m_SessionMap.insert(SessionMap::value_type(szSN, Info));	//����Ự��Ϣ
	LeaveCriticalSection(&m_sLock);
	return 0;
}
int CSIPServer::DeviceCatalogAnswer(SOCKADDR_IN &addr, char *msg)
{
    char szAns[DATA_BUFSIZE] = {0};
    DWORD dwLen = 0;
    //char *pBuf = _atou(msg);
    string sBody = xsip_get_body(msg);
    string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
    string sDeviceID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
    string sTag = xsip_get_to_tag(msg);
    if (sBody == "" && sDeviceID == "" && sTag == "")
    {
        xsip_message_build_answer(msg,400,szAns,sTag);
        dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ�400 BAD
        return -1;
    }

    EnterCriticalSection(&m_sLock);
    SessionMap::iterator iter = m_SessionMap.find(sSN);
    if (iter != m_SessionMap.end())
    {
        xsip_message_build_answer(msg, 200, szAns, iter->second.dwTTag);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);				//�ظ�200 OK

        if (iter->second.sTitle == "CATALOGSUB")				//Ŀ¼����
        {
            LPCATALOGINFO pCatalogInfo = new CATALOGINFO;
            memcpy(&pCatalogInfo->addr, &addr, sizeof(SOCKADDR_IN));
            pCatalogInfo->sBody = sBody;
            pCatalogInfo->sDeviceID = sDeviceID;
            m_listCatalogInfo.push_back(pCatalogInfo);      //����ͷĿ¼���浽����

            TiXmlDocument *pMyDocument = new TiXmlDocument();
            pMyDocument->Parse(sBody.c_str());
            TiXmlElement *pRootElement = pMyDocument->RootElement();
            string sSumNum = ReValue(pRootElement->FirstChildElement("SumNum"));
            if (sSumNum != "" && sSumNum != "0")
            {
                string sListNum = "0";
                TiXmlElement *pRecordList = pRootElement->FirstChildElement("DeviceList");
                if(NULL != pRecordList)
                {
                    TiXmlAttribute* attr = pRecordList->FirstAttribute();
                    if(NULL != attr)
                    {
                        sListNum = attr->Value();
                    }
                }

                iter->second.nNum += atoi(sListNum.c_str());
                {
                    //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�����վ�ͷĿ¼[%d].", iter->second.nNum);
                }
                if (iter->second.nNum >= atoi(sSumNum.c_str()))
                {					
                    LPCATALOGINFO pCatalogInfo = new CATALOGINFO;
                    memcpy(&pCatalogInfo->addr, &addr, sizeof(SOCKADDR_IN));
                    pCatalogInfo->sBody = "End";
                    pCatalogInfo->sDeviceID = sDeviceID;
                    m_listCatalogInfo.push_back(pCatalogInfo);

                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���վ�ͷĿ¼���[%d]!", iter->second.nNum);
                    iter = m_SessionMap.erase(iter);

                }
            }
            else if(sSumNum == "0")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: �豸�޾�ͷĿ¼!");
            }
            delete pMyDocument;
            LeaveCriticalSection(&m_sLock);
            return 0;
        }		
    }
    else
    {
        xsip_message_build_answer(msg,404,szAns,sTag);
        dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
        LeaveCriticalSection(&m_sLock);
        return -1;
    }
    LeaveCriticalSection(&m_sLock);
    return 0;
}
DWORD CSIPServer::SaveCatalogInfoThread(LPVOID lpParam)
{
    CSIPServer * pThis = (CSIPServer*)lpParam;
    pThis->SaveCatalogInfoAction();
    return 0;
}
void CSIPServer::SaveCatalogInfoAction()
{
    while(WAIT_OBJECT_0 != WaitForSingleObject(m_hStopEvent, 0))
    {
        ListCatalogInfo::iterator itCatalog = m_listCatalogInfo.begin();
        while(itCatalog != m_listCatalogInfo.end())
        {
            if(m_listCatalogInfo.size() % 200 == 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ʣ��%d��ͷĿ¼��Ϣ����...", m_listCatalogInfo.size());
            }
            if((*itCatalog)->sBody == "End")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ƽ̨|�豸[%s]��ͷĿ¼���ص����ݿ����..", (*itCatalog)->sDeviceID.c_str());
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ʼ����ƽ̨|�豸[%s]���ྵͷĿ¼..", (*itCatalog)->sDeviceID.c_str());
                DeleteOvertimeChannel((*itCatalog)->sDeviceID);
            }
            else
            {
                SaveCatalogToDB((*itCatalog)->addr, (*itCatalog)->sBody.c_str(), (*itCatalog)->sDeviceID.c_str());
            }
            delete (*itCatalog);
            itCatalog = m_listCatalogInfo.erase(itCatalog);
        }

        Sleep(100);
    }
    return;
}
int CSIPServer::DeleteOvertimeChannel(string sDeviceID)
{
    char pUpdateTime[32] = {0};
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    sprintf_s(pUpdateTime, sizeof(pUpdateTime), "%04d-%02d-%02d %02d:%02d:%02d", 
        sysTime.wYear, sysTime.wMonth, sysTime.wDay, 
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

    int nSecond = ChangeTimeToSecond(pUpdateTime);
    nSecond -= 3600 * 1;
    string sTime = ChangeSecondToTime(nSecond);

    char sSql[4096] = {0};
    otl_stream otlSelect;

    sprintf_s(sSql, sizeof(sSql), 
        "select deviceid, Name from %s where updatetime < TO_DATE('%s','yyyy-mm-dd hh24:mi:ss') and serverid = '%s'", 
        m_sSIPChannelTable.c_str(), sTime.c_str(), sDeviceID.c_str());
    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
    {
        g_LogRecorder.WriteWarnLogEx(__FUNCTION__,
            "^^^^Error: ��ѯOvertime��ͷsipchannel�����[%s].", sTime.c_str());
        return false;
    }
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "����ƽ̨|�豸[%s]��ͷĿ¼, SQL[%s].", sDeviceID.c_str(), sSql);

    char pCameraID[32] = {0};
    char pName[128] = {0};
    while(!otlSelect.eof())
    {
        otlSelect >> pCameraID >> pName;
        DelCameraInfo(pCameraID);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Del Overtime Channel[%s][%s]", pCameraID, pName);
    }
    otlSelect.close();
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "����ƽ̨|�豸[%s]���ྵͷĿ¼����.", sDeviceID.c_str());
    return 0;
}
int CSIPServer::SaveCatalogToDB(SOCKADDR_IN &addr, const char *xml, const char *sServerID, char *pSn)
{
    if(!m_pam.m_nWriteDB)
    {
        return 0;
    }

 	char *pBuf = _atou(xml);
	BOOL bRet = FALSE;
	TiXmlDocument *pMyDocument = new TiXmlDocument();
	pMyDocument->Parse(pBuf);
	TiXmlElement *pRootElement = pMyDocument->RootElement();
	

	TiXmlElement *pDeviceList = pRootElement->FirstChildElement("DeviceList");
	if (pDeviceList == NULL)
	{
		delete pMyDocument;
		delete []pBuf;
		return -1;
	}
	TiXmlElement * pItem = pDeviceList->FirstChildElement("Item");
    TiXmlElement * pPtzInfo;
	static int ch = 1;
	while (pItem != NULL)
	{		
        CATLOGNOTIFYTYPE nCatalogType = NONOCATALOG;
		string sDeviceID = ReConver(pItem->FirstChildElement("DeviceID"));
        if(sDeviceID.size() == 20 &&string(sDeviceID, 10, 3) == "134")
        {
            pItem = pItem->NextSiblingElement("Item");
            continue;
        }

        if(string(pBuf).find("<Event>") != string::npos)    //HKƽ̨Notify֪ͨ��Ϣ��������
        {
            string sCatalogType = ReConver(pItem->FirstChildElement("Event"));
            nCatalogType = sCatalogType == "DEL" ? DELCATALOG : ADDCATALOG;       //1���Ӿ�ͷ, 2ɾ��
            if(DELCATALOG == nCatalogType)
            {
                if(!DelCameraInfo(sDeviceID))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: �����ݿ�ɾ����ͷ[%s]��Ϣʧ��.", sDeviceID.c_str());
                }
                pItem = pItem->NextSiblingElement("Item");
                continue;
            }           
        }
		string sName = ReConver(pItem->FirstChildElement("Name"));
        if(sName == "")
        {
            sName = "Camera";
        }

        if( (sDeviceID.size() == 20 && string(sDeviceID, 10, 2) != "13") ||
            sDeviceID.size() <= 10)
        {
            string sParentID = ReConver(pItem->FirstChildElement("ParentID"));
            /*if(sParentID == "")
            {
                sParentID = sServerID;
            }*/
            if(!CreateChanGroupInfo(sDeviceID, sName, sParentID))
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "��ChanGroupInfo�д�����ͷ��[%s : %s]ʧ��.", sDeviceID.c_str(), sName.c_str());
            }

            pItem = pItem->NextSiblingElement("Item");
            continue;
        }

        string sParentID = ReConver(pItem->FirstChildElement("ParentID"));
        if(sParentID == "")
        {
            sParentID = sServerID;
        }
        
		string sManufacturer = ReConver(pItem->FirstChildElement("Manufacturer"));
		string sModel = ReConver(pItem->FirstChildElement("Model"));
		string sOwner = ReConver(pItem->FirstChildElement("Owner"));
		string sCivilCode = ReConver(pItem->FirstChildElement("CivilCode"));
        if(string(sCivilCode, 0, 4) != string(m_sSIPServerID, 0, 4))
        {   
            sCivilCode = m_sRealm;
        }
		string sBlock = ReConver(pItem->FirstChildElement("Block"));
        if(sBlock == "")
        {
            sBlock.assign(sDeviceID, 0, 10);
        }
		string sAddress = ReConver(pItem->FirstChildElement("Address"));
        if(sAddress.size() > 64)
        {
            sAddress.assign(sAddress, 0, 64);
        }
        sAddress = "Address";
		string sParental = ReConver(pItem->FirstChildElement("Parental"));
		
		string sSafetyWay = ReConver(pItem->FirstChildElement("SafetyWay"));
		string sRegisterWay = ReConver(pItem->FirstChildElement("RegisterWay"));
		string sCertNum = ReConver(pItem->FirstChildElement("CertNum"));
		string sCertifiable = ReConver(pItem->FirstChildElement("Certifiable"));
		string sErrCode = ReConver(pItem->FirstChildElement("ErrCode"));
		string sEndTime = ReConver(pItem->FirstChildElement("EndTime"));
		string sSecrecy = ReConver(pItem->FirstChildElement("Secrecy"));
		string sIPAdderss = ReConver(pItem->FirstChildElement("IPAddress"));
		string sPort = ReConver(pItem->FirstChildElement("Port"));
        //���������ȡ��IP, Portû������
        sIPAdderss = inet_ntoa(addr.sin_addr);
        int nPort = ntohs(addr.sin_port);

		string sPassword = ReConver(pItem->FirstChildElement("Password"));
		string sStatus = ReConver(pItem->FirstChildElement("Status"));
		string sLongitude = ReConver(pItem->FirstChildElement("Longitude"));
		string sLatitude = ReConver(pItem->FirstChildElement("Latitude"));
		string sSourceID =  ReConver(pItem->FirstChildElement("ServerID"));
        int nPTZType = 0;
        if(string(pBuf).find("PTZType") != string::npos)
        {
            pPtzInfo = pItem->FirstChildElement("Info");

            string sPtzType = ReConver(pPtzInfo->FirstChildElement("PTZType"));
            nPTZType = atoi(sPtzType.c_str());
            nPTZType = nPTZType == 3 ? 0 : nPTZType;
        }        
		pItem = pItem->NextSiblingElement();
		if (sDeviceID == "")
		{
			g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "ȡĿ¼��Ϣ, ��ͷIDΪ��.");
            continue;
		}

        if(sStatus == "")
        {
            sStatus = "OFF";
        }

        /**************���豸��Ϣд�����ݿ�*************/
		char sSql[4096] = {0};
        otl_stream otlSelect;
        bool bExist = false;
        char pUpdateTime[32] = {0};
        SYSTEMTIME sysTime;
        GetLocalTime(&sysTime);
        sprintf_s(pUpdateTime, sizeof(pUpdateTime), "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear, sysTime.wMonth, sysTime.wDay, 
            sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

        //�ж�sDeviceID�Ƿ�ƽ̨|�豸(DVR, NVR)����, ����˵����ǰ��ϢΪ������֯|�豸���, ���Ǿ�ͷ, �򴴽���ͷ����
        if(IsPlatform(sDeviceID) || IsDevice(sDeviceID) || IsChanGroup(sDeviceID) || sDeviceID.size() <= 10)
        {
            if(!CreateChanGroupInfo(sDeviceID, sName, sParentID))
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "��ChanGroupInfo�д�����ͷ��[%s : %s]ʧ��.", sDeviceID.c_str(), sName.c_str());
            }
            continue;
        }
        
        //���Ҿ�ͷ��ӦDVR ID
        int nDvrID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from %s where dvrip = '%s'", m_sDVRInfoTable.c_str(), sIPAdderss.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ��ȡdvrinfo��DVR[%s] ID����.", sIPAdderss.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> nDvrID;
        }
        otlSelect.close();
        if(nDvrID == 0 )
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ����DVRIP[%s]δ�ҵ�DVRInfo��¼.", sIPAdderss.c_str());
            return false;
        }

        //����channelInfo�����Ƿ���ڴ˾�ͷ
        bExist = false;
        int nChanID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from %s where Channo = '%s' and ID >= 10000000 ", m_sChannelInfoTable.c_str(), sDeviceID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯ��ͷ[%s]Ŀ¼�Ƿ񼺴���channelinfo�����.", sDeviceID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            bExist = true;     //��ͷĿ¼������.
            otlSelect >> nChanID;
        }
        otlSelect.close();

        if(!bExist) //������, ����뾵ͷ
        {
            sprintf_s(sSql, sizeof(sSql), "INSERT INTO %s(ID, DVRID, Channo, ChanName, ChanDesc, ChanIndex, "
                "ChanType, IsPTZ, IDPrivate, Px, Py, AreaID, ISCONNECTVIDEOCABLE, "
                "VISCHANNO, CHANQUALITY, CHANSTRINDEX, AddTime, UPDATETIME) "
                "VALUES (xy_base.seq_channelinfo_id_gb.nextval, %d, '%s','%s','%s', 1, 1, %d, 0, %lf, %lf, 0, 1, 0, 0, 0, " 
                "TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'), TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'))",
                m_sChannelInfoTable.c_str(), nDvrID, sDeviceID.c_str(), sName.c_str(), sName.c_str(), nPTZType, 
                strtod(sLongitude.c_str(), NULL), strtod(sLatitude.c_str(), NULL), pUpdateTime, pUpdateTime);

            m_DBMgr.ExecuteSQL(sSql, otlSelect, true);
            otlSelect.close();

            sprintf_s(sSql, sizeof(sSql), "select ID from %s where Channo = '%s'",  m_sChannelInfoTable.c_str(), sDeviceID.c_str());
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯ��ͷ[%s]ID, ִ��SQL���ʧ��.", sDeviceID.c_str());
                return false;
            }
            if(!otlSelect.eof())
            {
                otlSelect >> nChanID;
            }
            else
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: δ��ȡ����ͷ[%s]ID.", sDeviceID.c_str());
                return false;
            }   
            otlSelect.close();

            //���뾵ͷ������Ϣ��XY_MONITOR.DATACHANGE, ֪ͨ��������

            /*sprintf_s(sSql, sizeof(sSql), "Insert Into XY_MONITOR.DATACHANGE(ID, TableName, ChangeID, ChangeType, ChangeTime) "
                "Values (XY_MONITOR.seq_datachange_id.nextval, 'ChannelInfo', %d, 2, TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'))",
                nChanID, pUpdateTime);
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
                return false;
            otlSelect.close();    */
        }
        else
        {
            sprintf_s(sSql, sizeof(sSql), "Update %s "
                "Set ChanName = '%s', IsPTZ = %d, UpdateTime = TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss') Where Channo = '%s'",
                m_sChannelInfoTable.c_str(), sName.c_str(), nPTZType, pUpdateTime, sDeviceID.c_str());

            m_DBMgr.ExecuteSQL(sSql, otlSelect, true);
            otlSelect.close();
        }


        //sipChannel�����Ƿ���ڴ˾�ͷ        
        bExist = false;
        sprintf_s(sSql, sizeof(sSql), "select * from %s where ID = %d", m_sSIPChannelTable.c_str(), nChanID);
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "^^^^Error: ��ѯ��ͷID[%d]�Ƿ񼺴���sipchannel�����.", nChanID);
            return false;
        }
        while(!otlSelect.eof())
        {
            bExist = true;     //��ͷĿ¼������.
            break;
        }
        otlSelect.close();
        if(!bExist)     //�����������
		{           
            if(sEndTime == "")
            {
                sEndTime = pUpdateTime;
            }
            else
            {
                sEndTime.replace(10, 1, " ");
            }

			sprintf_s(sSql, sizeof(sSql), "INSERT INTO %s(ID, DeviceID, Name, Manufacturer, Model, Owner, CivilCode, Block, "
                "Address, Parental, ParentID, SafetyWay, RegisterWay, Certnum, Certifiable, Errcode, Endtime, Secrecy, "
                "IPAddress, Port, Password, Status, Longitude, Latitude, ServerID, UpdateTime) "
                "VALUES (%d, '%s','%s','%s','%s','%s','%s','%s','%s', %d, '%s', %d, %d, '%s', %d, "
                "%d, TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'), %d, '%s', %d, '%s', '%s', %lf, %lf, '%s', TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss'))",
				m_sSIPChannelTable.c_str(), nChanID, sDeviceID.c_str(), sName.c_str(), sManufacturer.c_str(), sModel.c_str(), sOwner.c_str(), sCivilCode.c_str(), sBlock.c_str(), sAddress.c_str(), 
				atoi(sParental.c_str()), sServerID, atoi(sSafetyWay.c_str()), atoi(sRegisterWay.c_str()), sCertNum.c_str(), atoi(sCertifiable.c_str()), atoi(sErrCode.c_str()), 
				sEndTime.c_str(), atoi(sSecrecy.c_str()), sIPAdderss.c_str(), nPort, sPassword.c_str(), sStatus.c_str(), strtod(sLongitude.c_str(), NULL), strtod(sLatitude.c_str(), NULL), 
				sServerID, pUpdateTime);
		}
		else         //��������¾�ͷ��Ϣ
		{
			sprintf_s(sSql, sizeof(sSql), "UPDATE %s set Name = '%s', IPAddress = '%s', Port = %d, "
                "Status = '%s', ParentID = '%s', ServerID = '%s', UpdateTime = TO_DATE('%s', 'yyyy-mm-dd hh24:mi:ss') "
                "where id = %d",
                m_sSIPChannelTable.c_str(), sName.c_str(), sIPAdderss.c_str(), nPort, 
                sStatus.c_str(), sServerID, sServerID, pUpdateTime, nChanID);
		}
		m_DBMgr.ExecuteSQL(sSql, otlSelect, true);
        otlSelect.close();
		m_nCatalogNum++;
       
        //changrouptochan�����Ƿ���ڴ˾�ͷ
        bExist = false;
        sprintf_s(sSql, sizeof(sSql), "select * from %s where NodeNo = '%s'", m_sChanGroupToChanTable.c_str(), sDeviceID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ѯ��ͷ[%s]�Ƿ񼺴���changrouptochan�����.", sDeviceID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            bExist = true;     //��ͷ������.
        }
        otlSelect.close();

        int nChanGroupID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupinfo where CopyParentID = '%s'", sParentID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: ��ȡchangroupinfo��[%s]ID����.", sParentID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> nChanGroupID;
        }
        else
        {
            otlSelect.close();
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "^^^^Warning: ����changroupinfo��[%s]�޼�¼.", sParentID.c_str());
            continue;
        }
        otlSelect.close();

        if(!bExist)     //���뾵ͷ��changrouptochan
        {    
            sprintf_s(sSql, sizeof(sSql), "Insert into %s"
                "(ID, ChanGroupID, ChanID, NodeNo, NodeName, NodeType) "
                "Values (xy_base.seq_changrouptochan_id.nextval, %d, %d, '%s', '%s', %d)",
                m_sChanGroupToChanTable.c_str(), nChanGroupID, nChanID, sDeviceID.c_str(), sName.c_str(), 0);

            m_DBMgr.ExecuteSQL(sSql, otlSelect, true);
            otlSelect.close();              
        }
        else
        {
            sprintf_s(sSql, sizeof(sSql), "Update %s Set ChanGroupID = %d, NodeName = '%s' Where NodeNo = '%s'",
                m_sChanGroupToChanTable.c_str(), nChanGroupID, sName.c_str(), sDeviceID.c_str());

            m_DBMgr.ExecuteSQL(sSql, otlSelect, true);
            otlSelect.close();
        }    

    }
		
        /**************���豸��Ϣд�����ݿ����*************/
	delete pMyDocument;
	delete []pBuf;
	return bRet;
}


int CSIPServer::SendSubscribeMsg(SOCKADDR_IN &addr, char *msg)
{
    SYSTEMTIME sysTime;
    GetLocalTime(&sysTime);
    char pStartTime[128] = {0};
    char pEndTime[128] = {0};
    sprintf_s(pStartTime, sizeof(pStartTime), "%04d-%02d-%02dT%02d:%02d:%02d", 
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

    sprintf_s(pEndTime, sizeof(pEndTime), "%04d-%02d-%02dT%02d:%02d:%02d",
        sysTime.wYear, sysTime.wMonth, sysTime.wDay+1,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);


	char subscribe[DATA_BUFSIZE] = {0};
	string sTo = xsip_get_to_url(msg);
	string sUserID = xsip_get_url_username((char *)sTo.c_str());
	xsip_subscribe_build_initial(subscribe, sTo.c_str());
	char body[MAX_CONTENT_LEN] = {0};
	sprintf_s(body, sizeof(body), 
		"<?xml version=\"1.0\"?>\r\n"
		"<Query>\r\n"
		"<CmdType>Catalog</CmdType>\r\n"
		"<SN>%d</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"<StartTime>%s</StartTime>\r\n"
		"<EndTime>%s</EndTime>\r\n"
		"</Query>\r\n",
		m_dwSn++, sUserID.c_str(), pStartTime, pEndTime);
	int nLen = (int)strlen(body);
	body[nLen] = '\0';
	xsip_set_content_type(subscribe, "Application/MANSCDP+xml");
	xsip_set_body(subscribe, body, nLen);
	DWORD len = (DWORD)strlen(subscribe);
	subscribe[len] = '\0';
    m_pSIPProtocol->sendBuf(addr, subscribe, len);
	
    return 0;
}
int CSIPServer::DeviceNotifyAnswer(SOCKADDR_IN &addr, char *msg)
{
    int nStatus = 200;
    string sCallid = xsip_get_call_id(msg);
    string sBody = xsip_get_body(msg);

    string sTo = xsip_get_from_url(msg);
    string sDeviceID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
    string sDeviceIP = inet_ntoa(addr.sin_addr);
    int nDevicePort = ntohs(addr.sin_port);
    string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>","</SN>", MAX_USERNAME_LEN);

    char szAns[DATA_BUFSIZE] = {0};
    xsip_notify_build_answer(msg, nStatus, szAns);
    char szBody[MAX_CONTENT_LEN] = {0};
    sprintf_s(szBody, sizeof(szBody), 
        "<?xml version=\"1.0\"?>\r\n"
        "<Response>\r\n"
        "<CmdType>Catalog</CmdType>\r\n"
        "<SN>%s</SN>\r\n"
        "<DeviceID>%s</DeviceID>\r\n"
        "<Result>OK</Result>\r\n"
        "</Response>\r\n",
        sSN.c_str(), sDeviceID.c_str());
    int nLen = (int)strlen(szBody);
    xsip_set_content_type(szAns, "Application/MANSCDP+xml");
    xsip_set_body(szAns, szBody, nLen);
    DWORD dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);

    DVRINFOMAP::iterator itDev = m_mapDevice.begin();
    for(; itDev != m_mapDevice.end(); itDev ++)
    {
        string sID = xsip_get_url_username((char*)itDev->second->pDeviceUri);
        if(sID == sDeviceID)
        {
            break;
        }
        string sIP = xsip_get_url_host((char*)itDev->second->pDeviceUri);
        int nPort = xsip_get_url_port((char*)itDev->second->pDeviceUri);
        if(sIP == sDeviceIP && nPort == nDevicePort)
        {
            sDeviceID = sID;
            break;
        }
    }

    //û���ҵ�sDeviceID, HIK�¼�ƽ̨���͵�Notify��Ϣ, DeviceIDΪ��ƽ̨�µ��豸|���ID, ������ƽ̨����ID
    if(itDev == m_mapDevice.end())
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "֪ͨ��Ϣ��Դ[%s@%s:%d]���¼�ƽ̨|�豸ƥ��, ����...", 
            sDeviceID.c_str(), sDeviceIP.c_str(), nDevicePort);
        return 0;
    }

    SaveSubscribe(addr, sBody.c_str(), sDeviceID.c_str()); //����Ŀ¼��Ϣ
  
    return 0;
}


int CSIPServer::SaveSubscribe(SOCKADDR_IN &addr, const char *xml, const char *sServerID)
{
    SaveCatalogToDB(addr, xml, sServerID);
    return 0;
}
int CSIPServer::SuperiorCatalogAnswer(SOCKADDR_IN &addr, char *msg)
{
    char szAns[DATA_BUFSIZE] = {0};
    DWORD dwLen = 0;
    string dwToTag = xsip_get_to_tag(msg);
    if (dwToTag == "")
    {
        dwToTag = xsip_get_tag();
    }
    string sTo = xsip_get_from_url(msg);
    string dwFmTag = xsip_get_from_tag(msg);
    string sFrom = xsip_get_to_url(msg);
    string sCallID = xsip_get_call_id(msg);
    int nCeq = xsip_get_cseq_number(msg);
    string sBody = xsip_get_body(msg);
    string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
    string sID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>", "</DeviceID>", MAX_USERNAME_LEN);
    if (sBody == "" || sSN == "" || sID == "")
    {
        xsip_message_build_answer(msg,400,szAns,dwToTag);
        dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ� 400
        return -1;
    }
    xsip_message_build_answer(msg,200,szAns,dwToTag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//��ȷ�ظ� 200

    /***Ȫ����Ŀ, SIP���������ݿ�, ��ͷĿ¼�������໥�䷢����ȥ, �����ﲻ�ٴ�������Ŀ¼�߳�**/
    //return 0;

    if (sID == m_sSIPServerID)	//��ͷ���ͣ����������߳�
    {
        SESSIONINFO sinfo;
        sinfo.sTitle = "CATALOGSS";
        sinfo.sAddr = xsip_get_via_addr(msg);
        sinfo.dwFTag = dwFmTag;
        sinfo.dwTTag = dwToTag;
        sinfo.request = msg;
        memcpy(&sinfo.sockAddr, &addr, sizeof(SOCKADDR_IN));
        EnterCriticalSection(&m_sLock);
        m_SessionMap.insert(SessionMap::value_type(sCallID, sinfo));
        LeaveCriticalSection(&m_sLock);
        HANDLE hThread = CreateThread(NULL, 0, SuperiorCatalogThread, this, 0, NULL);
        CloseHandle(hThread);

        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�ϼ�ƽ̨[%s]����Ŀ¼��ѯ!", sinfo.sAddr.c_str());
        return 0;
    }

    return 0;
}
DWORD WINAPI CSIPServer::SuperiorCatalogThread(LPVOID lpParam)
{
    CSIPServer *pSip = (CSIPServer *)lpParam;
    EnterCriticalSection(&pSip->m_sLock);
    SessionMap::iterator iter = pSip->m_SessionMap.begin();
    while(iter != pSip->m_SessionMap.end())
    {
        if (iter->second.sTitle == "CATALOGSS")
        {			
            string sSN = pSip->xsip_get_uri((char*)iter->second.request.c_str(),"<SN>","</SN>", 10);
            string sID = pSip->xsip_get_uri((char*)iter->second.request.c_str(),"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
            string sSD = pSip->xsip_get_url_username((char *)iter->second.sAddr.c_str());

            int nCount = 0;
            char szSql[512] = {0};
            sprintf_s(szSql, sizeof(szSql), "select count(*) as count from %s a, %s b where a.ID = b.ID and length(b.channo) < 20",
                pSip->m_sSIPChannelTable.c_str(), "xy_base.channelinfo");
            otl_stream otlSelect;
            if(pSip->m_DBMgr.ExecuteSQL(szSql, otlSelect))
            {
                if(!otlSelect.eof())
                {
                    otlSelect >> nCount;      
                }
            }

            if (nCount == 0)
            {
                LeaveCriticalSection(&pSip->m_sLock);
                return 0;
            }
            pSip->SuperiorCatalogSend(iter->second.sAddr, sSN.c_str(), sSD.c_str() ,iter->second.dwTTag,
                iter->second.dwFTag, "", nCount, pSip->m_sSIPChannelTable);
            iter = pSip->m_SessionMap.erase(iter);
        }
        else
        {
            iter++;
        }
    }
    LeaveCriticalSection(&pSip->m_sLock);
    return 0;
}


int CSIPServer::SuperiorCatalogSend(string sAddr, string sSN, string sServerID, string ttag, string ftag, 
                                    string callid, int nCount, string sTable, bool bNotify)
{
	SOCKADDR_IN sockin = {0};
	SetDestAddr(sAddr, &sockin);
	int nTime = 0;
	char szMsg[DATA_BUFSIZE] = {0};
	char szBody[MAX_CONTENT_LEN] = {0};
	char szSql[4096] = {0};

    sprintf_s(szSql, sizeof(szSql), "select a.DeviceID, a.Name, a.Manufacturer, a.Model, a.Owner, a.CivilCode, "
        "a.Block, a.Address, a.ParentID, a.Status, a.ServerID, a.ipaddress, a.port, a.longitude, a.latitude,  "
        "to_char(a.UpdateTime,\'yyyy-mm-dd hh24:mi:ss\') UpdateTime, b.isPTZ from %s a, %s b "
        "where a.ID = b.ID and length(b.channo) < 20", 
        m_sSIPChannelTable.c_str(), "xy_base.channelinfo");
	
	otl_stream otlSelect;
	if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ȡSIPChannel���ȡ��ͷĿ¼����ʧ��!");
		return -1;
	}

    //��ַ־�
    /*{
        char pNode[1024] = {0};
        sprintf_s(pNode, sizeof(pNode), 
            "<Item>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<Name>��ַ־�</Name>\r\n"
            "<Manufacturer>HJGA</Manufacturer>\r\n"
            "<Model>1.0</Model>\r\n"
            "<Owner>HJGA</Owner>\r\n"
            "<CivilCode>%s</CivilCode>\r\n"
            "<Address>HJ</Address>\r\n"
            "<RegisterWay>1</RegisterWay>\r\n"
            "<Secrecy>0</Secrecy>\r\n"
            "</Item>\r\n", 
            m_sSIPServerID.c_str(), string(m_sSIPServerID, 0, 6).c_str());

        char pXML[1024] = {0};
        sprintf_s(pXML, sizeof(pXML), 
            "<?xml version=\"1.0\"?>\r\n"
            "<Response>\r\n"
            "<CmdType>Catalog</CmdType>\r\n"
            "<SN>%s</SN>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<SumNum>%d</SumNum>\r\n"
            "<DeviceList Num=\"1\">\r\n"
            "%s"
            "</DeviceList>\r\n"
            "</Response>\r\n", 
            sSN.c_str(), m_sSIPServerID.c_str(), nCount, pNode);
        xsip_message_build_request(szMsg, "MESSAGE", XSIP::m_sFrom.c_str(), sAddr.c_str(), XSIP::m_sFrom.c_str(), NULL,21,ttag,ftag);
        xsip_set_content_type(szMsg, "Application/MANSCDP+xml");
        xsip_set_body(szMsg, pXML, strlen(pXML));
        m_pSIPProtocol->sendBuf(sockin, szMsg, strlen(szMsg));
    }*/
    
    

	while(!otlSelect.eof())
	{
		string DeviceID = "";
		string Name = "";
		string Manufacturer = "";
		string Model = "";
		string Owner = "";
		string CivilCode = "";
		string Block = "0";
		string Address = "";
		string ParentID = "";
		string Status = "";
		string ServerID = "";
        string DeviceIP = "";
        int nPort = 0;
        double longitude = 0;
        double latitude = 0;
		string UpdateTime = "";
        int  nParental = 0;
        int isPtz = 0;

		char strDeviceID[MAX_STRING_LEN] = {0};
		char strName[MAX_STRING_LEN] = {0};
		char strManufacturer[MAX_STRING_LEN] = {0};
		char strModel[MAX_STRING_LEN] = {0};
		char strOwner[MAX_STRING_LEN] = {0};
        char strCivilCode[MAX_STRING_LEN] = {0};
        char strBlock[MAX_STRING_LEN] = {0};
		char strAddress[MAX_STRING_LEN] = {0};
        char strParentID[MAX_STRING_LEN] = {0};
        char strStatus[MAX_STRING_LEN] = {0};
        char strServerID[MAX_STRING_LEN] = {0};
        char strDeviceIP[MAX_STRING_LEN] = {0};
		char strUpdateTime[MAX_STRING_LEN] = {0};

		otlSelect >> strDeviceID >> strName >> strManufacturer >> strModel >> strOwner >>
            strCivilCode >> strBlock >> strAddress >> strParentID >> strStatus >> strServerID >>
            strDeviceIP >> nPort >> longitude >> latitude >> strUpdateTime >> isPtz;      

		DeviceID		=	strDeviceID;
		Name			=	strName;
		Manufacturer	=	strManufacturer;
		Model			=	strModel;
		Owner			=   strOwner;
		CivilCode		=	strCivilCode;
        if(CivilCode == "")
        {
            CivilCode = m_sRealm;
        }
        Block           =   strBlock;
        if(Block == "")
        {
            Block = CivilCode;
        }
		Address			=	strAddress;
        ParentID        =   strParentID;
        if(ParentID == DeviceID)
        {
            ParentID = m_sSIPServerID;
        }
		Status			=	strStatus;
		ServerID		=	strServerID;
        DeviceIP        =   strDeviceIP;
		UpdateTime		=	strUpdateTime;
        if(isPtz == 0)//ǹ��
        {
            isPtz = 3;
        }

		memset(szBody, 0, sizeof(szBody));
		sprintf_s(szBody, sizeof(szBody), 
			"<?xml version=\"1.0\"?>\r\n"
			"<Response>\r\n"
			"<CmdType>Catalog</CmdType>\r\n"
			"<SN>%s</SN>\r\n"
			"<DeviceID>%s</DeviceID>\r\n"
			"<SumNum>%d</SumNum>\r\n"
			"<DeviceList Num=\"1\">\r\n"
			"<Item>\r\n"
			"<DeviceID>%s</DeviceID>\r\n"
			"<Name>%s</Name>\r\n"					//����
			"<Manufacturer>%s</Manufacturer>\r\n"	//����
			"<Model>%s</Model>\r\n"					//�ͺ�
			"<Owner>%s</Owner>\r\n"					//����
			"<CivilCode>%s</CivilCode>\r\n"			//��������(��ѡ)
			"<Block>%s</Block>\r\n"					//����(��ѡ)
			"<Address>%s</Address>\r\n"				//��װ��ַ
			"<Parental>%d</Parental>\r\n"			//�Ƿ������豸:1��0û��
			"<ParentID>%s</ParentID>\r\n"			//��ID(��ѡ)
			"<SafetyWay>0</SafetyWay>\r\n"			//���ȫģʽ(��ѡ):0������
			"<RegisterWay>1</RegisterWay>\r\n"		//ע��ģʽ:1���ϱ�׼��֤
			"<Secrecy>0</Secrecy>\r\n"				//��������:0������1����
			"<IPAddress>%s</IPAddress>\r\n"			//�豸IP��ַ(��ѡ)
			"<Port>%d</Port>\r\n"					//�豸�˿�(��ѡ)
			"<Status>%s</Status>\r\n"				//�豸״̬:ON����OFF������
			"<Longitude>%lf</Longitude>\r\n"		//����(��ѡ)
			"<Latitude>%lf</Latitude>\r\n"			//γ��(��ѡ)
            "<Info>\r\n"
            "<PTZType>%d</PTZType>\r\n"
            "</Info>\r\n"
			"</Item>\r\n"
			"</DeviceList>\r\n"
			"</Response>\r\n",
			sSN.c_str(),m_sSIPServerID.c_str(), nCount, DeviceID.c_str(), Name.c_str(),
            Manufacturer.c_str(), Model.c_str(), Owner.c_str(), CivilCode.c_str(), 
			Block.c_str(), Address.c_str(), nParental, /*ParentID.c_str()*/m_sSIPServerID.c_str(),
            DeviceIP.c_str(), nPort, Status.c_str(), longitude, latitude, isPtz);
		int nLen = (int)strlen(szBody);

		memset(szMsg, 0, sizeof(szMsg));
        if(bNotify)
        {
            xsip_subscribe_build_notify(szMsg, sAddr.c_str(), callid, 21, ttag, ftag);
        }
        else
        {
            xsip_message_build_request(szMsg, "MESSAGE", XSIP::m_sFrom.c_str(), sAddr.c_str(), XSIP::m_sFrom.c_str(), NULL,21,ttag,ftag);
        }
		xsip_set_content_type(szMsg, "Application/MANSCDP+xml");
		xsip_set_body(szMsg, szBody, nLen);
		DWORD dwLen = (DWORD)strlen(szMsg);
		szMsg[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(sockin, szMsg, dwLen);
		nTime++;
		Sleep(10);
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���;�ͷĿ¼���, ��ͷ��[%d], ������[%d].", nCount, nTime);

	return 0;
}



int CSIPServer::SuperiorSubscribeAnswer(SOCKADDR_IN &addr, char *msg)
{
    char pAnswer[DATA_BUFSIZE] = {0};
    string sFromTag = xsip_get_tag();
    string sBody = xsip_get_body(msg);

    TiXmlDocument *pMyDocument = new TiXmlDocument();
    pMyDocument->Parse(sBody.c_str());
    TiXmlElement *pRootElement = pMyDocument->RootElement();
    string sn = ReValue(pRootElement->FirstChildElement("SN"));
    string id = ReValue(pRootElement->FirstChildElement("DeviceID"));
    delete pMyDocument;

    char szBody[MAX_CONTENT_LEN] = {0};
    sprintf_s(szBody, sizeof(szBody), 
        "<?xml version=\"1.0\"?>\r\n" 
        "<Response>\r\n"
        "<CmdType>Catalog</CmdType>\r\n"
        "<SN>%s</SN>\r\n"
        "<DeviceID>%s</DeviceID>\r\n"
        "<Result>OK</Result>\r\n"
        "</Response>\r\n",
        sn.c_str(), id.c_str());
    int nLen = (int)strlen(szBody);
    szBody[nLen] = '\0';

    xsip_subscribe_build_answer(msg, 200, pAnswer, sFromTag);
    xsip_set_content_type(pAnswer, "Application/MANSCDP+xml");
    xsip_set_body(pAnswer, szBody, nLen);
    nLen = (int)strlen(pAnswer);
    pAnswer[nLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, pAnswer, (DWORD)nLen);

    /***Ȫ����Ŀ, SIP���������ݿ�, ��ͷĿ¼�������໥�䷢����ȥ, �����ﲻ�ٴ�������Ŀ¼�߳�**/
    return 0;

    SESSIONINFO Info;
    Info.sTitle = "SUBSCRIPTION";
    Info.sAddr = xsip_get_via_addr(msg);
    Info.dwFTag = sFromTag;
    Info.dwTTag = xsip_get_from_tag(msg);
    string sCallID = xsip_get_call_id(msg);

    if (FindSessionMap(sCallID))
    {
        EraseSessionMap(sCallID);
    }
    EnterCriticalSection(&m_sLock);
    m_SessionMap.insert(SessionMap::value_type(sCallID, Info));	//����Ự��Ϣ
    LeaveCriticalSection(&m_sLock);

    CreateThread(NULL, 0, SuperiorNotifyThread, this, 0, NULL);	    //�����̷߳��;�ͷĿ¼		
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�ϼ�ƽ̨[%s]����Ŀ¼����!", Info.sAddr.c_str());
    return 0;
}
DWORD WINAPI CSIPServer::SuperiorNotifyThread(LPVOID lpParam)
{
    CSIPServer *pSip = (CSIPServer *)lpParam;
    int nCount = 0;
    char szSql[512] = {0};
    sprintf_s(szSql, sizeof(szSql), "select count(*) as count from %s", pSip->m_sSIPChannelTable.c_str());
    otl_stream otlSelect;
    bool nExitSQL = pSip->m_DBMgr.ExecuteSQL(szSql, otlSelect);
    if(true == nExitSQL)
    {
        if(!otlSelect.eof())
        {
            otlSelect >> nCount;      
        }
    }

    EnterCriticalSection(&pSip->m_sLock);
    SessionMap::iterator iter = pSip->m_SessionMap.begin();
    while(iter != pSip->m_SessionMap.end())
    {
        if (iter->second.sTitle == "SUBSCRIPTION")
        {
            pSip->SuperiorNotifySend(iter->second.sAddr, iter->second.dwTTag, iter->second.dwFTag, iter->first, nCount);
            iter = pSip->m_SessionMap.erase(iter);
        }
        else
        {
            iter++;
        }
    }
    LeaveCriticalSection(&pSip->m_sLock);
    return 0;
}
int CSIPServer::SuperiorNotifySend(string sAddr, string ttag, string ftag, string callid, int nCount)
{
    SOCKADDR_IN addr = {0};
    SetDestAddr(sAddr, &addr);

    char szMsg[DATA_BUFSIZE] = {0};
    char szBody[MAX_CONTENT_LEN] = {0};
    char szSql[1024] = {0};
    sprintf_s(szSql, sizeof(szSql), "select DeviceID, Name, Manufacturer, Model, Owner, CivilCode, "
        "Address, Parental, Status, ServerID, ipaddress, port, longitude, latitude, "
        "to_char(UpdateTime,\'yyyy-mm-dd hh24:mi:ss\') UpdateTime from %s order by ServerID", m_sSIPChannelTable.c_str());
    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: ��ȡSIPChannel���ȡ��ͷĿ¼����ʧ��!");
        return -1;
    }

    while(!otlSelect.eof())
    {
        string DeviceID = "";
        string Name = "";
        string Manufacturer = "";
        string Model = "";
        string Owner = "";
        string CivilCode = "";
        string Block = "0";
        string Address = "";
        string ParentID = "";
        string Status = "";
        string ServerID = "";
        string DeviceIP = "";
        int nPort = 0;
        double longitude = 0;
        double latitude = 0;
        string UpdateTime = "";

        char strDeviceID[MAX_STRING_LEN] = {0};
        char strName[MAX_STRING_LEN] = {0};
        char strManufacturer[MAX_STRING_LEN] = {0};
        char strModel[MAX_STRING_LEN] = {0};
        char strOwner[MAX_STRING_LEN] = {0};
        char strCivilCode[MAX_STRING_LEN] = {0};
        char strAddress[MAX_STRING_LEN] = {0};
        int  nParental = 0;
        char strStatus[MAX_STRING_LEN] = {0};
        char strServerID[MAX_STRING_LEN] = {0};
        char strDeviceIP[MAX_STRING_LEN] = {0};
        char strUpdateTime[MAX_STRING_LEN] = {0};

        otlSelect >> strDeviceID >> strName >> strManufacturer >> strModel >> strOwner >>
            strCivilCode >> strAddress >> nParental >> strStatus >> strServerID >>
            strDeviceIP >> nPort >> longitude >> latitude >> strUpdateTime;      

        DeviceID		=	strDeviceID;
        Name			=	strName;
        Manufacturer	=	strManufacturer;
        Model			=	strModel;
        Owner			=   strOwner;
        CivilCode		=	strCivilCode;
        if(CivilCode == "")
        {
            CivilCode.assign(m_sSIPServerID, 0, 10);
        }
        Address			=	strAddress;
        Status			=	strStatus;
        ServerID		=	strServerID;
        DeviceIP        =   strDeviceIP;
        UpdateTime		=	strUpdateTime;

        memset(szBody, 0, sizeof(szBody));
        sprintf_s(szBody, sizeof(szBody), 
            "<?xml version=\"1.0\"?>\r\n"
            "<Response>\r\n"
            "<CmdType>Catalog</CmdType>\r\n"
            "<SN>%d</SN>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<SumNum>%d</SumNum>\r\n"
            "<DeviceList Num=\"1\">\r\n"
            "<Item>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<Name>%s</Name>\r\n"					//����
            "<Manufacturer>%s</Manufacturer>\r\n"	//����
            "<Model>%s</Model>\r\n"					//�ͺ�
            "<Owner>%s</Owner>\r\n"					//����
            "<CivilCode>%s</CivilCode>\r\n"			//��������(��ѡ)
            "<Block>%s</Block>\r\n"					//����(��ѡ)
            "<Address>%s</Address>\r\n"				//��װ��ַ
            "<Parental>%d</Parental>\r\n"			//�Ƿ������豸:1��0û��
            "<ParentID>%s</ParentID>\r\n"			//��ID(��ѡ)
            "<SafetyWay>0</SafetyWay>\r\n"			//���ȫģʽ(��ѡ):0������
            "<RegisterWay>1</RegisterWay>\r\n"		//ע��ģʽ:1���ϱ�׼��֤
            "<Secrecy>0</Secrecy>\r\n"				//��������:0������1����
            "<IPAddress>%s</IPAddress>\r\n"			//�豸IP��ַ(��ѡ)
            "<Port>%d</Port>\r\n"					//�豸�˿�(��ѡ)
            "<Status>%s</Status>\r\n"				//�豸״̬:ON����OFF������
            "<Longitude>%lf</Longitude>\r\n"		//����(��ѡ)
            "<Latitude>%lf</Latitude>\r\n"			//γ��(��ѡ)
            "</Item>\r\n"
            "</DeviceList>\r\n"
            "</Response>\r\n",
            m_dwSn++, XSIP::m_sUsername.c_str(), nCount, DeviceID.c_str(), Name.c_str(),
            Manufacturer.c_str(), Model.c_str(), Owner.c_str(), CivilCode.c_str(), 
            Block.c_str(), Address.c_str(), nParental, ServerID.c_str(),
            DeviceIP.c_str(), nPort, Status.c_str(), longitude, latitude);
        int nLen = (int)strlen(szBody);

        memset(szMsg, 0, sizeof(szMsg));
        xsip_subscribe_build_notify(szMsg,sAddr.c_str(),callid,21,ttag,ftag);
        xsip_set_subscriptoin_state(szMsg);
        xsip_set_event(szMsg, "presence");
        xsip_set_content_type(szMsg, "Application/MANSCDP+xml");
        xsip_set_body(szMsg, szBody, nLen);
        DWORD dwLen = (DWORD)strlen(szMsg);
        szMsg[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);
    }

    return 0;
}


int CSIPServer::InviteMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    char szCallAns[DATA_BUFSIZE] = {0};
    DWORD dwLen = 0;
    xsip_message_build_answer(msg, 100, szCallAns);
    dwLen = (DWORD)strlen(szCallAns);
    szCallAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);							//�ظ�100 Tyring

    string dwToTag = xsip_get_tag();
	string sCameraID = "";
	string dwFromTag = xsip_get_from_tag(msg);
	string sCallID = xsip_get_call_id(msg);
	string sToUri = xsip_get_to_url(msg);
	string sBody = xsip_get_body(msg);
	INVITE_TYPE nType = xsip_get_sdp_s_type(sBody.c_str());
    string subject = xsip_get_subject(msg);
    string sMediaRecvID = xsip_get_subject_recvid((char *)subject.c_str());
    SOCKADDR_IN sockin;

    //�ж��Ƿ��ظ��Ự.
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if (rep != REPNONE)
    {								//��ͬ�ĻỰ��ʶ;
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Repeat: �յ��ظ���Invite����, CallID[%s].", sCallID.c_str());
       
        if (PlayInfo.sInvite1Answer != "")
        {   //����Ự�����, ����ܻ�Ӧ��200 OK��ʧ, �ظ�����
            SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);
            m_pSIPProtocol->sendBuf(sockin, (char*)PlayInfo.sInvite1Answer.c_str(), PlayInfo.sInvite1Answer.size());
        }
        else
        {
            //����Ựδ���, ���Ӧ100 Trying, ͬʱ�����ȴ���Ӧ.
            xsip_message_build_answer(msg, 100, szCallAns);
            dwLen = (DWORD)strlen(szCallAns);
            szCallAns[dwLen] = '\0';
            m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);
        }
        return 0;
    }

    printf("\n----------------------------------------------------------------\n");
	switch (nType)
	{
	case XSIP_INVITE_PLAY:
		{
			sCameraID = xsip_get_url_username((char *)sToUri.c_str());	//��þ�ͷID			
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: �յ�InviteʵʱԤ������, ��ͷID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());

			break;
		}
	case XSIP_INVITE_PLAYBACK:
		{
			string sUID = xsip_get_sdp_u_url((char *)sBody.c_str());
			if (sUID == "")
			{
				sCameraID =  xsip_get_url_username((char *)sToUri.c_str());
			}
			else
			{
				sCameraID = sUID.substr(0,20);
			}
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: �յ�Invite����¼������, ��ͷID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());
			break;
		}
	case XSIP_INVITE_DOWNLOAD:
		{
			string sUID = xsip_get_sdp_u_url((char *)sBody.c_str());
			if (sUID == "")
			{
				sCameraID = xsip_get_url_username((char *)sToUri.c_str());
			}
			else
			{
				sCameraID = sUID.substr(0,20);
			}
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: �յ�Invite����¼������, ��ͷID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());
			break;
		}
	default:
		{
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: Invite����SDP��������!");
            return -1;
		}
	}
	
    CAMERAINFO CameraInfo;
    CameraInfo.nCameraType = 20;
	string sCameraUri = GetDeviceAddr(sCameraID.c_str(), nType, &CameraInfo);	//�жϾ�ͷ�Ƿ����������
	if (sCameraUri == "")
	{
		memset(szCallAns, 0, DATA_BUFSIZE);
		xsip_call_build_answer(msg, 400, szCallAns, dwToTag);
		dwLen = (DWORD)strlen(szCallAns);
		szCallAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);								//��������ظ� 400 Bad
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ���ž�ͷ[%s]ʧ��!", sCameraID.c_str());
        return 0;
	}

	string sDecodeUri = "";
	if (sMediaRecvID != "" && IsMonitor(sMediaRecvID))	//�жϽ���ͨ���Ƿ����������
	{
		sDecodeUri = GetDeviceAddr(sMediaRecvID.c_str());
		if (sDecodeUri == "")
		{
			memset(szCallAns, 0, DATA_BUFSIZE);
			xsip_call_build_answer(msg, 400, szCallAns, dwToTag);
			dwLen = (DWORD)strlen(szCallAns);
			szCallAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);								//��������ظ� 400 Bad
			g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ������ͨ��[%s]��ǽʧ��.", sMediaRecvID.c_str());
            return 0;
		}
	}
		
    string sRTPServerID = "", sRTPServerIP = "";
    int nRTPServerPort;
    bool bTransmit = false;  //ʵʱ����ʱ�Ƿ����ֱ��ת��
    if (!m_pRTPManager->GetPracticableRTPServer(msg, sCameraID, nType, sCallID, sRTPServerID, 
        sRTPServerIP, nRTPServerPort, bTransmit))
    {
        g_LogRecorder.WriteErrorLog(__FUNCTION__, "����RTP���񶼲�����!");
        return 0;
    }  
    string sRTPUri = xsip_to_init(sRTPServerID.c_str(), sRTPServerIP.c_str(), nRTPServerPort);
	SetDestAddr(sRTPUri, &sockin);

    string sSSRC = xsip_ssrc_init(nType, m_dwSSRC++); 
    if(m_dwSSRC >= 10000)
    {
        m_dwSSRC = 1;
    }
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "����ssrc[%s], CallID[%s].", sSSRC.c_str(), sCallID.c_str());

    char pSubject[128] = {0};
    sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%d", sCameraID.c_str(), 
        sSSRC.c_str(), sMediaRecvID.c_str(), CameraInfo.nNetType);
    
    string sCallID2 = "", sCallID4 = "";
    char sInviteMsg2[DATA_BUFSIZE] = {0};
    char sInviteMsg4[DATA_BUFSIZE] = {0};
	if (bTransmit)  //����ý���Ѿ�ת��
	{
		if (sBody == "")
		{
			memset(szCallAns, 0, DATA_BUFSIZE);
			xsip_call_build_answer(msg, 488, szCallAns, dwToTag);
			dwLen = (DWORD)strlen(szCallAns);
			szCallAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);					//û��SDP��Ϣ��ظ�488 BAD
			EraseSessionMap(sCallID);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: Inviteû��SDP��Ϣ!");
            return 0;
		}
		memset(sInviteMsg4, 0, sizeof(sInviteMsg4));
        sCallID4 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
        xsip_call_build_invite(sInviteMsg4, sRTPUri.c_str(), pSubject,NULL, dwFromTag, sCallID4);		     

		sBody = xsip_set_sdp_y(sBody.c_str(), sSSRC.c_str());

        if(CameraInfo.nNetType == 4)
        {
            char pCameraInfo[128] = {0};
            sprintf_s(pCameraInfo, sizeof(pCameraInfo), "%s:%s:%d:%s:%s:%d", CameraInfo.sDVRKey.c_str(), 
                CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort, CameraInfo.sDeviceUsername.c_str(), 
                CameraInfo.sDevicePassword.c_str(), CameraInfo.nChannel);
            sBody = xsip_set_sdp_u(sBody.c_str(), pCameraInfo);
        }
        
		xsip_set_content_type(sInviteMsg4, XSIP_APPLICATION_SDP);
		xsip_set_body(sInviteMsg4, (char *)sBody.c_str(), (int)sBody.size());
		dwLen = (DWORD)strlen(sInviteMsg4);
		sInviteMsg4[dwLen] = '\0';

        string sMediaRecvUri = xsip_get_contact_url(msg);
        m_pRTPManager->PushRTPInfo(sRTPServerID, sCameraID, sCameraUri, CameraInfo.nCameraType, sSSRC, sMediaRecvID, sMediaRecvUri, 
            sCallID, sCallID2, sCallID4, msg, sInviteMsg2, sInviteMsg4);

		m_pSIPProtocol->sendBuf(sockin, sInviteMsg4, dwLen);				//��ý��ת��(��SDP)
 
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: ��RTP��ý��������Ƶ��, CallID[%s].", sCallID4.c_str());
	}
	else
	{
        if(CameraInfo.nNetType >= 4)
        {
            int nLoad;
            //�ͻ���SDP��Ϣ
            int nNetType = 1;
            if(sBody.find("TCP/RTP/AVP") != string::npos)
            {
                nNetType = 2;
            }
            
            string sUsername = xsip_get_sdp_o_id(sBody.c_str());
            string sClientIP = xsip_get_sdp_o_ip(sBody.c_str());
            int nPort = xsip_get_sdp_m_port(sBody.c_str());
            string sTime = xsip_get_sdp_t_time(sBody.c_str());

            char szSdp[MAX_CONTENT_LEN] = {0};
            xsip_sdp_build_initial(szSdp, (char *)sUsername.c_str(), sClientIP.c_str(), nPort, nType,
                XSIP_RECVONLY, NULL, NULL, (char*)sSSRC.c_str(), (char *)sTime.c_str(), NULL, nNetType);
            string sSdp = szSdp;

            char pCameraInfo[128] = {0};
            sprintf_s(pCameraInfo, sizeof(pCameraInfo), "%s:%s:%d:%s:%s:%d:%d:%s:%d:%d", CameraInfo.sDVRKey.c_str(), 
                CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort, CameraInfo.sDeviceUsername.c_str(), 
                CameraInfo.sDevicePassword.c_str(), CameraInfo.nChannel, CameraInfo.nTranscoding, CameraInfo.sCameraID.c_str(), 
                CameraInfo.nChanID, CameraInfo.nNetType);
            sSdp = xsip_set_sdp_u(sSdp.c_str(), pCameraInfo);

            string sCallID4 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
            xsip_call_build_invite(sInviteMsg4, sRTPUri.c_str(), pSubject, NULL, "", sCallID4);
            xsip_set_content_type(sInviteMsg4, XSIP_APPLICATION_SDP);
            xsip_set_body(sInviteMsg4, (char *)sSdp.c_str(), (int)sSdp.size());

            string sMediaRecvUri = xsip_get_contact_url(msg);
            m_pRTPManager->PushRTPInfo(sRTPServerID, sCameraID, sCameraUri, CameraInfo.nCameraType, sSSRC, sMediaRecvID, sMediaRecvUri, 
                sCallID, sCallID2, sCallID4, msg, sInviteMsg2, sInviteMsg4);
            m_pSIPProtocol->sendBuf(sockin, sInviteMsg4, (DWORD)strlen(sInviteMsg4));	

            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: ��RTP��ý��[%s]����Invite����������, CallID[%s].", 
                sRTPServerID.c_str(), sCallID4.c_str());
        }
        else
        {
            sCallID2 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
		    xsip_call_build_invite(sInviteMsg2, sRTPUri.c_str(), pSubject, NULL, dwFromTag, sCallID2);
		    dwLen = (DWORD)strlen(sInviteMsg2);
		    sInviteMsg2[dwLen] = '\0';

            string sMediaRecvUri = xsip_get_contact_url(msg);
            m_pRTPManager->PushRTPInfo(sRTPServerID, sCameraID, sCameraUri, CameraInfo.nCameraType, sSSRC, sMediaRecvID, sMediaRecvUri, 
                sCallID, sCallID2, sCallID4, msg, sInviteMsg2, sInviteMsg4);

		    m_pSIPProtocol->sendBuf(sockin, sInviteMsg2, dwLen);			//��ý�巢������(��SDP)
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "2: ��RTP��ý��[%s]�����������ַ, CallID[%s].",
                sRTPUri.c_str(), sCallID2.c_str());
        }
        
	}

    
    return 0;
}

int CSIPServer::InviteAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
    SOCKADDR_IN sockin = {0};
	string sSdp = xsip_get_body(msg);
	string sCallid = xsip_get_call_id(msg);

    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallid, &PlayInfo);
    string sInvite1Body = xsip_get_body((char*)PlayInfo.sInviteMsg1.c_str());
    switch(rep)
    {
    case REPRTPRECVSTREAM:    //RTP��ý���������Ӧ
        {
            if(PlayInfo.sInvite2Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ��ظ�Invite2 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushRTPRecvAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "3: �յ�RTP��ý�������������ַ��Ӧ, CallID[%s].", sCallid.c_str());

            bool bUser = IsUser(PlayInfo.sMediaRecverID);
            char pSubject[128] = {0};
            sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%s", PlayInfo.sCameraID.c_str(), 
                PlayInfo.sSSRC.c_str(), PlayInfo.sRTPServerID.c_str(), "0");
            char szInvite[DATA_BUFSIZE] = {0};
            string sCallID3 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
            xsip_call_build_invite(szInvite, PlayInfo.sCameraUri.c_str(), pSubject, NULL, "", sCallID3);

            if (PlayInfo.InviteType == XSIP_INVITE_PLAY)
            {
                sSdp = xsip_set_sdp_s(sSdp.c_str(), "Play");
            }
            else if (PlayInfo.InviteType == XSIP_INVITE_PLAYBACK)
            {
                string sTime = xsip_get_sdp_t_time(sInvite1Body.c_str());
                sSdp = xsip_set_sdp_t(sSdp.c_str(), sTime.c_str());
                sSdp = xsip_set_sdp_s(sSdp.c_str(), "Playback");
                char szUri[128] = {0};
                sprintf_s(szUri, sizeof(szUri), "%s:3", PlayInfo.sCameraID.c_str());
                sSdp = xsip_set_sdp_u(sSdp.c_str(), szUri);
            }
            else if (PlayInfo.InviteType == XSIP_INVITE_DOWNLOAD)
            {
                string sTime = xsip_get_sdp_t_time(sInvite1Body.c_str());
                sSdp = xsip_set_sdp_t(sSdp.c_str(), sTime.c_str());
                sSdp = xsip_set_sdp_s(sSdp.c_str(), "Download");
                sSdp = xsip_set_sdp_downloadspeed(sSdp.c_str(), "4");
                char szUri[128] = {0};
                sprintf_s(szUri, sizeof(szUri), "%s:3", PlayInfo.sCameraID.c_str());
                sSdp = xsip_set_sdp_u(sSdp.c_str(), szUri);
            }
            
            sSdp = xsip_set_sdp_y(sSdp.c_str(), PlayInfo.sSSRC.c_str());

            xsip_set_content_type(szInvite, XSIP_APPLICATION_SDP);
            xsip_set_body(szInvite, (char *)sSdp.c_str(), (int)sSdp.size());
            DWORD dwLen = (DWORD)strlen(szInvite);
            szInvite[dwLen] = '\0';

            m_pRTPManager->PushCameraInfo(PlayInfo.sCameraID, sCallid, sCallID3, szInvite);

            SetDestAddr(PlayInfo.sCameraUri, &sockin);
            m_pSIPProtocol->sendBuf(sockin, szInvite, dwLen);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "4: ���豸[%s]���͵㲥����, CallID[%s], ssrc[%s].",
                PlayInfo.sCameraUri.c_str(), sCallID3.c_str(), PlayInfo.sSSRC.c_str());

        }
        break;
    case REPCAMERASEND:      //��ͷ��������Ӧ
        {
            if(PlayInfo.sInvite3Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ��ظ�Invite3 Answer, CallID[%s].", sCallid.c_str());

                char szAck[DATA_BUFSIZE] = {0};
                xsip_call_build_ack(msg, szAck);
                DWORD dwLen = (DWORD)strlen(szAck);
                szAck[dwLen] = '\0';
                m_pSIPProtocol->sendBuf(addr, szAck, dwLen);	//�ظ���ͷACK
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "7: ��ͷ[%s]����ACK, CallID[%s].", 
                    PlayInfo.sCameraUri.c_str(), PlayInfo.sCallID3.c_str());
                return 0;
            }
            string sSSRC = xsip_get_sdp_y_ssrc(sSdp.c_str());
            m_pRTPManager->PushCameraAnswer(sCallid, msg, sSSRC);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "5: �յ�����ͷ��������Ӧ, CallID[%s].", sCallid.c_str());

            char szAck[DATA_BUFSIZE] = {0};
            xsip_call_build_ack(msg, szAck);
            DWORD dwLen = (DWORD)strlen(szAck);
            szAck[dwLen] = '\0';
            m_pSIPProtocol->sendBuf(addr, szAck, dwLen);	//�ظ���ͷACK
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "7: ��ͷ[%s]����ACK, CallID[%s].", 
                PlayInfo.sCameraUri.c_str(), PlayInfo.sCallID3.c_str());
            Sleep(100);

            memset(szAck, 0, DATA_BUFSIZE);
            xsip_call_build_ack((char*)PlayInfo.sInvite2Answer.c_str(), szAck);
            xsip_set_content_type(szAck, XSIP_APPLICATION_SDP);
            xsip_set_body(szAck, (char *)sSdp.c_str(), (int)sSdp.size());
            dwLen = (DWORD)strlen(szAck);
            szAck[dwLen] = '\0';
            string sRTPUri = xsip_to_init(PlayInfo.sRTPServerID.c_str(), PlayInfo.sRTPServerIP.c_str(), PlayInfo.nRTPServerPort);
            SOCKADDR_IN sockin;           
            SetDestAddr(sRTPUri, &sockin);
            m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);	//�ظ�RTP��ý��ACK
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "6: ��RTP��ý��[%s]����ACK, CallID[%s].",
                PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID2.c_str());
            Sleep(50);
            

            if (PlayInfo.sMediaRecverID != "" && IsMonitor(PlayInfo.sMediaRecverID))						//�����������Invite����
            {
                char szInvite[DATA_BUFSIZE] = {0};
                string sCallID5 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
                char pSubject[128] = {0};
                sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%s", PlayInfo.sCameraID.c_str(), 
                    PlayInfo.sSSRC.c_str(), PlayInfo.sRTPServerID.c_str(), "0");
                xsip_call_build_invite(szInvite, PlayInfo.sMediaRecvUri.c_str(), pSubject, NULL, "", sCallID5);
                DWORD dwLen = (DWORD)strlen(szInvite);
                szInvite[dwLen] = '\0';

                m_pRTPManager->PushDecodeInfo(PlayInfo.sMediaRecverID, sCallID5, szInvite);
                SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);
                m_pSIPProtocol->sendBuf(sockin, szInvite, dwLen);

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�������ͨ��[%s]����Invite5����, CallID[%s].", 
                   PlayInfo.sMediaRecvUri.c_str(), sCallID5.c_str()) ;
            }
            else
            {
                char szInvite[DATA_BUFSIZE] = {0};							//��RTP��ý�巢��Invite����������
                char szMamu[32] = {0};
                int nLoad;
                xsip_get_sdp_a_load(sSdp.c_str(), &nLoad, szMamu);
                //�ͻ���SDP��Ϣ
                string sBody = xsip_get_body((char*)PlayInfo.sInviteMsg1.c_str());
                int nNetType = 1;
                if(sBody.find("TCP/RTP/AVP") != string::npos)
                {
                    nNetType = 2;
                }
                string sUsername = xsip_get_sdp_o_id(sBody.c_str());
                string sClientIP = xsip_get_sdp_o_ip(sBody.c_str());
                int nPort = xsip_get_sdp_m_port(sBody.c_str());
                string sTime = xsip_get_sdp_t_time(sBody.c_str());

                char szSdp[MAX_CONTENT_LEN] = {0};
                xsip_sdp_build_initial(szSdp, (char *)sUsername.c_str(), sClientIP.c_str(), nPort, PlayInfo.InviteType,
                    XSIP_RECVONLY, szMamu, NULL, (char*)sSSRC.c_str(), (char *)sTime.c_str(), NULL, nNetType);
                sSdp = szSdp;


                string sYvalue = xsip_get_sdp_y_ssrc(sSdp.c_str());
                char pSubject[128] = {0};
                sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%s", PlayInfo.sCameraID.c_str(), 
                    sYvalue.c_str(), PlayInfo.sRTPServerID.c_str(), sYvalue.c_str());
                string sCallID4 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
                string sRTPUri = xsip_to_init(PlayInfo.sRTPServerID.c_str(), PlayInfo.sRTPServerIP.c_str(), PlayInfo.nRTPServerPort);
                xsip_call_build_invite(szInvite, sRTPUri.c_str(), pSubject, NULL, "", sCallID4);
                xsip_set_content_type(szInvite, XSIP_APPLICATION_SDP);
                xsip_set_body(szInvite, (char *)sSdp.c_str(), (int)sSdp.size());

                m_pRTPManager->PushRTPSendInfo(PlayInfo.sCallID1, sCallID4, szInvite);
                m_pSIPProtocol->sendBuf(sockin, szInvite, (DWORD)strlen(szInvite));	

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: ��RTP��ý��[%s]����Invite����������, CallID[%s].", 
                    PlayInfo.sRTPServerID.c_str(), sCallID4.c_str());
            }
        }
        
        break;
    case REPRTPSENDSTREAM:      //RTP��ý�巢������Ӧ
        {
            if(PlayInfo.sInvite4Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ��ظ�Invite4 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushRTPSendAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "9: �յ�RTP��ý�巢������Ӧ, CallID[%s].", sCallid.c_str());

            if (PlayInfo.sMediaRecverID != "" && IsMonitor(PlayInfo.sMediaRecverID))  //�������ͨ��
            {
                string sTo = GetDeviceAddr(PlayInfo.sMediaRecverID.c_str());
                char szAck[DATA_BUFSIZE] = {0};
                xsip_call_build_ack((char*)PlayInfo.sInvite5Answer.c_str(), szAck);
                xsip_set_content_type(szAck, XSIP_APPLICATION_SDP);
                xsip_set_body(szAck, (char *)sSdp.c_str(), (int)sSdp.size());
                DWORD dwLen = (DWORD)strlen(szAck);
                szAck[dwLen] = '\0';
                SetDestAddr(sTo, &sockin);
                m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);		//�������ͨ������ack		
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "CallID[%s]�������ͨ������ack!", PlayInfo.sCallID5.c_str());

                char szAckRTP[DATA_BUFSIZE] = {0};
                xsip_call_build_ack((char*)PlayInfo.sInvite4Answer.c_str(), szAckRTP);
                m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);		//��RTP�����ͷ�����ack	
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "CallID[%s] ��RTP�����ͷ�����ack!", PlayInfo.sCallID4.c_str());
            }
            else
            {
                char szAnwser[DATA_BUFSIZE] = {0};
                xsip_call_build_answer((char*)PlayInfo.sInviteMsg1.c_str(), 200, szAnwser);
                xsip_set_content_type(szAnwser, XSIP_APPLICATION_SDP);
                xsip_set_body(szAnwser, (char *)sSdp.c_str(), (int)sSdp.size());
                DWORD dwLen = (DWORD)strlen(szAnwser);
                szAnwser[dwLen] = '\0';
                SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);

                //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "%s\r\n%s", sSdp.c_str(), szAnwser);
                m_pRTPManager->PushClientAnswer(PlayInfo.sCallID1, szAnwser);
                m_pSIPProtocol->sendBuf(sockin, szAnwser, dwLen);

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "10: ��Ӧ�ͻ��˲�������, CallID[%s].", PlayInfo.sCallID1.c_str());
            }   
        }
        break;
    case REPDECODERRECV:      //������ͨ����������Ӧ
        {
            if(PlayInfo.sInvite5Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ��ظ�Invite5 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushDecodeAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Decoder: CallID[%s]�յ�������ͨ����������Ӧ", PlayInfo.sCallID1.c_str());

            char szInvite[DATA_BUFSIZE] = {0};		
            char pSubject[128] = {0};
            sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%s", PlayInfo.sCameraID.c_str(), 
                PlayInfo.sSSRC.c_str(), PlayInfo.sRTPServerID.c_str(), PlayInfo.sSSRC.c_str());
            string sCallID4 = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
            string sRTPUri = xsip_to_init(PlayInfo.sRTPServerID.c_str(), PlayInfo.sRTPServerIP.c_str(), PlayInfo.nRTPServerPort);
            xsip_call_build_invite(szInvite, sRTPUri.c_str(), pSubject, NULL, "", sCallID4);
            xsip_set_content_type(szInvite, XSIP_APPLICATION_SDP);
            string strSdp = xsip_set_sdp_y(sSdp.c_str(), PlayInfo.sSSRC.c_str());
            xsip_set_body(szInvite, (char *)strSdp.c_str(), (int)strSdp.size());
            DWORD dwLen = (DWORD)strlen(szInvite);
            szInvite[dwLen] = '\0';
            SetDestAddr(sRTPUri, &sockin);

            m_pRTPManager->PushRTPSendInfo(PlayInfo.sCallID1, sCallID4, szInvite);
            m_pSIPProtocol->sendBuf(sockin, szInvite, dwLen);		//�򱾵�ý������ٴ�Invite����

            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: CallID[%s]��RTP��ý��[%s]����Invite����������.", 
                sCallID4.c_str(), sRTPUri.c_str());
        }
        break;
    default:
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: Invite��Ӧ��Ϣ�޼�¼. CallID[%s].", sCallid.c_str());
        break;
    }
    return 0;
}

int CSIPServer::InviteAckRecv(char *msg)
{
    string sCallID = xsip_get_call_id(msg);
    SOCKADDR_IN sockin;

    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if(rep == REPCLIENT)
    {
        m_pRTPManager->SetRecvACK(sCallID);

        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "11���յ��ͻ���ACK��Ϣ, CallID[%s].", sCallID.c_str());
        SOCKADDR_IN sockin = {0};
        string sRTPUri = xsip_to_init(PlayInfo.sRTPServerID.c_str(), PlayInfo.sRTPServerIP.c_str(), PlayInfo.nRTPServerPort);
        SetDestAddr(sRTPUri, &sockin);

        char szAck[DATA_BUFSIZE] = {0};
        xsip_call_build_ack((char*)PlayInfo.sInvite4Answer.c_str(), szAck);
        DWORD dwLen = (DWORD)strlen(szAck);
        szAck[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);	//ת��ACK������RTP��ý��
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "12: ��RTP��ý�巢��ACK��������Ϣ, CallID[%s].", PlayInfo.sCallID4.c_str());
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "*Warning: δ֪ACK��Ϣ, CallID[%s].", sCallID.c_str());
    }
	
	return 0;
}
int CSIPServer::InviteFailure(SOCKADDR_IN &addr, char *msg)
{
    string sCallid = xsip_get_call_id(msg);
    string sAnswer((string)msg, 8, 3);
    SOCKADDR_IN sockin;
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallid, &PlayInfo);
    if(rep == REPRTPRECVSTREAM)
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n****Error: �㲥��ͷ[%s]ʧ��, RTP��ý���Ӧ[%s], CallID[%s].",
            PlayInfo.sCameraID.c_str(), sAnswer.c_str(), sCallid.c_str());
    }
    else if(rep == REPCAMERASEND)
    {       
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n****Error: �㲥��ͷ[%s]ʧ��, �豸��Ӧ[%s], CallID[%s].",
            PlayInfo.sCameraID.c_str(), sAnswer.c_str(), sCallid.c_str());
        PlayInfo.sInvite3Answer = msg;
    }

    //��ͻ��˻��ϼ�ƽ̨��Ӧ�㲥ʧ��
    memset(&sockin, 0, sizeof(sockin));
    char szCallAns[1024] = {0};
    int nAnswer = atoi(sAnswer.c_str());
    xsip_call_build_answer((char*)PlayInfo.sInviteMsg1.c_str(), nAnswer, szCallAns);   
    SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);
    m_pSIPProtocol->sendBuf(sockin, szCallAns, (DWORD)strlen(szCallAns));	
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Invite Failed: ��ͻ��˷��Ͳ���ʧ����Ϣ, CallID[%s]!", PlayInfo.sCallID1.c_str());

    m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1, "", true);
    SendByeMsg(PlayInfo);

    
    return 0;
}

int CSIPServer::CancelMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	char szAns[DATA_BUFSIZE] = {0};
	string sCallID = xsip_get_call_id(msg);
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if(rep == REPCLIENT)
    {
        m_pRTPManager->ReduceRTPTotal(sCallID);

        char szBye[DATA_BUFSIZE] = {0};
        SOCKADDR_IN sockin;
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "**�յ��ͻ���Cancel��Ϣ, CallID[%s] !", sCallID.c_str());

        SendByeMsg(PlayInfo);
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Warning: �յ���ƥ��Cancel��Ϣ, CallID[%s].", sCallID.c_str());
        char szBye[1024] = {0};
        xsip_bye_build_answer(msg, 201, szBye);
        m_pSIPProtocol->sendBuf(addr, szBye, strlen(szBye)); 

    }

    return 0;
}

int CSIPServer::ByeMsgRecv(SOCKADDR_IN &addr, char *msg)
{

    char szAns[DATA_BUFSIZE] = {0};
    string sCallID = xsip_get_call_id(msg);
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    PlayInfo.rep = rep;
    if(rep == REPCLIENT || rep == REPRTPSENDSTREAM || rep == REPRTPRECVSTREAM || rep == REPCAMERASEND)
    {       
        SOCKADDR_IN sockin;
        if(rep == REPCLIENT)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "13: �յ��ͻ���Bye��Ϣ, CallID[%s].", sCallID.c_str());
            if(!m_pRTPManager->GetRecvACK(sCallID))
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning: δ�յ��ͻ���ACK����, CallID[%s].", sCallID.c_str());
            }
        }
        else if(rep == REPRTPSENDSTREAM)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: �յ�RTP��ý��Bye��Ϣ_4, CallID[%s].", sCallID.c_str());
        }
        else if(rep == REPRTPRECVSTREAM)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: �յ�RTP��ý��Bye��Ϣ_2, CallID[%s].", sCallID.c_str());
        }
        else if(rep == REPCAMERASEND)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: �յ��豸|ƽ̨Bye��Ϣ, CallID[%s].", sCallID.c_str());
        }

        bool bDeleteCam = (rep == REPRTPRECVSTREAM) ? true : false;
        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1, PlayInfo.sCallID4, bDeleteCam);

        char szBye[1024] = {0};
        xsip_bye_build_answer(msg, 200, szBye);
        m_pSIPProtocol->sendBuf(addr, szBye, strlen(szBye));        //��Ӧ�ͻ���bye��Ϣ.

        SendByeMsg(PlayInfo, bDeleteCam);
        
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: �յ���ƥ��Bye��Ϣ, CallID[%s].", sCallID.c_str());
        char szBye[1024] = {0};
        xsip_bye_build_answer(msg, 200, szBye);
        m_pSIPProtocol->sendBuf(addr, szBye, strlen(szBye)); 
    }
    

    return 0;
}
int CSIPServer::SendByeMsg(PLAYINFO PlayInfo, bool deleteCam)
{
    char szBye[DATA_BUFSIZE] = {0};
    SOCKADDR_IN sockin;
    if(PlayInfo.nTranmit == 1 || deleteCam)
    {
        if(PlayInfo.sInviteMsg2 != "")
        {
            xsip_call_build_bye((char*)PlayInfo.sInviteMsg2.c_str(), szBye);
            SetDestAddr(PlayInfo.sRTPUri.c_str(), &sockin);
            m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));	//��RTP��ý�巢�ͽ�����BYE
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "a: ��RTP��ý��[%s]����Bye_2, CallID[%s]", PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID2.c_str());
        }
        if(PlayInfo.sInviteMsg3 != "")
        {
            if(PlayInfo.sInviteMsg4 != "") //���Ѿ���RTP��ý�巢�͹�Invite4, ˵���뾵ͷ�Ự�����, ��Ҫ����Bye������Cancel
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInvite3Answer.c_str(), szBye, false, PlayInfo.nCamCseq);
                
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//��ͷ����BYE
            }
            else if(PlayInfo.sInvite3Answer != "")
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInvite3Answer.c_str(), szBye, false, PlayInfo.nCamCseq);
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//��ͷ����Cancel
            }
            else
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInviteMsg3.c_str(), szBye, true, PlayInfo.nCamCseq);
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//��ͷ����Cancel
            }
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "b: ��ͷ[%s]����Bye, CallID[%s].",PlayInfo.sCameraID.c_str(), PlayInfo.sCallID3.c_str());
        }

    }

    if(PlayInfo.sInviteMsg4 != "")
    {
        ZeroMemory(szBye, sizeof(szBye));
        xsip_call_build_bye((char*)PlayInfo.sInviteMsg4.c_str(), szBye);
        SetDestAddr(PlayInfo.sRTPUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));		//��RTP��ý�巢�ͷ�����BYE
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "c: ��RTP��ý��[%s]����Bye_4, CallID[%s].", PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID4.c_str());
    }
    if(PlayInfo.sInviteMsg5 != "")
    {
        ZeroMemory(szBye, sizeof(szBye));
        xsip_call_build_bye((char*)PlayInfo.sInviteMsg5.c_str(), szBye);
        SetDestAddr(PlayInfo.sMediaRecvUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));		
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�������ͨ��[%s]����Bye, CallID[%s].", PlayInfo.sMediaRecvUri.c_str(), PlayInfo.sCallID5.c_str());
    }

    if(PlayInfo.rep != REPCLIENT)
    {
        ZeroMemory(szBye, sizeof(szBye));
        string sMsg = PlayInfo.sInvite1Answer == "" ? PlayInfo.sInviteMsg1 : PlayInfo.sInvite1Answer;
        xsip_call_build_bye((char*)sMsg.c_str(), szBye, false, PlayInfo.nCamCseq);
        SetDestAddr(PlayInfo.sMediaRecvUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "d: ��ͻ���[%s]����Bye, CallID[%s].", 
            PlayInfo.sMediaRecvUri.c_str(), PlayInfo.sCallID1.c_str());
    }


    return 0;
}
int CSIPServer::ByeAnswerRecv(SOCKADDR_IN &addr, char *msg)
{    
    string sCallid = xsip_get_call_id(msg);
    string sTo = xsip_get_to_url(msg);
    string sTo_Code = xsip_get_url_username((char*)sTo.c_str());
    if(sTo_Code.substr(10, 3) == "202")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#RTP��ý��ر����ɹ�, CallID[%s]", sCallid.c_str());
    }
    else if(sTo_Code.substr(10, 3) == "131" || sTo_Code.substr(10, 3) == "132")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#��ͷ�ر���Ƶ���ɹ�, CallID[%s]", sCallid.c_str());
    }
    else if(sTo_Code.substr(10, 3) == "133")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#������ͨ���رճɹ�, CallID[%s]", sCallid.c_str());
    }
    return 0;  
}

int CSIPServer::PTZMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	//��ȡDeviceID
	string sDeviceID = xsip_get_uri(msg, "<DeviceID>", "</DeviceID>", MAX_USERNAME_LEN);
	//�жϾ�ͷ�����Ƿ�����
    PLAYINFO PlayInfo;
    if(!m_pRTPManager->GetPlayInfoByID(sDeviceID, false, &PlayInfo))
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: ��ͷ[%s]δ����, PTZ����ʧ��.", sDeviceID.c_str());
	    return -1;
	}
    string sAddr = PlayInfo.sCameraUri;

	//��ȡUserID
	string sFromUrl = xsip_get_from_url(msg);
	size_t nPos1 = sFromUrl.find(":");
	size_t nPos2 = sFromUrl.find("@");
	string sUserID = sFromUrl.substr(nPos1 + 1, nPos2 - nPos1 - 1);
	//��ȡ�û�Ȩ��
	//int nUserLevel = GetUserLevel(sUserID);
	string sUserLevel = xsip_get_uri(msg, "<ControlPriority>", "</ControlPriority>", MAX_USERNAME_LEN);
	int nUserLevel = atoi(sUserLevel.c_str());
	//��ȡ�û���������(����������������)
	string sCmdType = xsip_get_uri(msg, "<PTZCmd>", "</PTZCmd>", MAX_USERNAME_LEN);
	int nMsgType = 0;//������Ϣ����
	LockMap::iterator itMap = m_LockedDeviceMap.find(sDeviceID);
	if (sCmdType == "LOCK")
	{
		//����
		printf("�û�[%s:%d]����������ͷ[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap == m_LockedDeviceMap.end())
		{
				nMsgType = PTZ_LOCK_SUCCESSED;//�豸δ������,ֱ�Ӽ�������200
		}
		else
		{
			if (sUserID == itMap->second.sUserID)
			{
				//���ƽ̨�����Ҫ�ж�
				if (nUserLevel > itMap->second.nUserLevel)
				{
					nMsgType = PTZ_LOCK_SUCCESSED;
				}
				else if (nUserLevel == itMap->second.nUserLevel)
				{
					nMsgType = PTZ_LOCK_FAILED_LOCKED;
					printf("��ǰ�û����������������ٴ�����\n");
				}
				else
				{
					nMsgType = PTZ_LOCK_FAILED_LOW;
					printf("��ǰ�豸����Ȩ���û�����������ʧ��\n");
				}
			}
			else
			{
				if (nUserLevel > itMap->second.nUserLevel)
				{
					itMap = m_LockedDeviceMap.erase(itMap);
					nMsgType = PTZ_LOCK_SUCCESSED;//��ǰ�û�Ȩ�޽ϸߣ�����������200
				}
				else
				{
					nMsgType = PTZ_LOCK_FAILED_LOW;//��ǰ�û�Ȩ�޽ϵͣ�����ʧ�ܷ���400
					printf("��ǰ�豸����Ȩ���û�����������ʧ��\n");
				}
			}
		}	
	}
	else if (sCmdType == "UNLOCK")
	{
		//����
		printf("�û�[%s:%d]���������ͷ[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap != m_LockedDeviceMap.end())
		{
			if (sUserID == itMap->second.sUserID && nUserLevel == itMap->second.nUserLevel)
			{
				m_LockedDeviceMap.erase(itMap);
				nMsgType = PTZ_UNLOCK_SUCCESSED;//�����ɹ�,����200
			}
			else
			{
				nMsgType = PTZ_UNLOCK_FAILED;//�û�δ����������400
				printf("�豸δ���������������\n");
			}
		}
		else
		{
			nMsgType = PTZ_UNLOCK_FAILED;//�豸û�б�����������400
			printf("�豸δ���������������\n");
		}
	}
	else
	{
		//����
		printf("�û�[%s:%d]���������̨[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap == m_LockedDeviceMap.end())
		{
			nMsgType = PTZ_CONTROL_SUCCESSED;//�豸δ��������ֱ�ӿ��Ʋ�����200
		}
		else
		{
			if (sUserID == itMap->second.sUserID)
			{
				nMsgType = PTZ_CONTROL_SUCCESSED;//�豸����ǰ�û�������ֱ�ӿ��Ʋ�����200
			}
			else
			{
				if (nUserLevel > itMap->second.nUserLevel)
				{
					nMsgType = PTZ_CONTROL_SUCCESSED;//��ǰ�û�Ȩ�޽ϸߣ��ɲ����豸������200
				}
				else
				{
					nMsgType = PTZ_CONTROL_FAILED;//��ǰ�û�Ȩ�޽ϵͣ��޷������豸������400
					printf("�豸���������޷�����\n");
				}
			}
		}
	}

	//��Ӧ��Ϣ200
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}

	switch(nMsgType)
	{
	case PTZ_LOCK_SUCCESSED:    //�����ɹ�,����200�����û����������б�
		{
			LOCKINFO newLock;
			newLock.sUserID = sUserID;
			newLock.nUserLevel = nUserLevel;
			LockMap::iterator it = m_LockedDeviceMap.find(sDeviceID);
			if (it != m_LockedDeviceMap.end())
			{
				it->second.nUserLevel = nUserLevel;
			}
			else
			{
				m_LockedDeviceMap.insert(LockMap::value_type(sDeviceID, newLock));
			}
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg,200,szAns,dwttag);
			DWORD dwLen = (DWORD)strlen(szAns);
			szAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
		}
		break;
	case PTZ_UNLOCK_SUCCESSED:  //�����ɹ�,����200
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg,200,szAns,dwttag);
			DWORD dwLen = (DWORD)strlen(szAns);
			szAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
		}
		break;
	case PTZ_CONTROL_SUCCESSED: //���Ƴɹ�,����200�����豸���Ϳ�����Ϣ
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg,200,szAns,dwttag);
			DWORD dwLen = (DWORD)strlen(szAns);
			szAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
			PTZControlSend(addr, msg, sAddr);
		}
		break;
	case PTZ_LOCK_FAILED_LOCKED://�ظ�����,����480
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 480, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_LOCK_FAILED_LOW:   //Ȩ�޵ͣ��޷�����������481
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 481, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_UNLOCK_FAILED:		//����ʧ��,����482
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 482, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_CONTROL_FAILED:    //����ʧ��,����483
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 483, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	default:
		break;
	}
	
	return nMsgType;

}

int CSIPServer::PTZControlSend(SOCKADDR_IN &addr, char *msg, string sAddr)
{
	int nRet = 0;
	char szAns[DATA_BUFSIZE] = {0};
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	string sDeviceID = xsip_get_uri(msg, "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);

    PLAYINFO PlayInfo;
    if(!m_pRTPManager->GetPlayInfoByID(sDeviceID, false, &PlayInfo))
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: ��ͷ[%s]ֹͣ����, ����ʧ��.", sDeviceID.c_str());
    }
    if(PlayInfo.nCameraType != 20)     //��RTP��ý��PTZ����, �����RTP��ý��
    {
        SOCKADDR_IN sockin = {0};
        char szPTZ[DATA_BUFSIZE] = {0};
        string sBody = xsip_get_body(msg);
        xsip_message_build_request(szPTZ, "MESSAGE", NULL, PlayInfo.sRTPUri.c_str());
        xsip_set_content_type(szPTZ, XSIP_APPLICATION_XML);
        xsip_set_body(szPTZ, (char *)sBody.c_str(), (int)sBody.size());
        DWORD dwLen = (DWORD)strlen(szPTZ);
        szPTZ[dwLen] = '\0';
        SetDestAddr(PlayInfo.sRTPUri, &sockin);
        m_pSIPProtocol->sendBuf(sockin, szPTZ, dwLen);		//��SIP����
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "----��RTP��ý��[%s]����PTZ��������.", PlayInfo.sRTPUri.c_str());
    }
    else           //��������PTZ����, ����������豸
	{
		SOCKADDR_IN sockin = {0};
		char szPTZ[DATA_BUFSIZE] = {0};
		string sBody = xsip_get_body(msg);
		xsip_message_build_request(szPTZ,"MESSAGE", NULL, sAddr.c_str());
		xsip_set_content_type(szPTZ, XSIP_APPLICATION_XML);
		xsip_set_body(szPTZ, (char *)sBody.c_str(), (int)sBody.size());
		DWORD dwLen = (DWORD)strlen(szPTZ);
		szPTZ[dwLen] = '\0';
		SetDestAddr(sAddr, &sockin);
		m_pSIPProtocol->sendBuf(sockin, szPTZ, dwLen);		//��SIP����
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "----���豸[%s]����PTZ��������.", sAddr.c_str());
    }
    
    if(m_pam.m_bISINITIAL)
    {
        EnterCriticalSection(&m_csInitial);
        map<string, INITIALCAMERAINFO>::iterator it = m_mapInitialCamera.find(sDeviceID);
        string sCmdType = xsip_get_uri(msg, "<PTZCmd>", "</PTZCmd>", MAX_USERNAME_LEN);
        if(sCmdType == "INITIAL")
        {
            time_t tCurrentTime = time(&tCurrentTime);
            if(it == m_mapInitialCamera.end())
            {
                INITIALCAMERAINFO  InitialCameraInfo;
                InitialCameraInfo.tLastControlTime = tCurrentTime;

                InitialCameraInfo.sLowServerIP = xsip_get_url_host((char *)sAddr.c_str());
                InitialCameraInfo.nLowServerPort = xsip_get_url_port((char *)sAddr.c_str());
                m_mapInitialCamera.insert(make_pair(sDeviceID, InitialCameraInfo));
                printf("����µ�����ID[%s]\n", sDeviceID.c_str());
            }
        }
        else if(sCmdType == "UNINITIAL")
        {
            if(it != m_mapInitialCamera.end())
            {
                m_mapInitialCamera.erase(it);        
                printf("ɾ������ID[%s]\n", sDeviceID.c_str());
            }
        }

        it = m_mapInitialCamera.find(sDeviceID);
        if(it != m_mapInitialCamera.end())// && sCmdType == "A50F0100000000B5")
        {
            map<string, INITIALCAMERAINFO>::iterator itPtz = m_mapPTZInitial.find(sDeviceID);
            if(itPtz != m_mapPTZInitial.end())
            {
                time(&(*itPtz).second.tLastControlTime);
            }
            else
            {
                time(&(*it).second.tLastControlTime);
                m_mapPTZInitial.insert(make_pair(sDeviceID, it->second));
            }
            
            printf("������̨����ʱ��, ��ͷID[%s]\n", sDeviceID.c_str());
        }

        LeaveCriticalSection(&m_csInitial);
    }
    
	return nRet;
}

int CSIPServer::CommandResponse(SOCKADDR_IN &addr, char *msg)
{
    char szAns[DATA_BUFSIZE] = {0};
    string dwttag = xsip_get_to_tag(msg);
    if (dwttag == "")
    {
        dwttag = xsip_get_tag();
    }
    xsip_message_build_answer(msg,200,szAns,dwttag);
    DWORD dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);

    return 0;
}
int CSIPServer::ReplayControlMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	//static int siCseq = 1;
	SOCKADDR_IN sockin = {0};
	char szAnswer[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	string sDeviceID = xsip_get_to_url(msg);
	sDeviceID = xsip_get_url_username((char *)sDeviceID.c_str());
	string sCallID = xsip_get_call_id(msg);
	string sFmTag = xsip_get_from_tag(msg);
	int nCeq = xsip_get_cseq_number(msg);

    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if(REPNONE == rep)
    {
        xsip_message_build_answer(msg, 400, szAnswer, dwttag);
        dwLen = (DWORD)strlen(szAnswer); 
        szAnswer[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "δ�ҵ�ƥ��¼��طſ���, CallID[%s]!", sCallID.c_str());
        return -1;
    }

	string sBody = xsip_get_body(msg);
	if (sBody == "")
	{
		xsip_message_build_answer(msg, 400, szAnswer, dwttag);
		dwLen = (DWORD)strlen(szAnswer); 
		szAnswer[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Info������ϢδЯ��sdp, CallID[%s] .", sCallID.c_str());
		return -1;
	}

	int nType = xsip_get_rtsp_type(sBody.c_str());
    string sType = nType == XSIP_RTSP_PAUSE ? "Pause" : (nType == XSIP_RTSP_REPLAY ? "Replay" : (nType == XSIP_RTSP_SCALE ? "Scale" : (nType == XSIP_RTSP_RANGEPLAY ? "Range" : "Teardown")));
	if (nType != XSIP_RTSP_TEARDOWN)
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��طſ�������[%s].", sType.c_str());
		double dScale = xsip_get_rtsp_scale(sBody.c_str());
		long lRange = xsip_get_rtsp_npt(sBody.c_str());
		if (dScale < 0)
		{
			xsip_message_build_answer(msg, 551, szAnswer, dwttag);
			dwLen = (DWORD)strlen(szAnswer);
			szAnswer[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);
			return -1;
		}
		if (lRange < 0)
		{
			xsip_message_build_answer(msg, 457, szAnswer, dwttag);
			dwLen = (DWORD)strlen(szAnswer);
			szAnswer[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);
			return -1;
		}
	}
	xsip_message_build_answer(msg, 200, szAnswer, dwttag);
	dwLen = (DWORD)strlen(szAnswer);
	szAnswer[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);

    if(PlayInfo.nCameraType != 20)
    {
        memset(szAnswer, 0, sizeof(szAnswer));		
        SetDestAddr(PlayInfo.sRTPUri, &sockin);
        string sFromTag = xsip_get_from_tag((char*)PlayInfo.sInvite2Answer.c_str());
        string sToTag = xsip_get_to_tag((char*)PlayInfo.sInvite2Answer.c_str());
        xsip_info_build_initial(szAnswer, "INFO", PlayInfo.sRTPUri.c_str(), PlayInfo.sRTPUri.c_str(), NULL, 
            PlayInfo.sCallID4.c_str(), nCeq, sFromTag, sToTag);
        xsip_set_content_type(szAnswer, XSIP_APPLICATION_RTSP);
        xsip_set_body(szAnswer, (char *)sBody.c_str(), (int)sBody.size());
        dwLen = (DWORD)strlen(szAnswer);
        szAnswer[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(sockin, szAnswer, dwLen);		//����ý�����ת��
    }
    else
    {
        memset(szAnswer, 0, sizeof(szAnswer));		
	    SetDestAddr(PlayInfo.sCameraUri, &sockin);
        string sFromTag = xsip_get_from_tag((char*)PlayInfo.sInvite3Answer.c_str());
        string sToTag = xsip_get_to_tag((char*)PlayInfo.sInvite3Answer.c_str());
	    xsip_info_build_initial(szAnswer, "INFO", PlayInfo.sCameraUri.c_str(), PlayInfo.sCameraUri.c_str(), NULL, 
                                PlayInfo.sCallID3.c_str(), nCeq, sFromTag, sToTag);
	    xsip_set_content_type(szAnswer, XSIP_APPLICATION_RTSP);
	    xsip_set_body(szAnswer, (char *)sBody.c_str(), (int)sBody.size());
	    dwLen = (DWORD)strlen(szAnswer);
	    szAnswer[dwLen] = '\0';
	    m_pSIPProtocol->sendBuf(sockin, szAnswer, dwLen);		//���豸ת��
    }
    m_pRTPManager->AddPlaybackCseq(sCallID);

	return 0;
}
int CSIPServer::ReplayMediaStatusRecv(SOCKADDR_IN &addr, char *msg)
{
    char szAns[DATA_BUFSIZE] = {0};
    string sCallID = xsip_get_call_id(msg);
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ�ý����������Ϣ, CallID[%s].", sCallID.c_str());

    string sSdp = xsip_get_body(msg);
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if(rep == REPCAMERASEND)  //�豸����MediaStatus��Ϣ
    {
        xsip_message_build_answer(msg, 200, szAns);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);        //�ظ��豸200 OK

        //���û�ת��MediaStatus
        string sCallID1 = PlayInfo.sCallID1;
        string sFrom = xsip_get_from_url((char*)PlayInfo.sInvite1Answer.c_str());
        string sTo = xsip_get_to_url((char*)PlayInfo.sInvite1Answer.c_str());
        string sFmTag = xsip_get_from_tag((char*)PlayInfo.sInvite1Answer.c_str());
        string sToTag = xsip_get_to_tag((char*)PlayInfo.sInvite1Answer.c_str());
        int nCeq = xsip_get_cseq_number((char*)PlayInfo.sInvite1Answer.c_str());
        char szMediaStatus[DATA_BUFSIZE] = {0};
        xsip_message_build_request(szMediaStatus, "MESSAGE", PlayInfo.sMediaRecvUri.c_str(), sTo.c_str(), sFrom.c_str(), 
                                    sCallID1.c_str(), nCeq + PlayInfo.nCamCseq, sFmTag, sToTag);
        
        xsip_set_content_type(szMediaStatus, XSIP_APPLICATION_RTSP);
        xsip_set_body(szMediaStatus, (char *)sSdp.c_str(), (int)sSdp.size());
        dwLen = (DWORD)strlen(szMediaStatus);
        szMediaStatus[dwLen] = '\0';

        SOCKADDR_IN addrRecv;
        SetDestAddr(PlayInfo.sMediaRecvUri, &addrRecv);
        m_pSIPProtocol->sendBuf(addrRecv, szMediaStatus, dwLen);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���û�[%s]ת��MediaStatus��Ϣ, CallID[%s].", PlayInfo.sMediaRecverID.c_str(), PlayInfo.sCallID1.c_str());

        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1);
        SendByeMsg(PlayInfo);
    }
    else
    {
        xsip_message_build_answer(msg, 400, szAns);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: �豸MediaStatus������ƥ���¼, CallID[%s].", sCallID.c_str());
    }
    return 0;
}
int CSIPServer::ResendDeviceRecordMsg(SOCKADDR_IN &addr, char *msg)
{
	char szAns[DATA_BUFSIZE] = {0};
	string dwttag = xsip_get_to_tag(msg);
	string dwftag = xsip_get_from_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	xsip_message_build_answer(msg,200,szAns,dwttag);
	DWORD dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//�ظ�200OK

	string sDeviceID = xsip_get_to_url(msg);
	sDeviceID = xsip_get_url_username((char *)sDeviceID.c_str());

	string sAddr = GetDeviceAddr(sDeviceID.c_str());
    if(sAddr == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ��ͷ[%s]δ�ҵ������SIP��Ϣ.", sDeviceID.c_str());
        return -1;
    }

	int nCeq = xsip_get_cseq_number(msg);
	string sCallid = xsip_get_call_id(msg);
	string sBody = xsip_get_body(msg);
	string sHost = xsip_get_url_host((char *)sAddr.c_str());
	int nPort = xsip_get_url_port((char *)sAddr.c_str());
	string sTo = xsip_to_init(sDeviceID.c_str(), sHost.c_str(), nPort);
	SOCKADDR_IN sockin = {0};
	sockin.sin_family = AF_INET;
	sockin.sin_addr.S_un.S_addr = inet_addr(sHost.c_str());
	sockin.sin_port = htons(nPort);

	memset(szAns, 0, DATA_BUFSIZE);
	xsip_message_build_request(szAns,"MESSAGE",sTo.c_str(),sTo.c_str(),XSIP::m_sFrom.c_str(),sCallid.c_str(),nCeq,dwftag,dwttag);
	xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
	xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
	m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//ת�����豸���¼�ƽ̨

	return 0;
}

int CSIPServer::RecordInfoMsgRecv(SOCKADDR_IN &addr, char *msg)
{   
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
        "---------------------------------------------------\r\n�յ�¼���ļ�������Ϣ.");

    char szAns[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
	string dwToTag = xsip_get_to_tag(msg);
	if (dwToTag == "")
	{
		dwToTag = xsip_get_tag();
	}
	string sTo = xsip_get_from_url(msg);
	string dwFmTag = xsip_get_from_tag(msg);
	string sCallID = xsip_get_call_id(msg);
	int nCeq = xsip_get_cseq_number(msg);
	string sBody = xsip_get_body(msg);
	if (sBody == "")
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "û��XML��Ϣ��!");
		return -1;
	}
	string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
	string sID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	string sST = xsip_get_uri((char *)sBody.c_str(), "<StartTime>","</StartTime>", 20);
	string sET = xsip_get_uri((char *)sBody.c_str(), "<EndTime>","</EndTime>", 20);
	string sTP = xsip_get_uri((char *)sBody.c_str(), "<Type>","</Type>", 20);
	if (sBody == "" || sSN == "" || sID == "" || sST == "" || sET == "")
	{
		xsip_message_build_answer(msg,400,szAns,dwToTag);
		dwLen = (DWORD)strlen(szAns);
		szAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ�400 BAD
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "XML��Ϣ�岻��ȷ!");
		return -1;
	}
	xsip_message_build_answer(msg, 200, szAns, dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //�ظ�200 OK

	char szRecord[DATA_BUFSIZE] ={0};
	string sDeviceID = sID;
	string sFrom = xsip_to_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);

    CAMERAINFO CameraInfo;
	string sAddr = GetDeviceAddr(sDeviceID.c_str(), XSIP_INVITE_PLAYBACK, &CameraInfo);			//�ж��Ƿ��Ǳ��ؾ�ͷ�����򻹻��ϼ���ַ
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: ���Ҿ�ͷ[%s]¼����Ϣ, ��ѯ��ַʧ��!", sDeviceID.c_str());
		return -1;
	}
	else
	{
        if(CameraInfo.nNetType == 4)
        {            
            char szRecord[DATA_BUFSIZE] ={0};
            string sDVRType = CameraInfo.sDVRKey;
            BOOL bRes = false;
            int nLoginId = connectDVR((char*)CameraInfo.sDeviceIP.c_str(), 
                CameraInfo.nDevicePort,
                (char*)CameraInfo.sDeviceUsername.c_str(),
                (char*)CameraInfo.sDevicePassword.c_str(),
                CameraInfo.nChannel,
                (char*)sDVRType.c_str(), bRes);
            if(nLoginId <= 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: �����豸[%s:%s:%d]ʧ��",sDVRType.c_str(),
                    CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort);
                return -1;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^�����豸[%s:%s:%d]�ɹ�.",sDVRType.c_str(),
                    CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort);
            }

            sST = sST.replace(10, 1, " ");
            sET = sET.replace(10, 1, " ");
            RECORDFILE pRecinfo[100];
            int nCount = 0;
            nCount = getRecordFileEx(nLoginId, 0, (char*)sST.c_str(), (char*)sET.c_str(), pRecinfo, 100);
            if (nCount < 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: ȡ��ͷ[%s]¼����Ϣʧ��[%s  %s].", 
                    sID.c_str(), sST.c_str(), sET.c_str());
                return -1;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^ȡ��ͷ[%s]¼����Ϣ�ɹ�.\n"
                                                            "**[%s  %s]������Ŀ[%d].", 
                    sID.c_str(), sST.c_str(), sET.c_str(), nCount);
            }
            disconnectDVR(nLoginId);

            memset(szRecord, 0, sizeof(szRecord));
            string sFrom = xsip_get_to_url(msg);
            string sTo = xsip_get_from_url(msg);
            string dwFmTag = xsip_get_from_tag(msg);
            string sCallID = xsip_get_call_id(msg);
            int nCeq = xsip_get_cseq_number(msg);
            xsip_message_build_request(szRecord, "MESSAGE", sFrom.c_str(), sTo.c_str(), sFrom.c_str(), 
                sCallID.c_str(), nCeq, "", dwFmTag);

            string sRecorderID = xsip_get_url_username((char*)sTo.c_str());
            char szBody[4096] = {0};
            int nSum = nCount;
            for (int i=0; i < nCount; i++)
            {
                string sStartTime = pRecinfo[i].startTime;
                string sEndTime = pRecinfo[i].endTime;
                sStartTime = GetFormatTime(sStartTime.c_str());
                sEndTime = GetFormatTime(sEndTime.c_str());
                sprintf_s(szBody, sizeof(szBody), 
                    "<?xml version=\"1.0\"?>\r\n"
                    "<Response>\r\n"
                    "<CmdType>RecordInfo</CmdType>\r\n"
                    "<SN>%s</SN>\r\n"
                    "<DeviceID>%s</DeviceID>\r\n"
                    "<SumNum>%d</SumNum>\r\n"
                    "<RecordList Num=\"1\">\r\n"
                    "<Item>\r\n"
                    "<DeviceID>%s</DeviceID>\r\n"
                    "<Name>Camera</Name>\r\n"
                    "<FilePath>%s</FilePath>\r\n"
                    "<Address>Address</Address>\r\n"
                    "<StartTime>%s</StartTime>\r\n"
                    "<EndTime>%s</EndTime>\r\n"
                    "<Secrecy>0</Secrecy>\r\n"
                    "<Type>Time</Type>\r\n"
                    "<RecorderID>%s</RecorderID>\r\n"
                    "</Item>\r\n"
                    "</RecordList>\r\n"
                    "</Response>\r\n",
                    sSN.c_str(), sID.c_str(), nSum, sDeviceID.c_str(),/*CameraInfo.sCameraName.c_str(), */pRecinfo[i].sFileName,
                    sStartTime.c_str(), sEndTime.c_str(), /*sTP.c_str(),*/ sRecorderID.c_str());

                char szHead[DATA_BUFSIZE] = {0};
                memcpy(szHead, szRecord, strlen(szRecord));

                int nSize = (int)strlen(szBody);
                xsip_set_content_type(szHead, XSIP_APPLICATION_XML);
                xsip_set_body(szHead, szBody, nSize);
                dwLen = (DWORD)strlen(szHead);
                szHead[dwLen] = '\0';

                m_pSIPProtocol->sendBuf(addr, szHead, dwLen);      
                Sleep(10);
            }
        }
        else
        {

            //�滻�ͻ��˷��͹�����xml��Ϣ��sn�ֶ�           
            char pSN[10] = {0};
            sprintf_s(pSN, sizeof(pSN), "%d", m_dwSn ++);
            sBody = xsip_set_body_sn((char*)sBody.c_str(), pSN, strlen(pSN));

            SESSIONINFO info;
            info.sTitle = "RECORDINFO";
            info.sDeviceID = sDeviceID;
            info.sAddr = xsip_get_via_addr(msg);
            info.dwFTag = dwFmTag;
            info.dwTTag = dwToTag;
            info.sSN = sSN;
            time(&info.recvTime);
            memcpy(&info.sockAddr, &addr, sizeof(SOCKADDR_IN));

            EnterCriticalSection(&m_sLock);
            m_SessionMap.insert(SessionMap::value_type(pSN, info));
            LeaveCriticalSection(&m_sLock);

            string sRecordCallID = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
            xsip_message_build_request(szRecord, "MESSAGE", NULL, sAddr.c_str(), NULL, sRecordCallID.c_str(), 20);
            xsip_set_content_type(szRecord, XSIP_APPLICATION_XML);
                       
            xsip_set_body(szRecord, (char *)sBody.c_str(), (int)sBody.size());
            dwLen = (DWORD)strlen(szRecord);
            szRecord[dwLen] = '\0';
            SOCKADDR_IN sockin = {0};
            SetDestAddr(sAddr, &sockin);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���豸[%s]����¼���ѯ����......", sAddr.c_str());
            m_pSIPProtocol->sendBuf(sockin, szRecord, dwLen);
        }
		
	}
	return 0;
}

int CSIPServer::RecordInfoAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
    if(string(msg).find("</Response>") == string::npos)
    {
        string sCallID = xsip_get_call_id(msg);
        printf("--------------------Error: RecordInfo xml content illegal!, CallID[%s]\n", sCallID.c_str());
        return -1;
    }

	char szResponse[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
    //char *pBuf = _atou(msg);
	string sBody = xsip_get_body(msg);
	string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
	string sDeviceID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	string sTtag = xsip_get_to_tag(msg);
	if (sBody == "" && sDeviceID == "" && sTtag == "")
	{
		xsip_message_build_answer(msg,400,szResponse,sTtag);
		dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//SDP ���� �ظ�400 BAD
        //delete [] pBuf;
		return -1;
	}
	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		xsip_message_build_answer(msg, 200, szResponse, sTtag);
		dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//�ظ�200 OK
	
		string sCallid = xsip_get_call_id(msg);
		int nceq = xsip_get_cseq_number(msg);
		memset(szResponse, 0, sizeof(szResponse));
		string sFrom = xsip_to_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);
		xsip_message_build_request(szResponse,"MESSAGE",sFrom.c_str(),iter->second.sAddr.c_str(),sFrom.c_str(),
			sCallid.c_str(),nceq,iter->second.dwTTag,iter->second.dwFTag);
		xsip_set_content_type(szResponse, XSIP_APPLICATION_XML);

        sBody = xsip_set_body_sn((char*)sBody.c_str(), (char*)iter->second.sSN.c_str(), iter->second.sSN.size());
		xsip_set_body(szResponse, (char *)sBody.c_str(), (int)sBody.size());
		dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szResponse, dwLen);			//ת��      
		
		TiXmlDocument *pMyDocument = new TiXmlDocument();
		pMyDocument->Parse(sBody.c_str());
		TiXmlElement *pRootElement = pMyDocument->RootElement();
		string sSumNum = ReValue(pRootElement->FirstChildElement("SumNum"));

        string sListNum = "0";
        TiXmlElement *pRecordList = pRootElement->FirstChildElement("RecordList");
        if(NULL != pRecordList)
        {
            TiXmlAttribute* attr = pRecordList->FirstAttribute();
            if(NULL != attr)
            {
                sListNum = attr->Value();
            }
        }

		if (sSumNum != "" && sSumNum != "0")
		{			
			iter->second.nNum += atoi(sListNum.c_str());
			if (iter->second.nNum >= atoi(sSumNum.c_str()))
			{
				g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "ת��¼��Ŀ¼��Ϣ���[%d]!", iter->second.nNum);
				iter = m_SessionMap.erase(iter);
			}
		}
		delete pMyDocument;
	}
	else
	{
		xsip_message_build_answer(msg,400,szResponse,sTtag);
		dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//�ظ�400 BAD
		printf("û���ҵ�ƥ��ĻỰ��ʶ!");
        //delete [] pBuf;
		LeaveCriticalSection(&m_sLock);
		return -1;
	}
    //delete [] pBuf;
	LeaveCriticalSection(&m_sLock);
	return 0;
}
int CSIPServer::UserRecvSmartMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    string sUserUri = xsip_get_from_url(msg);
    string sUserID = xsip_get_url_username((char*)sUserUri.c_str());

    string sBody = xsip_get_body(msg);
    string sEnable = xsip_get_uri((char *)sBody.c_str(), "<Enable>", "</Enable>", 10);

    EnterCriticalSection(&m_SmartLock);
    if(sEnable == "false")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�[%s]ȡ������Smart������Ϣ!", sUserID.c_str());
        SmartUserMap::iterator SmartIt = m_mapSmartUser.find(sUserID);
        if(SmartIt != m_mapSmartUser.end())
        {
            m_mapSmartUser.erase(SmartIt);
        }
    }
    else if(sEnable == "true")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�[%s]��ʼ����Smart������Ϣ!", sUserID.c_str());
        SMARTUSER SmartUser;
        SmartUser.sUserUri = sUserUri;
        SmartUser.sUserID = sUserID;
        SmartUserMap::iterator SmartIt = m_mapSmartUser.find(sUserID);
        if(SmartIt != m_mapSmartUser.end())
        {
            m_mapSmartUser.erase(SmartIt);
        }
        m_mapSmartUser.insert(make_pair(sUserID, SmartUser));      
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�û�[%s]������Ϣ����!", sUserID.c_str());
    }
    LeaveCriticalSection(&m_SmartLock);

    return 0;
}
int CSIPServer::DeviceSmartMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ������豸Smart������Ϣ!");

    char szAns[DATA_BUFSIZE] = {0};
    DWORD dwLen = 0;
    string dwToTag = xsip_get_to_tag(msg);
    if (dwToTag == "")
    {
        dwToTag = xsip_get_tag();
    }
    string sFrom = xsip_get_to_url(msg);
    string dwFmTag = xsip_get_from_tag(msg);
    string sTo = xsip_get_from_url(msg);
    string sCallID = xsip_get_call_id(msg);
    int nCeq = xsip_get_cseq_number(msg);
    string sBody = xsip_get_body(msg);
    string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
    string sID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
    if (sBody == "" || sSN == "" || sID == "")
    {
        xsip_message_build_answer(msg,400,szAns,dwToTag);
        dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ�400 BAD
        return -1;
    }
    xsip_message_build_answer(msg,200,szAns,dwToTag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);			//�ظ�200 OK

    SOCKADDR_IN sockin = {0};
    char szDvrinfo[DATA_BUFSIZE] ={0};
    string sDeviceID = sID;
    string sAddr = GetDeviceAddr(sDeviceID.c_str()); //�����豸��ַ
    if (sAddr == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ��ͷ[%s]δ�ҵ��������Ϣ.", sDeviceID.c_str());
        return -1;
    }
    else
    {
        string sUserUri = xsip_get_from_url(msg);
        string sUserID = xsip_get_url_username((char*)sUserUri.c_str());
        SMARTUSER SmartUser;
        SmartUser.sUserUri = sUserUri;
        SmartUser.sUserID = sUserID;
        EnterCriticalSection(&m_SmartLock);
        SmartUserMap::iterator SmartIt = m_mapSmartUser.find(sUserID);
        if(SmartIt != m_mapSmartUser.end())
        {
            m_mapSmartUser.erase(SmartIt);
        }
        m_mapSmartUser.insert(make_pair(sUserID, SmartUser));       //����û�������Smart����, �򽫴��û�����Smart����֪ͨ�б�
        LeaveCriticalSection(&m_SmartLock);

        EnterCriticalSection(&m_csLock);
        string sSmartCallID = xsip_call_id_init(xsip_get_CallIDNum(), m_sSIPServerIP.c_str());
        LeaveCriticalSection(&m_csLock);
        xsip_message_build_request(szDvrinfo, "MESSAGE", NULL, sAddr.c_str(), NULL, sSmartCallID.c_str());
        xsip_set_content_type(szDvrinfo, XSIP_APPLICATION_XML);
        xsip_set_body(szDvrinfo, (char *)sBody.c_str(), (int)sBody.size());
        dwLen = (DWORD)strlen(szDvrinfo);
        szDvrinfo[dwLen] = '\0';
        SetDestAddr(sAddr, &sockin);
        m_pSIPProtocol->sendBuf(sockin, szDvrinfo, dwLen);		//���豸|�¼�ת��
    }

    return 0;
}
int CSIPServer::DeviceSmartAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��豸Smart������Ϣ!");

    char szAns[DATA_BUFSIZE] = {0};
    DWORD dwLen = 0;
    string dwttag = xsip_get_to_tag(msg);
    if (dwttag == "")
    {
        dwttag = xsip_get_tag();
    }
    string sDeviceID = xsip_get_uri(msg, "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
    string sSmartTime = xsip_get_uri(msg, "<AlarmTime>","</AlarmTime>", MAX_USERNAME_LEN);
    if (sDeviceID == "")
    {
        xsip_message_build_answer(msg, 400, szAns, dwttag);
        dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //�ظ�400 BAD

        return 0;
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�豸[%s]Smart������Ϣ, Time[%s].", sDeviceID.c_str(), sSmartTime.c_str());
    xsip_message_build_answer(msg, 200, szAns, dwttag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//�ظ�200 OK

    EnterCriticalSection(&m_SmartLock);
    SmartUserMap::iterator SmartIt = m_mapSmartUser.begin();
    for(; SmartIt != m_mapSmartUser.end(); SmartIt ++)          //�յ���Smart������Ϣ, ת����ÿһ�����ձ�����Ϣ���û�
    {
        string sTo = SmartIt->second.sUserUri;
        string sFrom = xsip_from_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);
        memset(szAns, 0, DATA_BUFSIZE);
        xsip_message_build_request(szAns,"MESSAGE", sFrom.c_str(), sTo.c_str(), sFrom.c_str());    

        string sSdp = xsip_get_body(msg);
        xsip_set_content_type(szAns, 1);
        xsip_set_body(szAns, (char*)sSdp.c_str(), sSdp.size());
        SOCKADDR_IN sockAddr;
        SetDestAddr(SmartIt->second.sUserUri, &sockAddr);
        m_pSIPProtocol->sendBuf(sockAddr, szAns, (DWORD)strlen(szAns)); //ת�����ϼ�|�û�
    }
    LeaveCriticalSection(&m_SmartLock);

    return 0;
}
int CSIPServer::DBSaveUserToSmart(string sUserID)
{


    return 0;
}
int CSIPServer::DBSaveDeviceSmartSet(string sBody)
{


    return 0;
}
int CSIPServer::DeviceInfoMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��豸��Ϣ��ѯ��Ϣ!");

	char szAns[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
	string dwToTag = xsip_get_to_tag(msg);
	if (dwToTag == "")
	{
		dwToTag = xsip_get_tag();
	}
	string sFrom = xsip_get_to_url(msg);
	string dwFmTag = xsip_get_from_tag(msg);
	string sTo = xsip_get_from_url(msg);
	string sCallID = xsip_get_call_id(msg);
	int nCeq = xsip_get_cseq_number(msg);
	string sBody = xsip_get_body(msg);
	string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
	string sID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	if (sBody == "" || sSN == "" || sID == "")
	{
		xsip_message_build_answer(msg,400,szAns,dwToTag);
		dwLen = (DWORD)strlen(szAns);
		szAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ�400 BAD
		return -1;
	}
	xsip_message_build_answer(msg,200,szAns,dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);			//�ظ�200 OK

	SOCKADDR_IN sockin = {0};
	char szDvrinfo[DATA_BUFSIZE] ={0};
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str()); //�����豸��ַ
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ��ͷ[%s]δ�ҵ��������Ϣ.", sDeviceID.c_str());
		return -1;
	}
	else
	{
		SESSIONINFO info;
		info.sTitle = "DEVICEINFO";
		info.sAddr = xsip_get_via_addr(msg);
		info.dwFTag = dwFmTag;
		info.dwTTag = dwToTag;
		memcpy(&info.sockAddr, &addr, sizeof(SOCKADDR_IN));

		EnterCriticalSection(&m_sLock);
		m_SessionMap.insert(SessionMap::value_type(sSN, info));	//����Ự
		LeaveCriticalSection(&m_sLock);

		xsip_message_build_request(szDvrinfo,"MESSAGE",NULL,sAddr.c_str(),NULL,sCallID.c_str(),nCeq,dwFmTag);
		xsip_set_content_type(szDvrinfo, XSIP_APPLICATION_XML);
		xsip_set_body(szDvrinfo, (char *)sBody.c_str(), (int)sBody.size());
		dwLen = (DWORD)strlen(szDvrinfo);
		szDvrinfo[dwLen] = '\0';
		SetDestAddr(sAddr, &sockin);
		m_pSIPProtocol->sendBuf(sockin, szDvrinfo, dwLen);		//���豸|�¼�ת��
	}
	return 0;
}

int CSIPServer::DeviceInfoAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
	char szAns[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	string sSN = xsip_get_uri(msg, "<SN>", "</SN>", 10);
	string sDeviceID = xsip_get_uri(msg, "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	if (sDeviceID == "")
	{
		xsip_message_build_answer(msg,400,szAns,dwttag);
		dwLen = (DWORD)strlen(szAns);
		szAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //�ظ�400 BAD

        return 0;
	}
	xsip_message_build_answer(msg,200,szAns,dwttag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//�ظ�200 OK

	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		string sTo = iter->second.sAddr;
		string sFrom = xsip_from_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);
		string dwftag = xsip_get_from_tag(msg);
		int nCeq = xsip_get_cseq_number(msg);
		memset(szAns, 0, DATA_BUFSIZE);
		xsip_message_build_request(szAns,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),NULL,nCeq,dwftag,dwttag);
		string sManu = xsip_get_uri(msg, "<Manufacturer>", "</Manufacturer>", 32);
		string sMode = xsip_get_uri(msg, "<Modeel>", "</Modeel>", 32);
		string sFim = xsip_get_uri(msg, "<Fimware>", "</Fimware>", 32);
		string sCam = xsip_get_uri(msg, "<MaxCamera>", "</MaxCamera>", 6);
		string sAlarm = xsip_get_uri(msg, "<MaxAlarm>", "</MaxAlarm>", 6);
		char szBody[MAX_CONTENT_LEN] = {0};
		sprintf_s(szBody, sizeof(szBody),
			"<?xml version=\"1.0\"?>\r\n" 
			"<Response>\r\n"
			"<CmdType>DeviceInfo</CmdType>\r\n"
			"<SN>%s</SN>\r\n"
			"<DeviceID>%s</DeviceID>\r\n"
			"<Result>OK</Result>\r\n"
			"<DeviceType>DVR</DeviceType>\r\n"
			"<Manufacturer>%s</Manufacturer>\r\n"
			"<Modeel>%s</Modeel>\r\n"
			"<Fimware>%s</Fimware>\r\n"
			"<MaxCamera>%s</MaxCamera>\r\n"
			"<MaxAlarm>%s</MaxAlarm>\r\n"
			"</Response>\r\n",
			sSN.c_str(), sDeviceID.c_str(), sManu.c_str(), sMode.c_str(), sFim.c_str(), sCam.c_str(), sAlarm.c_str());
		xsip_set_content_type(szAns, 1);
		xsip_set_body(szAns, szBody, (int)strlen(szBody));
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szAns, (DWORD)strlen(szAns)); //ת�����ϼ�|�û�
		iter = m_SessionMap.erase(iter);
	}
	else
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�豸��Ϣ��ѯ��Ӧ, xml SN[%s]δ�ҵ���Ӧ�Ự��Ϣ.", sSN.c_str());
		LeaveCriticalSection(&m_sLock);
		return -1;
	}
	LeaveCriticalSection(&m_sLock);
	return 0;
}

int CSIPServer::DeviceStatusMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��豸״̬��ѯ��Ϣ!");

    char szAns[DATA_BUFSIZE] = {0};
	DWORD dwLen = 0;
	string dwToTag = xsip_get_to_tag(msg);
	if (dwToTag == "")
	{
		dwToTag = xsip_get_tag();
	}
	string sTo = xsip_get_from_url(msg);
	string dwFmTag = xsip_get_from_tag(msg);
	string sCallID = xsip_get_call_id(msg);
	int nCeq = xsip_get_cseq_number(msg);
	string sBody = xsip_get_body(msg);
	string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
	string sID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	if (sBody == "" || sSN == "" || sID == "")
	{
		xsip_message_build_answer(msg,400,szAns,dwToTag);
		dwLen = (DWORD)strlen(szAns);
		szAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP ���� �ظ�400 BAD
		return -1;
	}
	xsip_message_build_answer(msg, 200, szAns, dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//�ظ�200 0K

	SOCKADDR_IN sockin = {0};
	char szStatus[DATA_BUFSIZE] ={0};
	char szBody[MAX_CONTENT_LEN] = {0};
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str()); 
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: ��ͷ[%s]δ�ҵ��������Ϣ.", sDeviceID.c_str());
		return -1;
	}
    else
    {
	    SESSIONINFO info;
	    info.sTitle = "DEVICESTATUS";
	    info.sAddr = xsip_get_via_addr(msg);
	    info.dwFTag = dwFmTag;
	    info.dwTTag = dwToTag;
	    memcpy(&info.sockAddr, &addr, sizeof(SOCKADDR_IN));

	    EnterCriticalSection(&m_sLock);
	    m_SessionMap.insert(SessionMap::value_type(sSN, info));
	    LeaveCriticalSection(&m_sLock);

	    xsip_message_build_request(szStatus,"MESSAGE",NULL,sAddr.c_str(),NULL,sCallID.c_str(),20,dwFmTag);
	    xsip_set_content_type(szStatus, XSIP_APPLICATION_XML);
	    xsip_set_body(szStatus, (char *)sBody.c_str(), (int)sBody.size());
	    dwLen = (DWORD)strlen(szStatus);
	    szStatus[dwLen] = '\0';
	    SetDestAddr(sAddr, &sockin);
	    m_pSIPProtocol->sendBuf(sockin, szStatus, dwLen);		//���ϼ�|�û�ת��
    }
	
	return 0;
}

int CSIPServer::DeviceStatusAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
	char szResponse[DATA_BUFSIZE] = {0};
	string sBody = xsip_get_body(msg);
	string sSN = xsip_get_uri((char *)sBody.c_str(), "<SN>", "</SN>", 10);
	string sDeviceID = xsip_get_uri((char *)sBody.c_str(), "<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	string sTag = xsip_get_to_tag(msg);
	if (sBody == "" && sDeviceID == "" && sTag == "")
	{
		xsip_message_build_answer(msg, 400, szResponse, sTag);
		DWORD dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//�ظ�400 BAD
	}
	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		xsip_message_build_answer(msg, 200, szResponse, iter->second.dwTTag);
		DWORD dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//�ظ�200 OK

		string sCallid = xsip_get_call_id(msg);
		int nceq = xsip_get_cseq_number(msg);
		memset(szResponse, 0, sizeof(szResponse));
		string sFrom = xsip_to_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);
		xsip_message_build_request(szResponse,"MESSAGE",sFrom.c_str(),iter->second.sAddr.c_str(),sFrom.c_str(),
			sCallid.c_str(),nceq,iter->second.dwTTag,iter->second.dwFTag);
		xsip_set_content_type(szResponse, XSIP_APPLICATION_XML);
		xsip_set_body(szResponse, (char *)sBody.c_str(), (int)sBody.size());
		dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szResponse, dwLen);			//ת���ϼ�|�û�

		iter = m_SessionMap.erase(iter);
	}
	else
	{
		xsip_message_build_answer(msg, 404, szResponse, sTag);
		DWORD dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//�ظ�400 BAD
		LeaveCriticalSection(&m_sLock);
		return -1;
	}
	LeaveCriticalSection(&m_sLock);
	return 0;
}

int CSIPServer::DeviceGuardMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	char szAns[DATA_BUFSIZE] = {0};
	string sTo = xsip_get_from_url(msg);
	string sFrom = xsip_get_to_url(msg);
	string dwToTag = xsip_get_from_tag(msg);
	string dwFmTag = xsip_get_to_tag(msg);
	if (dwFmTag == "")
	{
		dwFmTag = xsip_get_tag();
	}
	xsip_message_build_answer(msg,200,szAns,dwFmTag);
	DWORD dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//�ظ�200OK

	int nceq = xsip_get_cseq_number(msg);
	string sSN = xsip_get_uri(msg,"<SN>","</SN>",10);
	string sID = xsip_get_uri(msg,"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str());
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: �豸[%s]δ�ҵ������SIP������Ϣ.", sDeviceID.c_str());
		char szMsg[DATA_BUFSIZE] = {0};
		xsip_message_build_request(szMsg,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),NULL,nceq,dwFmTag,dwToTag);
		char szBody[MAX_CONTENT_LEN] = {0};
		sprintf_s(szBody, sizeof(szBody),
			"<?xml version=\"1.0\"?>\r\n"
			"<Response>\r\n"
			"<CmdType>DeviceControl</CmdType>\r\n"
			"<SN>%s</SN>\r\n"
			"<DeviceID>%s</DeviceID>\r\n"
			"<Result>ERROR</Result>\r\n"
			"</Response>\r\n",
			sSN.c_str(), sID.c_str());
		xsip_set_content_type(szMsg, XSIP_APPLICATION_XML);
		xsip_set_body(szMsg, szBody, (int)strlen(szBody));
		DWORD dwLen = (DWORD)strlen(szMsg);
		szMsg[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);	//�ظ�Ӧ��MESSAGE
		return -1;
	}
	char szMsg[DATA_BUFSIZE] = {0};
	xsip_message_build_request(szMsg,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),NULL,nceq,dwFmTag,dwToTag);
	char szBody[MAX_CONTENT_LEN] = {0};
	sprintf_s(szBody, sizeof(szBody),
		"<?xml version=\"1.0\"?>\r\n"
		"<Response>\r\n"
		"<CmdType>DeviceControl</CmdType>\r\n"
		"<SN>%s</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"<Result>OK</Result>\r\n"
		"</Response>\r\n",
		sSN.c_str(), sID.c_str());
	xsip_set_content_type(szMsg, XSIP_APPLICATION_XML);
	xsip_set_body(szMsg, szBody, (int)strlen(szBody));
	dwLen = (DWORD)strlen(szMsg);
	szMsg[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);	//�ظ�Ӧ��MESSAGE

	string sCmd = xsip_get_uri(msg, "<GuardCmd>","</GuardCmd>", 12);
	if (sCmd == "SetGuard")
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��ͷ[%s]�յ�����������Ϣ!", sID);
		SESSIONINFO info;
		info.sTitle = "GUARD";
		info.sAddr = sAddr;		//�豸��ַ
		info.dwFTag = dwFmTag;
		info.dwTTag = dwToTag;
		info.sDeviceID = sID;
		info.request = msg;		
		memcpy(&info.sockAddr, &addr, sizeof(SOCKADDR_IN));	//�ͻ��˵�ַ;

		EnterCriticalSection(&m_sLock);
		m_SessionMap.insert(SessionMap::value_type(sSN, info));
		LeaveCriticalSection(&m_sLock);
	}
	else if (sCmd == "ResetGuard")
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "��ͷ[%s]�յ�����������Ϣ!", sID);
		EnterCriticalSection(&m_sLock);
		SessionMap::iterator iter = m_SessionMap.find(sSN);
		if (iter != m_SessionMap.end())
		{
			iter = m_SessionMap.erase(iter);
		}
		LeaveCriticalSection(&m_sLock);
	}

	string sCallid = xsip_get_call_id(msg);
	string sBody = xsip_get_body(msg);
	memset(szAns, 0, DATA_BUFSIZE);
	xsip_message_build_request(szAns,"MESSAGE",NULL,sAddr.c_str(),XSIP::m_sFrom.c_str(),sCallid.c_str(),nceq,dwToTag);
	xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
	xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
	SOCKADDR_IN sockin = {0};
	SetDestAddr(sAddr, &sockin);
	m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//ת��

	return 0;
}

int CSIPServer::DeviceAlarmMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ�����֪ͨ��Ϣ!");
	char szAns[DATA_BUFSIZE] = {0};
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	xsip_message_build_answer(msg,200,szAns,dwttag);
	DWORD dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//�ظ�200 OK

	memset(szAns, 0, DATA_BUFSIZE);
	string dwftag = xsip_get_from_tag(msg);
	string sTo = xsip_get_from_url(msg);
	string sFrom = xsip_get_to_url(msg);
	string sDeviceID = xsip_get_via_username(msg);
	string sCallID = xsip_get_call_id(msg);
	string sProxy = xsip_from_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);
	int nceq = xsip_get_cseq_number(msg);
	string sSN = xsip_get_uri(msg,"<SN>","</SN>",10);
	string sID = xsip_get_uri(msg,"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	xsip_message_build_request(szAns,"MESSAGE",sProxy.c_str(),sTo.c_str(),sFrom.c_str(),sCallID.c_str(),nceq,dwttag,dwftag);
	char szBody[MAX_CONTENT_LEN] = {0};
	sprintf_s(szBody, sizeof(szBody),
		"<?xml version=\"1.0\"?>\r\n"
		"<Response>\r\n"
		"<CmdType>Alarm</CmdType>\r\n"
		"<SN>%s</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"<Result>OK</Result>\r\n"
		"</Response>\r\n",
		sSN.c_str(), sID.c_str());
	xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
	xsip_set_body(szAns, szBody, (int)strlen(szBody));
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//�ظ�Ӧ�� MESSAGE

	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		string sTo = xsip_get_from_url((char *)iter->second.request.c_str());
		string sBody = xsip_get_body(msg);
		memset(szAns, 0, DATA_BUFSIZE);
		xsip_message_build_request(szAns,"MESSAGE",sTo.c_str(),sTo.c_str(),m_sFrom.c_str(),sCallID.c_str(),nceq,dwttag,dwftag);
		xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
		xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szAns, (DWORD)strlen(szAns));		//ת���ͻ���
	}
	else
	{
		//g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��Ч�ĻỰ��ʶ[%s]!<", sSN.c_str());
		iter = m_SessionMap.begin();
		for (;iter!=m_SessionMap.end();iter++)
		{
			if (iter->second.sTitle == "GUARD")
			{
				string sTo = xsip_get_from_url((char *)iter->second.request.c_str());
				string sBody = xsip_get_body(msg);
				memset(szAns, 0, DATA_BUFSIZE);
				xsip_message_build_request(szAns,"MESSAGE",sTo.c_str(),sTo.c_str(),m_sFrom.c_str(),sCallID.c_str(),nceq,dwttag,dwftag);
				xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
				xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
				m_pSIPProtocol->sendBuf(iter->second.sockAddr, szAns, (DWORD)strlen(szAns));
			}
		}
	}
	LeaveCriticalSection(&m_sLock);
	return 0;
}

int CSIPServer::DeviceAlarmMsgAnswer(SOCKADDR_IN &addr, char *msg)
{
	g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "�յ�������λ��Ϣ!");
	char szAns[DATA_BUFSIZE] = {0};
	string sTo = xsip_get_from_url(msg);
	string sFrom = xsip_get_to_url(msg);
	string dwToTag = xsip_get_from_tag(msg);
	string dwFmTag = xsip_get_to_tag(msg);
	if (dwFmTag == "")
	{
		dwFmTag = xsip_get_tag();
	}
	xsip_message_build_answer(msg,200,szAns,dwFmTag);
	DWORD dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//�ظ�200 OK

	int nceq = xsip_get_cseq_number(msg);
	string sCallID = xsip_get_call_id(msg);
	string sSN = xsip_get_uri(msg,"<SN>","</SN>",10);
	string sID = xsip_get_uri(msg,"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	char szMsg[DATA_BUFSIZE] = {0};
	xsip_message_build_request(szMsg,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),sCallID.c_str(),nceq,dwFmTag,dwToTag);
	char szBody[MAX_CONTENT_LEN] = {0};
	sprintf_s(szBody, sizeof(szBody),
		"<?xml version=\"1.0\"?>\r\n"
		"<Response>\r\n"
		"<CmdType>DeviceControl</CmdType>\r\n"
		"<SN>%s</SN>\r\n"
		"<DeviceID>%s</DeviceID>\r\n"
		"<Result>OK</Result>\r\n"
		"</Response>\r\n",
		sSN.c_str(), sID.c_str());
	xsip_set_content_type(szMsg, XSIP_APPLICATION_XML);
	xsip_set_body(szMsg, szBody, (int)strlen(szBody));
	dwLen = (DWORD)strlen(szMsg);
	szMsg[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);

	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		string sAddr = iter->second.sAddr; //�豸��ַ
		if (sAddr != "LOCAL" && sAddr != "")
		{
			string sHost = xsip_get_url_host((char *)sAddr.c_str());
			int nPort = xsip_get_url_port((char *)sAddr.c_str());
			string sTo = xsip_to_init(sID.c_str(), sHost.c_str(), nPort);
			string sBody = xsip_get_body(msg);
			SOCKADDR_IN sockin = {0};
			sockin.sin_family = AF_INET;
			sockin.sin_addr.S_un.S_addr = inet_addr(sHost.c_str());
			sockin.sin_port = htons(nPort);
			memset(szAns, 0, DATA_BUFSIZE);
			xsip_message_build_request(szAns,"MESSAGE",sTo.c_str(),sTo.c_str(),m_sFrom.c_str(),sCallID.c_str(),nceq,dwFmTag);
			xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
			xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
			m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//ת��
		}
		else
		{
			if (m_hAlarm)
			{
				m_bAlarm = FALSE;
				Sleep(100);
				CloseHandle(m_hAlarm);
				m_hAlarm = NULL;
			}
		}
	}
	else
	{
		//g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "��Ч�ĻỰ��ʶ[%s]!<", sSN.c_str());
		iter = m_SessionMap.begin();
		for (;iter!=m_SessionMap.end();iter++)
		{
			if (iter->second.sTitle == "GUARD")
			{
				string sAddr = iter->second.sAddr; //�豸��ַ
				string sHost = xsip_get_url_host((char *)sAddr.c_str());
				int nPort = xsip_get_url_port((char *)sAddr.c_str());
				string sTo = xsip_to_init(sID.c_str(), sHost.c_str(), nPort);
				string sBody = xsip_get_body(msg);
				SOCKADDR_IN sockin = {0};
				sockin.sin_family = AF_INET;
				sockin.sin_addr.S_un.S_addr = inet_addr(sHost.c_str());
				sockin.sin_port = htons(nPort);
				memset(szAns, 0, DATA_BUFSIZE);
				xsip_message_build_request(szAns,"MESSAGE",sTo.c_str(),sTo.c_str(),m_sFrom.c_str(),sCallID.c_str(),nceq,dwToTag,dwFmTag);
				xsip_set_content_type(szAns, XSIP_APPLICATION_XML);
				xsip_set_body(szAns, (char *)sBody.c_str(), (int)sBody.size());
				m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));
			}
		}
	}
	LeaveCriticalSection(&m_sLock);
	return 0;
}

int CSIPServer::DeviceRecordMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    string sTo = xsip_get_from_url(msg);
    string sFrom = xsip_get_to_url(msg);
    string dwToTag = xsip_get_from_tag(msg);
    string dwFmTag = xsip_get_to_tag(msg);
    if (dwFmTag == "")
    {
        dwFmTag = xsip_get_tag();
    }
    int nceq = xsip_get_cseq_number(msg);
    string sSN = xsip_get_uri(msg,"<SN>","</SN>",10);
    string sID = xsip_get_uri(msg,"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
    string sCmd = xsip_get_uri(msg, "<RecordCmd>","</RecordCmd>", 12);
    if (sCmd == "Record")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ��ֶ�¼�������Ϣ!");
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�յ�¼��رտ�����Ϣ!");
    }

    ResendDeviceRecordMsg(addr, msg);

    char szMsg[DATA_BUFSIZE] = {0};
    xsip_message_build_request(szMsg,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),NULL,nceq,dwFmTag,dwToTag);
    char szBody[MAX_CONTENT_LEN] = {0};
    sprintf_s(szBody, sizeof(szBody),
        "<?xml version=\"1.0\"?>\r\n"
        "<Response>\r\n"
        "<CmdType>DeviceControl</CmdType>\r\n"
        "<SN>%s</SN>\r\n"
        "<DeviceID>%s</DeviceID>\r\n"
        "<Result>OK</Result>\r\n"
        "</Response>\r\n",
        sSN.c_str(), sID.c_str());
    xsip_set_content_type(szMsg, XSIP_APPLICATION_XML);
    xsip_set_body(szMsg, szBody, (int)strlen(szBody));
    DWORD dwLen = (DWORD)strlen(szMsg);
    szMsg[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);

    return 0;
}



DWORD WINAPI CSIPServer::SuperiorKeepaliveThread(LPVOID lpParam)
{
	CSIPServer *pSip = (CSIPServer *)lpParam;
	pSip->SuperiorSendKeepalive();
	return 0;
}

int CSIPServer::SuperiorSendKeepalive(void)
{
    Sleep(5 * 1000);
    static int nHeartbit = 0;
    SOCKADDR_IN addr;
    char szStatus[DATA_BUFSIZE] = {0};
    int nLen = 0;

    while (m_bKeepalive)
    {
        if(!m_bRegSupServer)
        {
            Sleep(1000 * 60);
            continue;
        }

        char szBody[MAX_CONTENT_LEN] = {0};
        sprintf_s(szBody, sizeof(szBody),
            "<?xml version=\"1.0\"?>\r\n"
            "<Notify>\r\n"
            "<CmdType>Keepalive</CmdType>\r\n"
            "<SN>%u</SN>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<Status>OK</Status>\r\n"
            "</Notify>\r\n",
            m_dwSn++, m_sUsername.c_str());
        int nBodylen = (int)strlen(szBody);
        if (nHeartbit >= 60)                     //ע�ᱣ��ʱ�䵽, ����ע��.
        {
            nHeartbit = 0;
            DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
            for (; itSup != m_mapSuperior.end(); itSup ++)
            {
                if (itSup->second->bReg)
                {
                    itSup->second->bReg = false;
                    SetDestAddr(itSup->second->pDeviceUri, &addr);
                    char szReg[DATA_BUFSIZE] = {0};
                    string sTo = itSup->second->pDeviceUri;
                    string sProxy = GetProxy(sTo);
                    xsip_register_build_initial(szReg, sProxy.c_str(), sTo.c_str());
                    DWORD dwLen = (DWORD)strlen(szReg);
                    szReg[dwLen] = '\0';
                    m_pSIPProtocol->sendBuf(addr, szReg, dwLen);
                }
            }
        }
        else
        {
            DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
            for (; itSup != m_mapSuperior.end(); itSup ++)
            {
                if (itSup->second->bReg)
                {
                    if(itSup->second->nNoAnsKeepalive >= KEEPALIVENOANS)     //���ϼ��������������޻�Ӧ, ������ע��
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "���ϼ�[%s]����������Ϣ�޻�Ӧ����[%d], ����ע��.", itSup->second->pDeviceUri, KEEPALIVENOANS);

                        itSup->second->bReg = false;
                        char szReg[DATA_BUFSIZE] = {0};
                        string sTo = itSup->second->pDeviceUri;
                        string sProxy = GetProxy(sTo);
                        xsip_register_build_initial(szReg, sProxy.c_str(), sTo.c_str());
                        DWORD dwLen = (DWORD)strlen(szReg);
                        szReg[dwLen] = '\0';
                        m_pSIPProtocol->sendBuf(addr, szReg, dwLen);

                        itSup->second->nNoAnsKeepalive = 0;
                    }
                    else                                                  //���������������
                    {
                        memset(&addr, 0, sizeof(SOCKADDR_IN));
                        memset(szStatus, 0, DATA_BUFSIZE);
                        SetDestAddr(itSup->second->pDeviceUri, &addr);

                        string sID = xsip_call_id_init(xsip_get_CallIDNum(), m_sHost.c_str());
                        xsip_message_build_request(szStatus, "MESSAGE", NULL, itSup->second->pDeviceUri,
                            NULL, sID.c_str(), 2, "", xsip_get_tag());
                        xsip_set_content_type(szStatus, "Application/MANSCDP+xml");
                        xsip_set_body(szStatus, szBody, nBodylen);
                        nLen = (int)strlen(szStatus);
                        szStatus[nLen] = '\0';
                        m_pSIPProtocol->sendBuf(addr, szStatus, nLen);

                        itSup->second->nNoAnsKeepalive ++;
                    }                               
                }
                else                                      //��δע��ɹ�����ע��
                {
                    char szReg[DATA_BUFSIZE] = {0};
                    SetDestAddr(itSup->second->pDeviceUri, &addr);
                    string sTo = itSup->second->pDeviceUri;
                    string sProxy = GetProxy(sTo);
                    xsip_register_build_initial(szReg, sProxy.c_str(), sTo.c_str());
                    DWORD dwLen = (DWORD)strlen(szReg);
                    szReg[dwLen] = '\0';
                    m_pSIPProtocol->sendBuf(addr, szReg, dwLen);

                    itSup->second->nNoAnsKeepalive = 0;
                }
            }
        }
        
        nHeartbit += 1;
        Sleep(30 * 1000);
    }
    return 0;
}
int CSIPServer::SuperiorKeepaliveAns(SOCKADDR_IN &addr, char *msg)
{
    string sToUri = xsip_get_to_url(msg);
    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
    for (; itSup != m_mapSuperior.end(); itSup ++)
    {
        if(itSup->second->pDeviceUri == sToUri)
        {
            itSup->second->nNoAnsKeepalive = 0;
            break;
        }
    }
    return 0;
}
DWORD WINAPI CSIPServer::DeviceStatusThread(LPVOID lpParam)
{
	CSIPServer* pSip = (CSIPServer*)lpParam;
	pSip->CheckDeviceStatus();
	return 0;
}

void CSIPServer::CheckDeviceStatus(void)
{
    //return;

    Sleep(1000 * 15);     //15S��ʼÿСʱһ�εļ���豸״̬
    int nTimeOut = 200;   //�豸ע��/������ʱʱ��(S), Ĭ��200s
	vector<string> UserClearCode;
	
	while(WAIT_OBJECT_0 != WaitForSingleObject(m_hStopEvent, 0))
	{
		UserClearCode.clear();
		EnterCriticalSection(&m_vLock);
		DVRINFOMAP::iterator itDev = m_mapDevice.begin();
		if (m_mapDevice.size() > 0)
		{
			//printf("\n>>>>>>>>>>>>>>>>>>>>����豸״̬��Ϣ<<<<<<<<<<<<<<<<<<<<\n");
			for (; itDev != m_mapDevice.end(); itDev++)
			{
				time_t tCurrentTime;
				tCurrentTime = time(&tCurrentTime);
				//printf("���ڼ���豸��Ϣ[%s]\n", m_mapDevice[i].addr.c_str());
				//printf("ע��ʱ��:%s", ctime(&(m_mapDevice[i].tRegistTime)));
				//printf("��ǰʱ��:%s", ctime(&tCurrentTime));
				//printf("��ǰ�����ϴ�ע��ʱ����:%d\n", tCurrentTime - m_mapDevice[i].tRegistTime);
				//*******�ж����û������豸*********
				string sDevID = itDev->second->pDeviceCode;
				//**********************************
				if (itDev->second->bReg && (tCurrentTime - itDev->second->tRegistTime > nTimeOut))
				{					
					itDev->second->bReg = false;                    

					if (itDev->second->nNetType != 4 && itDev->second->nNetType != 7 && (IsCamera(sDevID) || IsDevice(sDevID)))
					{
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,"�豸[%s]ע�ᳬʱ.", itDev->second->pDeviceUri);
						if (!UpdateDeviceStatus(itDev->second->pDeviceUri, false))
						{
							g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�����豸����״̬ʧ��[%s].", itDev->second->pDeviceUri);
						}
						else
						{
							g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�����豸����״̬�ɹ�[%s][ON->OFF].", itDev->second->pDeviceUri);
						}
					}
                    else if(IsPlatform(sDevID))
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,"�¼�ƽ̨[%s]ע�ᳬʱ.", itDev->second->pDeviceUri);
                        if (!UpdatePlatformStatus(itDev->second->pDeviceUri, false))
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "����ƽ̨�����豸����״̬ʧ��[%s]\n", itDev->second->pDeviceUri);
                        }
                        else
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "����ƽ̨�����豸����״̬�ɹ�[%s:ON->OFF]\n", itDev->second->pDeviceUri);
                        }
                    }
					else if (IsUser(sDevID))
					{
                        UserClearCode.push_back(sDevID);
                        itDev->second->bReg = false;
					}
					else
					{
						continue;
					}
				}                               
			}
			//printf(">>>>>>>>>>>>>>>>>>>>������<<<<<<<<<<<<<<<<<<<<\n\n");
		}
		LeaveCriticalSection(&m_vLock);
		
		vector<string>::iterator userIt = UserClearCode.begin();
		while(userIt != UserClearCode.end())
		{
			printf("�û�[%s]ע�ᳬʱ.\n", (*userIt).c_str());
			ClearUserStream((char*)(*userIt).c_str());
			userIt++;
		}
		UserClearCode.clear();
		Sleep(1000 * 300);
	}	
    return;
}



DWORD WINAPI CSIPServer::AlarmThread(LPVOID lpParm)
{
	CSIPServer *pSip = (CSIPServer *)lpParm;
	SOCKADDR_IN sockin = {0};
	char szAlarm[DATA_BUFSIZE] = {0};
	char szBody[MAX_CONTENT_LEN] = {0};
	DWORD dwLen = 0;
	EnterCriticalSection(&pSip->m_sLock);
	SessionMap::iterator iter = pSip->m_SessionMap.begin();
	for (; iter != pSip->m_SessionMap.end(); iter++)
	{
		if (iter->second.sTitle == "GUARD")
		{
			SOCKADDR_IN sockAddr = iter->second.sockAddr;
			string sTo = pSip->xsip_get_from_url((char *)iter->second.request.c_str());
			string sFrom = pSip->xsip_get_to_url((char *)iter->second.request.c_str());
			//pSip->SetDestAddr(iter->second.sAddr, &sockin);
			string sAlarmID = iter->second.sDeviceID;
			LeaveCriticalSection(&pSip->m_sLock);

			sAlarmID.replace(10, 3, "134");
			while (pSip->m_bAlarm)
			{
				memset(szAlarm, 0, DATA_BUFSIZE);
				memset(szBody, 0, MAX_CONTENT_LEN);
				string sTime = pSip->GetNowTime(1);
				pSip->xsip_message_build_request(szAlarm,"MESSAGE",sFrom.c_str(),sTo.c_str(),sFrom.c_str(),NULL,21,"",pSip->xsip_get_tag());
				sprintf_s(szBody, sizeof(szBody),
					"<?xml version=\"1.0\"?>\r\n"
					"<Notify>\r\n"
					"<CmdType>Alarm</CmdType>\r\n"
					"<SN>%d</SN>\r\n"
					"<DeviceID>%s</DeviceID>\r\n"
					"<AlarmPriority>4</AlarmPriority>\r\n"
					"<AlarmTime>%s</AlarmTime>\r\n"
					"<AlarmMethod>2</AlarmMethod>\r\n"
					"</Notify>\r\n",
					pSip->m_dwSn++, sAlarmID.c_str(), sTime.c_str());
				pSip->xsip_set_content_type(szAlarm, XSIP_APPLICATION_XML);
				pSip->xsip_set_body(szAlarm, szBody, (int)strlen(szBody));
				dwLen = (DWORD)strlen(szAlarm);
				szAlarm[dwLen] = '\0';
				pSip->m_pSIPProtocol->sendBuf(sockAddr, szAlarm, dwLen);
				Sleep(10000);
			}
			return 0;
		}
	}
	LeaveCriticalSection(&pSip->m_sLock);
	return 0;
}

DWORD CSIPServer::CheckRTPStreamThread(LPVOID lpParam)
{
    CSIPServer * pThis = (CSIPServer * )lpParam;
    pThis->CheckRTPStreamAction();
    return 0;
}
void CSIPServer::CheckRTPStreamAction()
{    
    char pSendMsg[] = "KeepRTPStreamAlive";
    do 
    {
        time_t tCurrentTime = time(&tCurrentTime);       
        RTPIT rtpit = m_pRTPManager->m_listRTPInfo.begin();
        for(; rtpit != m_pRTPManager->m_listRTPInfo.end(); rtpit++)
        {
            if((*rtpit)->nType == FILEMEDIASERVERTYPE)
            {
                continue;
            }

            if(tCurrentTime - (*rtpit)->tRepTime >= 8)
            {
                if((*rtpit)->bOnLine)
                {
                    g_LogRecorder.WriteErrorLogEx(__FUNCTION__, 
                        "****Error: RTP��ý��[%s]��Ӧ��, ״̬��Ϊ������!",  (*rtpit)->sRTPUri.c_str());

                    (*rtpit)->bOnLine = false;

                    //����RTP��ý��ת���������Ϣ                   
                    printf("-------------------------------------\n");
                    g_LogRecorder.WriteErrorLogEx(__FUNCTION__,
                        "***Warning: ����RTP��ý��[%s]���ת������Ϣ.", (*rtpit)->sRTPUri.c_str());
                    SOCKADDR_IN sockin = {0};
                    char szBye[DATA_BUFSIZE] = {0};

                    while(true)
                    {
                        PLAYINFO PlayInfo;
                        if(!m_pRTPManager->GetPlayInfo((*rtpit)->sRTPID, &PlayInfo))
                        {
                            break;
                        }
                        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1, PlayInfo.sCallID4);
                        SendByeMsg(PlayInfo);
                    }

                    printf("-------------------------------------\n");
                }
            }

            SOCKADDR_IN addrStream = {0};
            SetDestAddr((*rtpit)->sRTPUri, &addrStream);
            m_pSIPProtocol->sendBuf(addrStream, pSendMsg, strlen(pSendMsg));
        }
    } while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 2000));  //ÿ2S���һ��״̬
}
bool CSIPServer::SetRTPServerStatus(SOCKADDR_IN &addr, char *msg)
{
    string sIP = inet_ntoa(addr.sin_addr);
    int nPort = ntohs(addr.sin_port);
    RTPIT rtpit = m_pRTPManager->m_listRTPInfo.begin();
    for(; rtpit != m_pRTPManager->m_listRTPInfo.end(); rtpit++)
    {
        if(sIP == (*rtpit)->sRTPIP && nPort == (*rtpit)->nRTPPort)
        {
            if(!(*rtpit)->bOnLine)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
                    "RTP��ý��[%s]��Ӧ��, ״̬��Ϊ����!", (*rtpit)->sRTPUri.c_str());
                (*rtpit)->bOnLine = true;
            }
            time(&(*rtpit)->tRepTime);
        }      
    }
    return true;
}
DWORD CSIPServer::CheckTimeThread(LPVOID lpParam)
{
    CSIPServer * pThis = (CSIPServer * )lpParam;
    pThis->CheckTimeAction();
    return 0;
}
void CSIPServer::CheckTimeAction()
{
    do 
    {
        //����¼���ѯʱ�����ڶ�ʧ��¼�������� ���û����ȷ��ɾ��m_SessionMap�ﱣ�������
        time_t tCurrentTime;
        tCurrentTime = time(&tCurrentTime);

        EnterCriticalSection(&m_sLock);
        SessionMap::iterator itsm = m_SessionMap.begin();
        while(itsm != m_SessionMap.end())
        {
            if((itsm->second.sTitle == "RECORDINFO" || itsm->second.sTitle == "CATALOGSUB")  && tCurrentTime - itsm->second.recvTime > 60 * 20)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�Ự[%s]��ʱTitle[%s]ɾ��.", itsm->first.c_str(), itsm->second.sTitle.c_str());
                itsm = m_SessionMap.erase(itsm);
            }
            else
            {
                itsm ++;
            }
        }

        LeaveCriticalSection(&m_sLock);       
    } while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 300 * 1000));  //ÿ5min���һ��״̬
}

bool CSIPServer::ClearUserStream(char * sDevID)
{
    printf("--------------------------------------\n");
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�����û�[%s]��Դ.", sDevID);

    SOCKADDR_IN sockin = {0};
    INVITE_TYPE nType = XSIP_INVITE_PLAY;
    char szBye[DATA_BUFSIZE] = {0};
    PLAYINFO PlayInfo;
    while(m_pRTPManager->GetPlayInfoByID(sDevID, false, &PlayInfo))
    {
        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1);
        SendByeMsg(PlayInfo, false);
    }   
    printf("--------------------------------------\n");
    return true;
}

DWORD CSIPServer::CheckIsInitialThread(LPVOID lpParam)
{
    CSIPServer * pThis = (CSIPServer * )lpParam;
    pThis->CheckIsInitialAction();
    return 0;
}
void CSIPServer::CheckIsInitialAction()
{
    GetInitialCamera();

    char szPTZ[1024*8] = {0};
    string sCmd;
    char szPTZCmd[17] = {0};
    long bit[8] = {0x00};
    bit[0] = 0xA5;
    bit[1] = 0x0F;
    bit[2] = 0x01;

    sCmd = "GOTO";
    bit[3] = CMD_PTZ_GOTO;
    bit[5] = (long)1;
    bit[7] = (bit[0]+bit[1]+bit[2]+bit[3]+bit[4]+bit[5]+bit[6])%0x100;
    sprintf_s(szPTZCmd,sizeof(szPTZCmd),"A50F%02X%02X%02X%02X%02X%02X",bit[2],bit[3],bit[4],bit[5],bit[6],bit[7]);

    while(WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 10 * 1000))
    {
        EnterCriticalSection(&m_csInitial);

        time_t tCurrentTime = time(&tCurrentTime);
        map<string, INITIALCAMERAINFO>::iterator it = m_mapPTZInitial.begin();
        while(it != m_mapPTZInitial.end())
        {
            if(it->second.tLastControlTime != 0  && tCurrentTime - it->second.tLastControlTime > m_pam.m_nInitialTime)
            {
                SOCKADDR_IN saddr;
                saddr.sin_family = AF_INET;
                saddr.sin_addr.S_un.S_addr = inet_addr((char *)it->second.sLowServerIP.c_str());
                saddr.sin_port = htons(it->second.nLowServerPort);

                string sTo = xsip_to_init((char*)it->first.c_str(), (char*)it->second.sLowServerIP.c_str(), it->second.nLowServerPort);
                xsip_message_build_request(szPTZ,"MESSAGE",NULL,sTo.c_str());
                xsip_set_monitor_user_identity(szPTZ, "ptz");
                char szBody[MAX_CONTENT_LEN] = {0};
                sprintf_s(szBody, sizeof(szBody),
                    "<?xml version=\"1.0\"?>\r\n"
                    "<Control>\r\n"
                    "<CmdType>DeviceControl</CmdType>\r\n"
                    "<SN>51</SN>\r\n"
                    "<DeviceID>%s</DeviceID>\r\n"
                    "<PTZCmd>%s</PTZCmd>\r\n"
                    "<Info>\r\n"
                    "<ControlPriority>%d</ControlPriority>\r\n"
                    "</Info>\r\n"
                    "</Control>\r\n",
                    it->first.c_str(), szPTZCmd, 5);
                xsip_set_content_type(szPTZ, XSIP_APPLICATION_XML);
                xsip_set_body(szPTZ, szBody, (int)strlen(szBody));
                DWORD dwLen = (DWORD)strlen(szPTZ);
                szPTZ[dwLen] = '\0';
                m_pSIPProtocol->sendBuf(saddr, szPTZ, dwLen);
                printf("�ص�������ID[%s]\n", it->first.c_str());
                it = m_mapPTZInitial.erase(it);
            }
            else
            {
                it ++;
            }
        }
        LeaveCriticalSection(&m_csInitial);
    }


}
bool CSIPServer::GetInitialCamera()
{
    char szSql[1024] = {0};
    sprintf_s(szSql, sizeof(szSql), "select CHANNO from xy_base.channelInfo where ISINITIAL = 1");
    otl_stream otlSelect;
    if(!m_DBMgr.ExecuteSQL(szSql, otlSelect))
        return false;

    time_t tCurrentTime = time(&tCurrentTime);

    printf("\n=============================================\n");
    printf("��Ҫ�����ľ�ͷ:\r\n");
    while(!otlSelect.eof())
    {
        char sCameraCode[21] = {0};
        try
        {
            otlSelect >> sCameraCode;     
        }
        catch(otl_exception& p)
        {
            char sError[2048] = {0};
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "�������ݿ�ʧ��!>������Ϣ��%s", p.msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "������룺%d", p.code);
            return false;
        }
        INITIALCAMERAINFO  InitialCameraInfo;
        InitialCameraInfo.tLastControlTime = tCurrentTime;

        string sLowServerUrl = GetDeviceAddr(sCameraCode);
        if(sLowServerUrl == "")
        {
            continue;
        }
        InitialCameraInfo.sLowServerIP = xsip_get_url_host((char *)sLowServerUrl.c_str());
        InitialCameraInfo.nLowServerPort = xsip_get_url_port((char *)sLowServerUrl.c_str());

        EnterCriticalSection(&m_csInitial);
        m_mapInitialCamera.insert(make_pair(sCameraCode, InitialCameraInfo));
        LeaveCriticalSection(&m_csInitial);

        printf("\t%s\n", sCameraCode);
    }
    printf("=============================================\n");
    return true;
}
DWORD CSIPServer::ReadDBThread(LPVOID lpParam)
{
    CSIPServer * pThis = (CSIPServer * )lpParam;
    pThis->ReadDBAction();
    return 0;
}
void CSIPServer::ReadDBAction()
{ 
    while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 1200 * 1000))//ÿ1200S���»�ȡһ��
    {
        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "=======���¶�ȡ���ݿ��ȡ�������豸�����Ϣ!=======");

        EnterCriticalSection(&m_vLock);

        Init(true);

        
        LeaveCriticalSection(&m_vLock);
        g_LogRecorder.WriteInfoLog(__FUNCTION__, "=======���¶�ȡ���ݿ��ȡ�������豸�����Ϣ���!=======");
    }  
}
string CSIPServer::ChangeSecondToTime(int nSecond)
{
    time_t ctime = nSecond;
    tm *tTime = localtime(&ctime);
    char sTime[20];
    sprintf_s(sTime, 20, "%04d-%02d-%02d %02d:%02d:%02d", tTime->tm_year+1900, tTime->tm_mon+1, tTime->tm_mday,
        tTime->tm_hour, tTime->tm_min, tTime->tm_sec);
    string RetTime = sTime;
    return sTime;
}
int CSIPServer::ChangeTimeToSecond(string sTime)
{
    tm tTime;
    try{
        sscanf_s(sTime.c_str(), "%d-%d-%d %d:%d:%d", &(tTime.tm_year), &(tTime.tm_mon), &(tTime.tm_mday),
            &(tTime.tm_hour), &(tTime.tm_min), &(tTime.tm_sec));
    }catch(...)
    {
        return -3;
    }

    tTime.tm_year -= 1900;
    tTime.tm_mon -= 1;
    int nBeginTime = (int)mktime(&tTime);
    if(nBeginTime == -1)
    {
        return -3;
    }

    return nBeginTime;
}