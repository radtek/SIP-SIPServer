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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "读取配置文件时参数错误!");
        }
        return false;
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "读取配置文件成功!");
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : 连接数据库失败!");
            return false;
        }

        if(m_pam.m_sLGDBServer != "")
        {
            m_LGDBMgr.SetConnectString(m_pam.m_nDBDriver, m_pam.m_sLGDBServer, m_pam.m_sLGDBName, m_pam.m_sLGDBUid, m_pam.m_sLGDBPwd);
            if (!m_LGDBMgr.ConnectDB())
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error : 连接龙岗数据库失败!");
                return false;
            }
        }

        if(!GetServerInfo())   //查找SIPSERVERINFO表获取sip服务相关信息
        {
            return false;
        }
        if(!GetRTPStreamInfo())     //从数据库表SIPServerInfo中取得当前SIP服务所对应的流媒体相关信息
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
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning:  更新设备指向SIP服务信息失败!");
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Info: 强制指定下级SIP服务[%s].", m_pam.m_sAssignLowServer.c_str());
        SetAssignRTPServer(m_pam.m_sAssignRTPServer);
    }


    //上级服务信息
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "...增加上级[%s].", pRegInfo->pDeviceUri);
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "...删除上级[%s].", itSup->second->pDeviceUri);
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
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "RTP流媒体接收流类型[%s].", sNetType.c_str());

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
       g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: SIP服务初始化失败!");
   }
    
    if(!m_pSIPProtocol->init(m_sSIPServerIP, m_nSIPServerPort, ParseMessage))
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "服务启动失败!");
        return false;
    }

    CreateThread(NULL, 0, ReadDBThread, this, 0, NULL);      //定时读取数据库更新服务信息
    CreateThread(NULL, 0, DeviceStatusThread, this, 0, NULL);  //创建线程检测设备|用户在线状态, 目前用来检测客户端在线状态.
    CreateThread(NULL, 0, CheckTimeThread, this, 0, NULL);  //创建线程检测录像查询, 目录查询冗余信息.
    CreateThread(NULL, 0, CheckRTPStreamThread, this, 0, NULL); //创建RTP流媒体状态检测线程。
    CreateThread(NULL, 0, SaveCatalogInfoThread, this, 0, NULL); //创建存储下级设备|平台镜头目录线程。
    CreateThread(NULL, 0, SuperiorKeepaliveThread, this, 0, NULL);	//开启与上级平台的心跳                       

    if(m_pam.m_bISINITIAL)
    {
        //CreateThread(NULL, 0, CheckIsInitialThread, this, 0, NULL);  //守望线程
    }

    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n======== SIP服务启动成功[%s@%s:%d] ========\n\n",  
        m_sSIPServerID.c_str(), m_pSIPProtocol->m_sLocalIP.c_str(), m_pSIPProtocol->m_nLocalPort);

    if (m_bRegSupServer)	//是否要向上级平台注册
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
                printf("启动时间: %s\n", pTime);
                EnterCriticalSection(&m_vLock);
                printf("-------------------------\n下级设备|平台列表:\n");
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
                printf("-------------------------\n注册用户列表:\n");
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
                printf("-------------------------\n播放信息:\n");
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

                printf("-------------------------\n\n其余会话信息:\n");
                EnterCriticalSection(&m_sLock);
                SessionMap::iterator itsm = m_SessionMap.begin();
                while(itsm != m_SessionMap.end())
                {
                    printf("\t镜头%02d[%s], Title[%s], CallID[%s], 发起者[%s]\n", nNum,
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
                    cout << "输入下级平台或设备编码: ";
                    cin >> pDeviceID;
                    if(pDeviceID[0] == 'b' || pDeviceID[0] == 'B')
                    {
                        break;
                    }
                    if(strlen(pDeviceID) != 20)
                    {
                        cout << "输入编号错误!" << endl;
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
                            g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "在ChanGroupInfo中创建镜头组[%s]失败.", pDeviceID);
                        }
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向[%s]发送目录推送请求..", itDev->second->pDeviceUri);
                        SendCatalogMsg(sockin, itDev->second->pDeviceUri);

                    }
                }
                LeaveCriticalSection(&m_vLock);
            }
            break;
        case 'r': case 'R':
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^^^重新读取服务|设备信息...");
            GetServerInfo();
            GetRTPStreamInfo();
            GetDeviceInfo();
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^^^读取结束.");
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
    case XSIP_REGSUPSERVER_SUCCESS:	case XSIP_REGSUPSERVER_FAILURE:	//向上级 注册/注销 的回应消息处理
	{
		SuperiorRegAnswer(addr, msg);
		break;
	}
    
	case XSIP_SUBSCRIPTION_NEW:			//收到上级平台目录订阅消息
	{
        SuperiorSubscribeAnswer(addr, msg);
		break;
	}
    case XSIP_DEVICE_REG:   //下级发送的请求 注册|注销 消息
    {
        EnterCriticalSection(&m_csLock);
        DeviceRegAnswer(addr, msg);
        LeaveCriticalSection(&m_csLock);
        break;
    }
    case XSIP_SUBSCRIPTION_ANSWERED:	//订阅成功
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向下级平台|设备订阅目录成功!");
        break;
    }
    case XSIP_SUBSCRIPTION_FAILURE:		//订阅失败
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向下级平台|设备订阅目录失败!");
        break;
    }
    case XSIP_SUBSCRIPTION_NOTIFY:		//向下级平台发送目录订阅消息后, 收到的目录镜头信息处理
    {
        DeviceNotifyAnswer(addr, msg);	//保存镜头目录更新消息到xy_base.sipsubscribe表
        break;
    }	   
	case XSIP_CALL_INVITE:		//收到客户端|上级平台播放镜头请求
	{
        EnterCriticalSection(&m_csLock);
		InviteMsgRecv(addr, msg);		//收到Invite消息处理
        LeaveCriticalSection(&m_csLock);
        break;
	}
	case XSIP_CALL_INVITEANSWERED:    
	{
        EnterCriticalSection(&m_csLock);
        InviteAnswerRecv(addr, msg);	//发送Invite消息给流媒体或设备后, 回应的消息处理
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#CancelAns: 取消镜头播放请求成功!");
		break;
	}
	case XSIP_CANCEL_FAILURE:
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#CancelAns: 取消镜头播放请求失败!");
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
	case XSIP_MESSAGE_NEW:				/***********************MESSAGE消息*************************************************************************************************/
	{
		xsip_message_type nMsg = xsip_get_message_type(msg);
		switch (nMsg)
		{
        case XSIP_MESSAGE_CATALOG_QUERY:	//收到上级平台目录查询消息
        {
            SuperiorCatalogAnswer(addr, msg);
            break;
        }
        case XSIP_MESSAGE_AREADETECTION_QUERY: case XSIP_MESSAGE_CROSSDETECTION_QUERY:	//收到上级平台区域侦测消息
        {
            DeviceSmartMsgRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_AREADETECTION_RESPONSE: case XSIP_MESSAGE_CROSSDETECTION_RESPONSE:	//收到上级平台目录查询消息
        {
            DeviceSmartAnswerRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_RECVSMART:
        {
            UserRecvSmartMsgRecv(addr, msg);
            break;
        }
        case XSIP_MESSAGE_CATALOG_RESPONSE:								//设备目录查询回应
            {
                DeviceCatalogAnswer(addr, msg);
                break;
            }
		case XSIP_MESSAGE_KEEPALIVE:								//收到心跳消息
			{
				DeviceKeepaliveRecv(addr, msg);
				break;
			}			
		case XSIP_MESSAGE_BOOT_CMD:									//设备远程重启
			{
				RebootMsgRecv(addr, msg);
				break;
			}					
		case XSIP_MESSAGE_DVRINFO_QUERY:								//设备信息查询
			{
				DeviceInfoMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRINFO_RESPONSE:								//设备信息回应
			{
				DeviceInfoAnswerRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRSTATUS_QUERY:								//设备状态查询
			{
				DeviceStatusMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_DVRSTATUS_RESPONSE:							//设备状态回应
			{
				DeviceStatusAnswerRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_GUARD_CMD:									//布防/撤防消息
			{
				DeviceGuardMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM:										//报警通知消息
			{
				DeviceAlarmMsgRecv(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM_RESPONSE:										//报警通知回应
			{
				CommandResponse(addr, msg);
				break;
			}
		case XSIP_MESSAGE_ALARM_CMD:											//报警复位消息
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
	case XSIP_MESSAGE_ANSWERED:									//正确的MESSAGE回应消息
	{
        SuperiorKeepaliveAns(addr, msg);
		break;
	}
	case XSIP_MESSAGE_FAILURE:									//错误的MESSAGE消息
	{
		xsip_message_type nMsg = xsip_get_message_type(msg);
		if (nMsg == XSIP_MESSAGE_KEEPALIVE_RESPHONSE)		//心跳返回失败
		{
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到403 Forbidden信令.");
			//SuperiorKeepaliveFailed(addr, msg);
		}
		else
		{
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "Warning: 未知的信令!");
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
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Warning: SIP服务编码信息[%s@%s:%d][%s:%d]错误!", 
                sServerCode, sPoliceNetIP, nPoliceNetPort, sExteranlIP, nExteranlPort);
            continue;
        }
        if(string(sPoliceNetIP) == "" || nPoliceNetPort == 0 || string(sParentCode) == "" || string(sServerCode) == "")
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: SIP服务数据错误[%s@%s:%d], Parent[%s].", 
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
            if(itServer->second.sPoliceNetIP == itServer->second.sExteranlIP)  //如公安网IP==专网IP, 说明此服务并没有映射到公安网, 只是数据库强制非空, 复制的专网IP
            {
                itServer->second.sPoliceNetIP = "";
                itServer->second.nPoliceNetPort = 0;
            }
        }
        else
        {
            continue;
        }

        //将下级平台信息加入到设备检测列表, 用以检测状态        
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
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "Error: 下级平台[%s]无匹配IP, 无法通信.", itServer->second.sSIPServerCode.c_str());
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
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "查找SIPSERVERIFNO表获取SIP服务相关信息完毕");
    //printf("-------------------------------------------------------------------------------\n");

    if(m_nServerID == 0)
    {
        if(!GetLocalServerID(m_sSIPServerID.c_str()))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 取本地服务[%s]ServerInfo表ID失败!", m_sSIPServerID.c_str());
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
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: RTP流媒体编码信息[%s@%s:%d]错误!", 
                strServerCode.c_str(), sStreamIP, nStreamPort);
            continue;
        }
        else if("200" == strServerCode.substr(10, 3))
        {
            
        }
        else if("202" == strServerCode.substr(10, 3) || "224" == strServerCode.substr(10, 3))
        {
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "当前SIP服务关联RTP流媒体[%s@%s:%s].",
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
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "Error : 当前SIP[%s]服务没有分配RTP流媒体.", m_sSIPServerID.c_str());
        return false;
    }

    m_pRTPManager->EraseRTPInfo(tCurrent);  //清理原本属于SIP服务, 后不再属于的RTP服务.
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

    //读取镜头信息
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "己获取镜头目录数[%d]...", nNum);
        }
    }
    otlSelect.close();
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "己获取镜头目录总数[%d]...", nNum);
    LeaveCriticalSection(&m_vLock);

    //读取设备信息(有些对接设备或平台此时尚无镜头信息, 必须在读取后, 在设备或平台注册后向其发送目录推送信息
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

        if(itDev != m_mapDevice.end())      //此设备在上面己被读取到, 有镜头信息
        {
            if(pDVRInfo->nDVRType == 8)
            {
                strcpy_s(pDVRInfo->pDVRKey, sizeof(pDVRInfo->pDVRKey), "IMOS");
            }
        }
        else                                //此设备尚未读取, 无镜头信息
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "获取设备信息完毕!",
            m_sSIPServerID.c_str(), m_sSIPServerIP.c_str(), m_nSIPServerPort);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "设备总数: %d.\r\n设备最后更新时间[%s]", m_mapDevice.size(), m_sUpdateTime.c_str());
    }
    
    return true;
}
bool CSIPServer::GetLGDeviceInfo(bool bUpdate)
{
    if(!bUpdate)
    {
        printf("获取龙岗内部监控设备信息...\n");
    }
    int nCount = 0;

    char szSql[2048] = {0};
    char pAddr[512] = {0}; 

    //读取镜头信息
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
        printf("获取龙岗内部监控信息完毕, 镜头数[%d].\n", nCount);
    }
    LeaveCriticalSection(&m_vLock);
    return true;
}
bool CSIPServer::GetLGPrivateNetworkDeviceInfo(bool bUpdate)
{
    if(!bUpdate)
    {
        printf("获取龙岗专网镜头信息...\n");
    }
    int nCount = 0;

    char szSql[2048] = {0};
    char pAddr[512] = {0}; 

    //读取镜头信息
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
        printf("获取龙岗专网镜头信息完毕, 镜头数[%d].\n", nCount);
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
        "<RegDevice>%d</RegDevice>\r\n"         //所管理设备|平台数量
        "<RTPNum>%d</RTPNum>\r\n"               //RTP流媒体数量
        "<GetStream>%d</GetStream>\r\n"           //当前转发视频流数量
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

    //不需要认证
    if (!m_bAuth)										
    {
        xsip_message_build_answer(msg, 200, answer, xsip_get_tag());
        string sTime = GetNowTime(3);
        xsip_set_date(answer, sTime.c_str());
        xsip_set_expires(answer, expires);

        m_pSIPProtocol->sendBuf(addr, answer, (DWORD)strlen(answer));
        if (expires != 0)								//注册成功，保存下级服务地址
        {            
            EnterCriticalSection(&m_vLock);
            DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
            if (itDev != m_mapDevice.end())
            {
                itDev->second->bReg = true;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户|设备[%s]注册成功.", sDevID.c_str());
                LPDVRINFO pReginfo = new DVRINFO;
                strcpy_s(pReginfo->pDeviceUri, sizeof(pReginfo->pDeviceUri), sDevID.c_str());
                pReginfo->bReg = true;
                m_mapDevice.insert(make_pair(sDevID, pReginfo));
            }
            LeaveCriticalSection(&m_vLock);
        }
        else											//注销成功，删除下级服务地址
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户|设备[%s]注销成功.", sDevID.c_str());
            EnterCriticalSection(&m_vLock);
            DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
            if (itDev != m_mapDevice.end())
            {
                itDev->second->bReg = false;
            }
            LeaveCriticalSection(&m_vLock);
        }
        return 0;
    }													//需要认证，获取认证参数

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
            if (expires != 0)							//注册成功保存下级服务地址
            {
                EnterCriticalSection(&m_vLock);
                DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
                if (itDev != m_mapDevice.end())
                {                      
                    if (IsDevice(sDevID) || IsCamera(sDevID))
                    {
                        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "Reg: 设备[%s]注册成功.", sDevUri.c_str());
                        //如果需要查询设备目录(服务启动后第一次), 或者设备由不在线转为在线状态且配置文件需要查询设备目录时
                        if(itDev->second->bSendCatalog || (m_pam.m_nSearchDevice && !itDev->second->bReg))  
                        {
                            itDev->second->bSendCatalog = false;
                            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "向设备[%s]查询目录...", sDevUri.c_str());
                            SendCatalogMsg(addr, sDevUri);
                        }
                        if (m_bSub)
                        {
                            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "向设备[%s]发送目录订阅消息...", sDevUri.c_str());
                            SendSubscribeMsg(addr, msg);										//订阅目录
                        }
                    }
                    else if (IsPlatform(sDevID))
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Reg: 下级平台[%s]注册成功.", sDevUri.c_str());
                        //如果需要查询平台目录(服务启动后第一次), 或者平台由不在线转为在线状态且配置文件需要查询平台目录时
                        if(itDev->second->bSendCatalog || (m_pam.m_nSearchPlat && !itDev->second->bReg))
                        {
                            itDev->second->bSendCatalog = false;
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向下级平台[%s]查询目录...", sDevUri.c_str());
                            if(!CreateChanGroupInfo(sDevID))
                            {
                                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "在ChanGroupInfo中创建镜头组[%s]失败.", sDevID.c_str());
                            }
                            SendCatalogMsg(addr, sDevUri);
                        }
                        if (m_bSub)
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向下级平台[%s]发送目录订阅消息...", sDevUri.c_str());
                            SendSubscribeMsg(addr, msg);										//订阅目录
                        }
                    }

                    itDev->second->bReg = true;
                    itDev->second->tRegistTime = time(&itDev->second->tRegistTime);//保存注册时间
                }
                else
                {
                    if(IsUser(sDevID))
                    {
                        LPDVRINFO pRegInfo = new DVRINFO;
                        strcpy_s(pRegInfo->pDeviceUri, sizeof(pRegInfo->pDeviceUri), sDevUri.c_str());
                        pRegInfo->bReg = true;
                        pRegInfo->tRegistTime = time(&pRegInfo->tRegistTime);

                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Reg: 用户[%s]注册成功.", sDevUri.c_str());
                        m_mapDevice.insert(make_pair(sDevID, pRegInfo));   //只有用户才加入列表, 其余设备和平台信息在启动时读数据库直接加入                       
                    }
                }
                LeaveCriticalSection(&m_vLock);				
            }
            else										//注销成功删除下级地址
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户|设备|下级平台[%s]注销成功.", sDevUri.c_str());
                EnterCriticalSection(&m_vLock);
                DVRINFOMAP::iterator itDev = m_mapDevice.find(sDevID);
                if (itDev != m_mapDevice.end())
                {
                    if (IsUser(sDevID))
                    {
                        itDev->second->bReg = false;
                        ClearUserStream((char*)sDevID.c_str());
                        //如果是用户则判断用户是否锁定云台，如果锁定则解锁
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>用户认证信息错误!<\n\n");
            xsip_message_build_answer(msg, 400, answer, xsip_get_tag());
            xsip_set_expires(answer, expires);
            len = (DWORD)strlen(answer);
            m_pSIPProtocol->sendBuf(addr, answer, len);
        }
    }													
    else        //没有认证参数返回 401 
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

    //查询是否己存在此镜头组
    sprintf_s(sSql, sizeof(sSql), "select ParentID from xy_base.changroupInfo where CopyParentID = '%s'", sDeviceID.c_str());
    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
    {
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询changroupInfo表是否存在镜头组[%s]出错.", sDeviceID.c_str());
        return false;
    }
    if(!otlSelect.eof())     //镜头组己存在
    {        
        if(sParentID != "")  //如父ID不为空, 则更新父结点信息
        {
            int nDeviceParentID = -1;
            otlSelect >> nDeviceParentID;
            otlSelect.close();

            sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询changroupInfo表镜头组[%s]信息出错.", sParentID.c_str());
                return false;
            }
            if(!otlSelect.eof())
            {
                int nParentID = -1;
                otlSelect >> nParentID;
                otlSelect.close();
                if(nParentID == nDeviceParentID)     //相同则不用更新
                {
                    
                }
                else
                {
                    sprintf_s(sSql, sizeof(sSql), "Update xy_base.changroupInfo set ParentID = %d, NodeName = '%s' where CopyParentID = '%s'",
                                                    nParentID, sDeviceName.c_str(), sDeviceID.c_str());
                    if(!m_DBMgr.ExecuteSQL(sSql, otlSelect, true))
                        return false;
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新镜头组[%s][%s]信息, 父结点[%s][ID = %d].", 
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新镜头组[%s][%s]信息, 父结点[%s][ID = %d].", 
                    sDeviceName.c_str(), sDeviceID.c_str(), sParentID.c_str(), nPID);
                otlSelect.close();
            }
            

        }
        return true;
    }
    otlSelect.close();

    //获取镜头组父结点ID
    int nParentID = -1;
    if(sParentID != "")
    {
        //sParentID不为空则直接查找此结点ID
        sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询changroupInfo表镜头组[%s]信息出错.", sParentID.c_str());
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
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Warning: 查找changroupInfo表[%s][%s]父镜头组[%s]无记录.", sDeviceName.c_str(), sDeviceID.c_str(), sParentID.c_str());
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "创建父结点[%s].", sParentID.c_str());

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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "创建父镜头组[%s]成功.", sParentID.c_str());
            }
            otlSelect.close();

            sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupInfo where CopyParentID = '%s'", sParentID.c_str());
            if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询changroupInfo表镜头组[%s]信息出错.", sParentID.c_str());
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
        //sParentID为空则根据国标20位编码规则查找相应父结点
        char pParentCode[32] = "";
        ZeroMemory(sSql, sizeof(sSql));
        sprintf_s(sSql, sizeof(sSql), 
            "select ID, CopyParentID from xy_base.changroupInfo where CopyParentid != '-1'  order by id");
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询changroupInfo表ID, CopyParentID出错.");
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: 在changroupInfo表未查找到平台[%s]对应父镜头组.", sDeviceID.c_str());
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
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询sipdvr表[%s] name出错.", sDeviceID.c_str());
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "创建镜头组[%s][%s]成功.", sDeviceName.c_str(), sDeviceID.c_str());
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
	//中心用户
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
	//平台
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
    //平台
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

	//获取设备状态  
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

    //获取设备状态
    string sStatus = bStatus ? "ON" : "OFF";

    char szExc[1024] = {0};
    char szSql[1024] = {0};
    bool bDevicePlat = false;   //True: 下级别司平台, 当设备处理; False:下级本司平台
    
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
    if(m_pam.m_sAssignLowServer != "")     //查看配置文件中是否己指定转发下级SIP服务
    {
        string sServerIP = xsip_get_url_host((char*)m_pam.m_sAssignLowServer.c_str());
        int nServerPort = xsip_get_url_port((char*)m_pam.m_sAssignLowServer.c_str());
        if(sServerIP == "" || nServerPort == 0)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "指定下级SIP服务信息错误[%s].", m_pam.m_sAssignLowServer.c_str());
            return "";
        }
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "找到指定下级SIP服务[%s@%s:%d].", sID, sServerIP.c_str(), nServerPort);

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
        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "镜头[%s@%s:%d]找到关联sip服务[%s].", 
            sID, pDVRInfo->pDVRIP, nDVRPort, pDVRInfo->pSIPServerCode);

        if(!pChannelInfo->bOnline && InviteType == XSIP_INVITE_PLAY)
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 镜头[%s@%s:%d]不在线.", 
                sID, pDVRInfo->pDVRIP, nDVRPort);
            return "";
        }

        if(string(pDVRInfo->pSIPServerCode) == m_sSIPServerID)   //比较关联SIP服务是否为当前SIP服务
		{
            if(pCameraInfo != NULL)
            {
                pCameraInfo->nNetType = pDVRInfo->nNetType;
            }

            if(XSIP_INVITE_PLAY != InviteType && m_pam.m_bFileStream)    //有文件流媒体时, 查找对应文件流媒体地址.
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
            case 4: case 7:     //sdk连接设备取流
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
		else  //否则, 则说明所属SIP服务为当前SIP服务的一个子服务或子子服务或更子, 具体的从属关系需要再查找
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
                        "***Warning: 当前播放镜头[%s]不属于当前SIP[%s]服务!", 
                        sID, m_sSIPServerID.c_str());
					break;
				}
				if(m_mapSIPServer[sParentSIPCode].sParentCode == m_sSIPServerID)
				{
					bFindParent = true;
					g_LogRecorder.WriteInfoLogEx(__FUNCTION__, 
                        "找到当前SIP服务[%s]下级SIP服务[%s].", m_sSIPServerID.c_str(), 
                        sParentSIPCode.c_str());
					break;
				}

                if(sParentSIPCode == m_mapSIPServer[sParentSIPCode].sParentCode)
                {
                    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, 
                        "***Warning: SIP[%s]的上级服务为自己本身!\n请检查客户端配置文件, 更改SIP服务相关信息!\n", 
                        sParentSIPCode.c_str());
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__,
                        "***Warning: 镜头[%s]关联SIP服务[%s]与当前SIP服务[%s]无关联!", 
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
            else      //当前播放镜头非当前SIP服务下级, 则通过IP判断是否可直接通信
			{               
				g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "****Warning: 镜头[%s]没有找到当前转发下级SIP服务!", sID);

                if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[pDVRInfo->pSIPServerCode].sExteranlIP))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "==直接向SIP服务[%s]请求镜头[%s]视频...", pDVRInfo->pSIPServerCode, sID);
                    return xsip_to_init(sID, (const char *)m_mapSIPServer[pDVRInfo->pSIPServerCode].sExteranlIP.c_str(), m_mapSIPServer[pDVRInfo->pSIPServerCode].nExteranlPort);
                }
                else if(IsSameNet(m_sSIPServerIP, m_mapSIPServer[pDVRInfo->pSIPServerCode].sPoliceNetIP))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "==直接向SIP服务[%s]请求镜头[%s]视频...", pDVRInfo->pSIPServerCode, sID);
                    return xsip_to_init(sID, (const char *)m_mapSIPServer[pDVRInfo->pSIPServerCode].sPoliceNetIP.c_str(),
                        m_mapSIPServer[pDVRInfo->pSIPServerCode].nPoliceNetPort);
                }
                else 
                {                   
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__,
                        "**Warning: 无法直接通信请求设备[%s]视频, 所属SIP服务[%s]", sID, pDVRInfo->pSIPServerCode);
                    return "";
                }
			}
		}
	}
   
	else
	{
		g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 镜头[%s]不存在或未分配SIP服务!", sID);
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
        sscanf_s(time, "%d-%d-%d %d:%d:%d", &nYear,&nMon,&nDay,&nHour,&nMin,&nSec);  //32位环境
    }else
    {
	
        sscanf_s(time, "%d/%d/%d %d:%d:%d", &nYear,&nMon,&nDay,&nHour,&nMin,&nSec);   //64位环境
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
int CSIPServer::SuperiorRegAnswer(SOCKADDR_IN addr, char * sRegAnswer)            //向上级注册的回应消息
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
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向上级服务[%s]注册成功!", itSup->second->pDeviceUri);
                        }
                        else
                        {
                            SetLocalTime(sTime);
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向上级服务[%s]注册成功.\n同步校时为:%s", itSup->second->pDeviceUri,sTime.c_str());
                        }
                    }	
                }
                else
                {
                    m_bRegisterOut = false;
                    if (itSup->second->bReg == true)
                    {
                        itSup->second->bReg = false;
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向上级服务[%s]注销成功!", itSup->second->pDeviceUri);

                        //重注册
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
        if (string(sRegAnswer).find("401") != string::npos)	//需要加密
        {
            string sRealm = xsip_get_www_authenticate_realm(sRegAnswer);
            string sNonce = xsip_get_www_authenticate_nonce(sRegAnswer);
            if (sRealm == "" || sNonce == "")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "注册失败,Realm=\"%s\",sNonce=\"%s\"!<\n\n", sRealm.c_str(), sNonce.c_str());
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
            if (!m_bRegisterOut)				//带认证重新注册
            {
                xsip_register_build_initial(szReg, sProxy.c_str(), sUri.c_str(), callid.c_str(), 2, ftag);
            }
            else								//带认证重新注销
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>向上级[%s]注册失败,检查上级服务!<\n\n", sUri.c_str());
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "!>向上级[%s]注销失败,检查上级服务!<\n\n", sUri.c_str());
            }
        }
    }

    
    return 0;
}

int CSIPServer::RebootMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到设备重启消息!");
    bool bSup = false;
    string sAddr = xsip_get_via_addr(msg);
    DVRINFOMAP::iterator itSup = m_mapSuperior.begin();
    for (; itSup != m_mapSuperior.end(); itSup++)
    {
        if (sAddr == itSup->second->pDeviceUri)		//上级的设备重启命令，则重注册
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
    if (!bSup)					//上级的设备重启命令，转发下级
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
            m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//转发下级
        }
    }
	return 0;
}



int CSIPServer::DeviceKeepaliveRecv(SOCKADDR_IN &addr, char *msg)
{
    string sBody = xsip_get_body(msg);
    if (sBody == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Warning: Keepalive消息没有XML信息!");
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
	m_SessionMap.insert(SessionMap::value_type(szSN, Info));	//保存会话信息
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
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复400 BAD
        return -1;
    }

    EnterCriticalSection(&m_sLock);
    SessionMap::iterator iter = m_SessionMap.find(sSN);
    if (iter != m_SessionMap.end())
    {
        xsip_message_build_answer(msg, 200, szAns, iter->second.dwTTag);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);				//回复200 OK

        if (iter->second.sTitle == "CATALOGSUB")				//目录接收
        {
            LPCATALOGINFO pCatalogInfo = new CATALOGINFO;
            memcpy(&pCatalogInfo->addr, &addr, sizeof(SOCKADDR_IN));
            pCatalogInfo->sBody = sBody;
            pCatalogInfo->sDeviceID = sDeviceID;
            m_listCatalogInfo.push_back(pCatalogInfo);      //将镜头目录保存到链表

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
                    //g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "己接收镜头目录[%d].", iter->second.nNum);
                }
                if (iter->second.nNum >= atoi(sSumNum.c_str()))
                {					
                    LPCATALOGINFO pCatalogInfo = new CATALOGINFO;
                    memcpy(&pCatalogInfo->addr, &addr, sizeof(SOCKADDR_IN));
                    pCatalogInfo->sBody = "End";
                    pCatalogInfo->sDeviceID = sDeviceID;
                    m_listCatalogInfo.push_back(pCatalogInfo);

                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "接收镜头目录完毕[%d]!", iter->second.nNum);
                    iter = m_SessionMap.erase(iter);

                }
            }
            else if(sSumNum == "0")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: 设备无镜头目录!");
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "剩余%d镜头目录信息处理...", m_listCatalogInfo.size());
            }
            if((*itCatalog)->sBody == "End")
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "平台|设备[%s]镜头目录加载到数据库完毕..", (*itCatalog)->sDeviceID.c_str());
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "开始清理平台|设备[%s]冗余镜头目录..", (*itCatalog)->sDeviceID.c_str());
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
            "^^^^Error: 查询Overtime镜头sipchannel表出错[%s].", sTime.c_str());
        return false;
    }
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "清理平台|设备[%s]镜头目录, SQL[%s].", sDeviceID.c_str(), sSql);

    char pCameraID[32] = {0};
    char pName[128] = {0};
    while(!otlSelect.eof())
    {
        otlSelect >> pCameraID >> pName;
        DelCameraInfo(pCameraID);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Del Overtime Channel[%s][%s]", pCameraID, pName);
    }
    otlSelect.close();
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "清理平台|设备[%s]冗余镜头目录结束.", sDeviceID.c_str());
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

        if(string(pBuf).find("<Event>") != string::npos)    //HK平台Notify通知消息操作类型
        {
            string sCatalogType = ReConver(pItem->FirstChildElement("Event"));
            nCatalogType = sCatalogType == "DEL" ? DELCATALOG : ADDCATALOG;       //1增加镜头, 2删除
            if(DELCATALOG == nCatalogType)
            {
                if(!DelCameraInfo(sDeviceID))
                {
                    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: 从数据库删除镜头[%s]信息失败.", sDeviceID.c_str());
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
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "在ChanGroupInfo中创建镜头组[%s : %s]失败.", sDeviceID.c_str(), sName.c_str());
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
        //从信令里获取的IP, Port没有意义
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
			g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "取目录信息, 镜头ID为空.");
            continue;
		}

        if(sStatus == "")
        {
            sStatus = "OFF";
        }

        /**************将设备信息写入数据库*************/
		char sSql[4096] = {0};
        otl_stream otlSelect;
        bool bExist = false;
        char pUpdateTime[32] = {0};
        SYSTEMTIME sysTime;
        GetLocalTime(&sysTime);
        sprintf_s(pUpdateTime, sizeof(pUpdateTime), "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear, sysTime.wMonth, sysTime.wDay, 
            sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

        //判断sDeviceID是否平台|设备(DVR, NVR)类型, 是则说明当前信息为虚拟组织|设备结点, 而非镜头, 则创建镜头分组
        if(IsPlatform(sDeviceID) || IsDevice(sDeviceID) || IsChanGroup(sDeviceID) || sDeviceID.size() <= 10)
        {
            if(!CreateChanGroupInfo(sDeviceID, sName, sParentID))
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "在ChanGroupInfo中创建镜头组[%s : %s]失败.", sDeviceID.c_str(), sName.c_str());
            }
            continue;
        }
        
        //查找镜头对应DVR ID
        int nDvrID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from %s where dvrip = '%s'", m_sDVRInfoTable.c_str(), sIPAdderss.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 获取dvrinfo表DVR[%s] ID出错.", sIPAdderss.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> nDvrID;
        }
        otlSelect.close();
        if(nDvrID == 0 )
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 根据DVRIP[%s]未找到DVRInfo记录.", sIPAdderss.c_str());
            return false;
        }

        //查找channelInfo表中是否存在此镜头
        bExist = false;
        int nChanID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from %s where Channo = '%s' and ID >= 10000000 ", m_sChannelInfoTable.c_str(), sDeviceID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询镜头[%s]目录是否己存在channelinfo表出错.", sDeviceID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            bExist = true;     //镜头目录己存在.
            otlSelect >> nChanID;
        }
        otlSelect.close();

        if(!bExist) //不存在, 则插入镜头
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
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询镜头[%s]ID, 执行SQL语句失败.", sDeviceID.c_str());
                return false;
            }
            if(!otlSelect.eof())
            {
                otlSelect >> nChanID;
            }
            else
            {
                g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 未获取到镜头[%s]ID.", sDeviceID.c_str());
                return false;
            }   
            otlSelect.close();

            //插入镜头增加信息到XY_MONITOR.DATACHANGE, 通知索引服务

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


        //sipChannel表中是否存在此镜头        
        bExist = false;
        sprintf_s(sSql, sizeof(sSql), "select * from %s where ID = %d", m_sSIPChannelTable.c_str(), nChanID);
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "^^^^Error: 查询镜头ID[%d]是否己存在sipchannel表出错.", nChanID);
            return false;
        }
        while(!otlSelect.eof())
        {
            bExist = true;     //镜头目录己存在.
            break;
        }
        otlSelect.close();
        if(!bExist)     //不存在则插入
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
		else         //存在则更新镜头信息
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
       
        //changrouptochan表中是否存在此镜头
        bExist = false;
        sprintf_s(sSql, sizeof(sSql), "select * from %s where NodeNo = '%s'", m_sChanGroupToChanTable.c_str(), sDeviceID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 查询镜头[%s]是否己存在changrouptochan表出错.", sDeviceID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            bExist = true;     //镜头己存在.
        }
        otlSelect.close();

        int nChanGroupID = 0;
        sprintf_s(sSql, sizeof(sSql), "select ID from xy_base.changroupinfo where CopyParentID = '%s'", sParentID.c_str());
        if(!m_DBMgr.ExecuteSQL(sSql, otlSelect))
        {
            g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "^^^^Error: 获取changroupinfo表[%s]ID出错.", sParentID.c_str());
            return false;
        }
        if(!otlSelect.eof())
        {
            otlSelect >> nChanGroupID;
        }
        else
        {
            otlSelect.close();
            g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "^^^^Warning: 查找changroupinfo表[%s]无记录.", sParentID.c_str());
            continue;
        }
        otlSelect.close();

        if(!bExist)     //插入镜头到changrouptochan
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
		
        /**************将设备信息写入数据库完毕*************/
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

    //没有找到sDeviceID, HIK下级平台推送的Notify消息, DeviceID为其平台下的设备|结点ID, 而不是平台自身ID
    if(itDev == m_mapDevice.end())
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "通知消息来源[%s@%s:%d]无下级平台|设备匹配, 忽略...", 
            sDeviceID.c_str(), sDeviceIP.c_str(), nDevicePort);
        return 0;
    }

    SaveSubscribe(addr, sBody.c_str(), sDeviceID.c_str()); //保存目录信息
  
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
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复 400
        return -1;
    }
    xsip_message_build_answer(msg,200,szAns,dwToTag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//正确回复 200

    /***泉州项目, SIP服务共用数据库, 镜头目录不用在相互间发来发去, 故这里不再创建发送目录线程**/
    //return 0;

    if (sID == m_sSIPServerID)	//镜头推送，创建发送线程
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

        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "上级平台[%s]请求目录查询!", sinfo.sAddr.c_str());
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
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "读取SIPChannel表获取镜头目录数据失败!");
		return -1;
	}

    //厚街分局
    /*{
        char pNode[1024] = {0};
        sprintf_s(pNode, sizeof(pNode), 
            "<Item>\r\n"
            "<DeviceID>%s</DeviceID>\r\n"
            "<Name>厚街分局</Name>\r\n"
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
        if(isPtz == 0)//枪机
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
			"<Name>%s</Name>\r\n"					//名称
			"<Manufacturer>%s</Manufacturer>\r\n"	//厂商
			"<Model>%s</Model>\r\n"					//型号
			"<Owner>%s</Owner>\r\n"					//归属
			"<CivilCode>%s</CivilCode>\r\n"			//行政区域(可选)
			"<Block>%s</Block>\r\n"					//警区(可选)
			"<Address>%s</Address>\r\n"				//安装地址
			"<Parental>%d</Parental>\r\n"			//是否有子设备:1有0没有
			"<ParentID>%s</ParentID>\r\n"			//父ID(可选)
			"<SafetyWay>0</SafetyWay>\r\n"			//信令安全模式(可选):0不采用
			"<RegisterWay>1</RegisterWay>\r\n"		//注册模式:1符合标准认证
			"<Secrecy>0</Secrecy>\r\n"				//保密属性:0不涉密1涉密
			"<IPAddress>%s</IPAddress>\r\n"			//设备IP地址(可选)
			"<Port>%d</Port>\r\n"					//设备端口(可选)
			"<Status>%s</Status>\r\n"				//设备状态:ON可用OFF不可用
			"<Longitude>%lf</Longitude>\r\n"		//经度(可选)
			"<Latitude>%lf</Latitude>\r\n"			//纬度(可选)
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
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "发送镜头目录完毕, 镜头数[%d], 己发送[%d].", nCount, nTime);

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

    /***泉州项目, SIP服务共用数据库, 镜头目录不用在相互间发来发去, 故这里不再创建发送目录线程**/
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
    m_SessionMap.insert(SessionMap::value_type(sCallID, Info));	//保存会话信息
    LeaveCriticalSection(&m_sLock);

    CreateThread(NULL, 0, SuperiorNotifyThread, this, 0, NULL);	    //创建线程发送镜头目录		
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "上级平台[%s]请求目录订阅!", Info.sAddr.c_str());
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
        g_LogRecorder.WriteErrorLogEx(__FUNCTION__, "****Error: 读取SIPChannel表获取镜头目录数据失败!");
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
            "<Name>%s</Name>\r\n"					//名称
            "<Manufacturer>%s</Manufacturer>\r\n"	//厂商
            "<Model>%s</Model>\r\n"					//型号
            "<Owner>%s</Owner>\r\n"					//归属
            "<CivilCode>%s</CivilCode>\r\n"			//行政区域(可选)
            "<Block>%s</Block>\r\n"					//警区(可选)
            "<Address>%s</Address>\r\n"				//安装地址
            "<Parental>%d</Parental>\r\n"			//是否有子设备:1有0没有
            "<ParentID>%s</ParentID>\r\n"			//父ID(可选)
            "<SafetyWay>0</SafetyWay>\r\n"			//信令安全模式(可选):0不采用
            "<RegisterWay>1</RegisterWay>\r\n"		//注册模式:1符合标准认证
            "<Secrecy>0</Secrecy>\r\n"				//保密属性:0不涉密1涉密
            "<IPAddress>%s</IPAddress>\r\n"			//设备IP地址(可选)
            "<Port>%d</Port>\r\n"					//设备端口(可选)
            "<Status>%s</Status>\r\n"				//设备状态:ON可用OFF不可用
            "<Longitude>%lf</Longitude>\r\n"		//经度(可选)
            "<Latitude>%lf</Latitude>\r\n"			//纬度(可选)
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
    m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);							//回复100 Tyring

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

    //判断是否重复会话.
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if (rep != REPNONE)
    {								//相同的会话标识;
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Repeat: 收到重复的Invite信令, CallID[%s].", sCallID.c_str());
       
        if (PlayInfo.sInvite1Answer != "")
        {   //如果会话己完成, 则可能回应的200 OK丢失, 重复发送
            SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);
            m_pSIPProtocol->sendBuf(sockin, (char*)PlayInfo.sInvite1Answer.c_str(), PlayInfo.sInvite1Answer.size());
        }
        else
        {
            //如果会话未完成, 则回应100 Trying, 同时继续等待回应.
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
			sCameraID = xsip_get_url_username((char *)sToUri.c_str());	//获得镜头ID			
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: 收到Invite实时预览请求, 镜头ID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());

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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: 收到Invite播放录像请求, 镜头ID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "1: 收到Invite下载录像请求, 镜头ID[%s], CallID[%s].", sCameraID.c_str(), sCallID.c_str());
			break;
		}
	default:
		{
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: Invite信令SDP解析错误!");
            return -1;
		}
	}
	
    CAMERAINFO CameraInfo;
    CameraInfo.nCameraType = 20;
	string sCameraUri = GetDeviceAddr(sCameraID.c_str(), nType, &CameraInfo);	//判断镜头是否存在且在线
	if (sCameraUri == "")
	{
		memset(szCallAns, 0, DATA_BUFSIZE);
		xsip_call_build_answer(msg, 400, szCallAns, dwToTag);
		dwLen = (DWORD)strlen(szCallAns);
		szCallAns[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);								//不存在则回复 400 Bad
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 播放镜头[%s]失败!", sCameraID.c_str());
        return 0;
	}

	string sDecodeUri = "";
	if (sMediaRecvID != "" && IsMonitor(sMediaRecvID))	//判断解码通道是否存在且在线
	{
		sDecodeUri = GetDeviceAddr(sMediaRecvID.c_str());
		if (sDecodeUri == "")
		{
			memset(szCallAns, 0, DATA_BUFSIZE);
			xsip_call_build_answer(msg, 400, szCallAns, dwToTag);
			dwLen = (DWORD)strlen(szCallAns);
			szCallAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);								//不存在则回复 400 Bad
			g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 解码器通道[%s]上墙失败.", sMediaRecvID.c_str());
            return 0;
		}
	}
		
    string sRTPServerID = "", sRTPServerIP = "";
    int nRTPServerPort;
    bool bTransmit = false;  //实时播放时是否可以直接转发
    if (!m_pRTPManager->GetPracticableRTPServer(msg, sCameraID, nType, sCallID, sRTPServerID, 
        sRTPServerIP, nRTPServerPort, bTransmit))
    {
        g_LogRecorder.WriteErrorLog(__FUNCTION__, "所有RTP服务都不在线!");
        return 0;
    }  
    string sRTPUri = xsip_to_init(sRTPServerID.c_str(), sRTPServerIP.c_str(), nRTPServerPort);
	SetDestAddr(sRTPUri, &sockin);

    string sSSRC = xsip_ssrc_init(nType, m_dwSSRC++); 
    if(m_dwSSRC >= 10000)
    {
        m_dwSSRC = 1;
    }
    g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "生成ssrc[%s], CallID[%s].", sSSRC.c_str(), sCallID.c_str());

    char pSubject[128] = {0};
    sprintf_s(pSubject, sizeof(pSubject), "%s:%s,%s:%d", sCameraID.c_str(), 
        sSSRC.c_str(), sMediaRecvID.c_str(), CameraInfo.nNetType);
    
    string sCallID2 = "", sCallID4 = "";
    char sInviteMsg2[DATA_BUFSIZE] = {0};
    char sInviteMsg4[DATA_BUFSIZE] = {0};
	if (bTransmit)  //本地媒体已经转发
	{
		if (sBody == "")
		{
			memset(szCallAns, 0, DATA_BUFSIZE);
			xsip_call_build_answer(msg, 488, szCallAns, dwToTag);
			dwLen = (DWORD)strlen(szCallAns);
			szCallAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szCallAns, dwLen);					//没有SDP信息则回复488 BAD
			EraseSessionMap(sCallID);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: Invite没有SDP信息!");
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

		m_pSIPProtocol->sendBuf(sockin, sInviteMsg4, dwLen);				//向媒体转发(带SDP)
 
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: 向RTP流媒体请求视频流, CallID[%s].", sCallID4.c_str());
	}
	else
	{
        if(CameraInfo.nNetType >= 4)
        {
            int nLoad;
            //客户端SDP信息
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

            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: 向RTP流媒体[%s]发送Invite发送流请求, CallID[%s].", 
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

		    m_pSIPProtocol->sendBuf(sockin, sInviteMsg2, dwLen);			//向媒体发送请求(无SDP)
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "2: 向RTP流媒体[%s]请求接收流地址, CallID[%s].",
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
    case REPRTPRECVSTREAM:    //RTP流媒体接收流回应
        {
            if(PlayInfo.sInvite2Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到重复Invite2 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushRTPRecvAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "3: 收到RTP流媒体请求接收流地址回应, CallID[%s].", sCallid.c_str());

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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "4: 向设备[%s]发送点播请求, CallID[%s], ssrc[%s].",
                PlayInfo.sCameraUri.c_str(), sCallID3.c_str(), PlayInfo.sSSRC.c_str());

        }
        break;
    case REPCAMERASEND:      //镜头发送流回应
        {
            if(PlayInfo.sInvite3Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到重复Invite3 Answer, CallID[%s].", sCallid.c_str());

                char szAck[DATA_BUFSIZE] = {0};
                xsip_call_build_ack(msg, szAck);
                DWORD dwLen = (DWORD)strlen(szAck);
                szAck[dwLen] = '\0';
                m_pSIPProtocol->sendBuf(addr, szAck, dwLen);	//回复镜头ACK
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "7: 向镜头[%s]发送ACK, CallID[%s].", 
                    PlayInfo.sCameraUri.c_str(), PlayInfo.sCallID3.c_str());
                return 0;
            }
            string sSSRC = xsip_get_sdp_y_ssrc(sSdp.c_str());
            m_pRTPManager->PushCameraAnswer(sCallid, msg, sSSRC);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "5: 收到请求镜头发送流回应, CallID[%s].", sCallid.c_str());

            char szAck[DATA_BUFSIZE] = {0};
            xsip_call_build_ack(msg, szAck);
            DWORD dwLen = (DWORD)strlen(szAck);
            szAck[dwLen] = '\0';
            m_pSIPProtocol->sendBuf(addr, szAck, dwLen);	//回复镜头ACK
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "7: 向镜头[%s]发送ACK, CallID[%s].", 
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
            m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);	//回复RTP流媒体ACK
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "6: 向RTP流媒体[%s]发送ACK, CallID[%s].",
                PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID2.c_str());
            Sleep(50);
            

            if (PlayInfo.sMediaRecverID != "" && IsMonitor(PlayInfo.sMediaRecverID))						//向解码器发送Invite请求
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

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向解码器通道[%s]发送Invite5信令, CallID[%s].", 
                   PlayInfo.sMediaRecvUri.c_str(), sCallID5.c_str()) ;
            }
            else
            {
                char szInvite[DATA_BUFSIZE] = {0};							//向RTP流媒体发送Invite发送流请求
                char szMamu[32] = {0};
                int nLoad;
                xsip_get_sdp_a_load(sSdp.c_str(), &nLoad, szMamu);
                //客户端SDP信息
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

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: 向RTP流媒体[%s]发送Invite发送流请求, CallID[%s].", 
                    PlayInfo.sRTPServerID.c_str(), sCallID4.c_str());
            }
        }
        
        break;
    case REPRTPSENDSTREAM:      //RTP流媒体发送流回应
        {
            if(PlayInfo.sInvite4Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到重复Invite4 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushRTPSendAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "9: 收到RTP流媒体发送流回应, CallID[%s].", sCallid.c_str());

            if (PlayInfo.sMediaRecverID != "" && IsMonitor(PlayInfo.sMediaRecverID))  //如解码器通道
            {
                string sTo = GetDeviceAddr(PlayInfo.sMediaRecverID.c_str());
                char szAck[DATA_BUFSIZE] = {0};
                xsip_call_build_ack((char*)PlayInfo.sInvite5Answer.c_str(), szAck);
                xsip_set_content_type(szAck, XSIP_APPLICATION_SDP);
                xsip_set_body(szAck, (char *)sSdp.c_str(), (int)sSdp.size());
                DWORD dwLen = (DWORD)strlen(szAck);
                szAck[dwLen] = '\0';
                SetDestAddr(sTo, &sockin);
                m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);		//向解码器通道发送ack		
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "CallID[%s]向解码器通道发送ack!", PlayInfo.sCallID5.c_str());

                char szAckRTP[DATA_BUFSIZE] = {0};
                xsip_call_build_ack((char*)PlayInfo.sInvite4Answer.c_str(), szAckRTP);
                m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);		//向RTP服务发送发送流ack	
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "CallID[%s] 向RTP服务发送发送流ack!", PlayInfo.sCallID4.c_str());
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

                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "10: 回应客户端播放请求, CallID[%s].", PlayInfo.sCallID1.c_str());
            }   
        }
        break;
    case REPDECODERRECV:      //解码器通道接收流回应
        {
            if(PlayInfo.sInvite5Answer != "")
            {
                g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到重复Invite5 Answer, CallID[%s].", sCallid.c_str());
                return 0;
            }
            m_pRTPManager->PushDecodeAnswer(sCallid, msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Decoder: CallID[%s]收到解码器通道接收流回应", PlayInfo.sCallID1.c_str());

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
            m_pSIPProtocol->sendBuf(sockin, szInvite, dwLen);		//向本地媒体服务再次Invite请求

            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "8: CallID[%s]向RTP流媒体[%s]发送Invite发送流请求.", 
                sCallID4.c_str(), sRTPUri.c_str());
        }
        break;
    default:
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: Invite回应消息无记录. CallID[%s].", sCallid.c_str());
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

        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "11：收到客户端ACK消息, CallID[%s].", sCallID.c_str());
        SOCKADDR_IN sockin = {0};
        string sRTPUri = xsip_to_init(PlayInfo.sRTPServerID.c_str(), PlayInfo.sRTPServerIP.c_str(), PlayInfo.nRTPServerPort);
        SetDestAddr(sRTPUri, &sockin);

        char szAck[DATA_BUFSIZE] = {0};
        xsip_call_build_ack((char*)PlayInfo.sInvite4Answer.c_str(), szAck);
        DWORD dwLen = (DWORD)strlen(szAck);
        szAck[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(sockin, szAck, dwLen);	//转发ACK给本地RTP流媒体
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "12: 向RTP流媒体发送ACK发送流消息, CallID[%s].", PlayInfo.sCallID4.c_str());
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "*Warning: 未知ACK消息, CallID[%s].", sCallID.c_str());
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n****Error: 点播镜头[%s]失败, RTP流媒体回应[%s], CallID[%s].",
            PlayInfo.sCameraID.c_str(), sAnswer.c_str(), sCallid.c_str());
    }
    else if(rep == REPCAMERASEND)
    {       
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "\n****Error: 点播镜头[%s]失败, 设备回应[%s], CallID[%s].",
            PlayInfo.sCameraID.c_str(), sAnswer.c_str(), sCallid.c_str());
        PlayInfo.sInvite3Answer = msg;
    }

    //向客户端或上级平台回应点播失败
    memset(&sockin, 0, sizeof(sockin));
    char szCallAns[1024] = {0};
    int nAnswer = atoi(sAnswer.c_str());
    xsip_call_build_answer((char*)PlayInfo.sInviteMsg1.c_str(), nAnswer, szCallAns);   
    SetDestAddr(PlayInfo.sMediaRecvUri, &sockin);
    m_pSIPProtocol->sendBuf(sockin, szCallAns, (DWORD)strlen(szCallAns));	
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Invite Failed: 向客户端发送播放失败消息, CallID[%s]!", PlayInfo.sCallID1.c_str());

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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "**收到客户端Cancel消息, CallID[%s] !", sCallID.c_str());

        SendByeMsg(PlayInfo);
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "**Warning: 收到无匹配Cancel消息, CallID[%s].", sCallID.c_str());
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "13: 收到客户端Bye消息, CallID[%s].", sCallID.c_str());
            if(!m_pRTPManager->GetRecvACK(sCallID))
            {
                g_LogRecorder.WriteWarnLogEx(__FUNCTION__, "***Warning: 未收到客户端ACK信令, CallID[%s].", sCallID.c_str());
            }
        }
        else if(rep == REPRTPSENDSTREAM)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: 收到RTP流媒体Bye消息_4, CallID[%s].", sCallID.c_str());
        }
        else if(rep == REPRTPRECVSTREAM)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: 收到RTP流媒体Bye消息_2, CallID[%s].", sCallID.c_str());
        }
        else if(rep == REPCAMERASEND)
        {
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "***Warning: 收到设备|平台Bye消息, CallID[%s].", sCallID.c_str());
        }

        bool bDeleteCam = (rep == REPRTPRECVSTREAM) ? true : false;
        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1, PlayInfo.sCallID4, bDeleteCam);

        char szBye[1024] = {0};
        xsip_bye_build_answer(msg, 200, szBye);
        m_pSIPProtocol->sendBuf(addr, szBye, strlen(szBye));        //回应客户端bye消息.

        SendByeMsg(PlayInfo, bDeleteCam);
        
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Warning: 收到无匹配Bye消息, CallID[%s].", sCallID.c_str());
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
            m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));	//向RTP流媒体发送接收流BYE
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "a: 向RTP流媒体[%s]发送Bye_2, CallID[%s]", PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID2.c_str());
        }
        if(PlayInfo.sInviteMsg3 != "")
        {
            if(PlayInfo.sInviteMsg4 != "") //如已经向RTP流媒体发送过Invite4, 说明与镜头会话己完成, 需要发送Bye而不是Cancel
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInvite3Answer.c_str(), szBye, false, PlayInfo.nCamCseq);
                
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//向镜头发送BYE
            }
            else if(PlayInfo.sInvite3Answer != "")
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInvite3Answer.c_str(), szBye, false, PlayInfo.nCamCseq);
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//向镜头发送Cancel
            }
            else
            {
                ZeroMemory(szBye, sizeof(szBye));
                xsip_call_build_bye((char*)PlayInfo.sInviteMsg3.c_str(), szBye, true, PlayInfo.nCamCseq);
                SetDestAddr(PlayInfo.sCameraUri.c_str(), &sockin);
                m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));			//向镜头发送Cancel
            }
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "b: 向镜头[%s]发送Bye, CallID[%s].",PlayInfo.sCameraID.c_str(), PlayInfo.sCallID3.c_str());
        }

    }

    if(PlayInfo.sInviteMsg4 != "")
    {
        ZeroMemory(szBye, sizeof(szBye));
        xsip_call_build_bye((char*)PlayInfo.sInviteMsg4.c_str(), szBye);
        SetDestAddr(PlayInfo.sRTPUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));		//向RTP流媒体发送发送流BYE
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "c: 向RTP流媒体[%s]发送Bye_4, CallID[%s].", PlayInfo.sRTPServerID.c_str(), PlayInfo.sCallID4.c_str());
    }
    if(PlayInfo.sInviteMsg5 != "")
    {
        ZeroMemory(szBye, sizeof(szBye));
        xsip_call_build_bye((char*)PlayInfo.sInviteMsg5.c_str(), szBye);
        SetDestAddr(PlayInfo.sMediaRecvUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));		
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向解码器通道[%s]发送Bye, CallID[%s].", PlayInfo.sMediaRecvUri.c_str(), PlayInfo.sCallID5.c_str());
    }

    if(PlayInfo.rep != REPCLIENT)
    {
        ZeroMemory(szBye, sizeof(szBye));
        string sMsg = PlayInfo.sInvite1Answer == "" ? PlayInfo.sInviteMsg1 : PlayInfo.sInvite1Answer;
        xsip_call_build_bye((char*)sMsg.c_str(), szBye, false, PlayInfo.nCamCseq);
        SetDestAddr(PlayInfo.sMediaRecvUri.c_str(), &sockin);
        m_pSIPProtocol->sendBuf(sockin, szBye, (DWORD)strlen(szBye));
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "d: 向客户端[%s]发送Bye, CallID[%s].", 
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#RTP流媒体关闭流成功, CallID[%s]", sCallid.c_str());
    }
    else if(sTo_Code.substr(10, 3) == "131" || sTo_Code.substr(10, 3) == "132")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#镜头关闭视频流成功, CallID[%s]", sCallid.c_str());
    }
    else if(sTo_Code.substr(10, 3) == "133")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "#解码器通道关闭成功, CallID[%s]", sCallid.c_str());
    }
    return 0;  
}

int CSIPServer::PTZMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	//获取DeviceID
	string sDeviceID = xsip_get_uri(msg, "<DeviceID>", "</DeviceID>", MAX_USERNAME_LEN);
	//判断镜头编码是否有误
    PLAYINFO PlayInfo;
    if(!m_pRTPManager->GetPlayInfoByID(sDeviceID, false, &PlayInfo))
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: 镜头[%s]未播放, PTZ控制失败.", sDeviceID.c_str());
	    return -1;
	}
    string sAddr = PlayInfo.sCameraUri;

	//获取UserID
	string sFromUrl = xsip_get_from_url(msg);
	size_t nPos1 = sFromUrl.find(":");
	size_t nPos2 = sFromUrl.find("@");
	string sUserID = sFromUrl.substr(nPos1 + 1, nPos2 - nPos1 - 1);
	//获取用户权限
	//int nUserLevel = GetUserLevel(sUserID);
	string sUserLevel = xsip_get_uri(msg, "<ControlPriority>", "</ControlPriority>", MAX_USERNAME_LEN);
	int nUserLevel = atoi(sUserLevel.c_str());
	//获取用户操作类型(加锁、解锁、控制)
	string sCmdType = xsip_get_uri(msg, "<PTZCmd>", "</PTZCmd>", MAX_USERNAME_LEN);
	int nMsgType = 0;//返回消息类型
	LockMap::iterator itMap = m_LockedDeviceMap.find(sDeviceID);
	if (sCmdType == "LOCK")
	{
		//锁定
		printf("用户[%s:%d]请求锁定镜头[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap == m_LockedDeviceMap.end())
		{
				nMsgType = PTZ_LOCK_SUCCESSED;//设备未被锁定,直接加锁返回200
		}
		else
		{
			if (sUserID == itMap->second.sUserID)
			{
				//针对平台情况需要判断
				if (nUserLevel > itMap->second.nUserLevel)
				{
					nMsgType = PTZ_LOCK_SUCCESSED;
				}
				else if (nUserLevel == itMap->second.nUserLevel)
				{
					nMsgType = PTZ_LOCK_FAILED_LOCKED;
					printf("当前用户已锁定过，无需再次锁定\n");
				}
				else
				{
					nMsgType = PTZ_LOCK_FAILED_LOW;
					printf("当前设备被高权限用户锁定，锁定失败\n");
				}
			}
			else
			{
				if (nUserLevel > itMap->second.nUserLevel)
				{
					itMap = m_LockedDeviceMap.erase(itMap);
					nMsgType = PTZ_LOCK_SUCCESSED;//当前用户权限较高，锁定并返回200
				}
				else
				{
					nMsgType = PTZ_LOCK_FAILED_LOW;//当前用户权限较低，锁定失败返回400
					printf("当前设备被高权限用户锁定，锁定失败\n");
				}
			}
		}	
	}
	else if (sCmdType == "UNLOCK")
	{
		//解锁
		printf("用户[%s:%d]请求解锁镜头[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap != m_LockedDeviceMap.end())
		{
			if (sUserID == itMap->second.sUserID && nUserLevel == itMap->second.nUserLevel)
			{
				m_LockedDeviceMap.erase(itMap);
				nMsgType = PTZ_UNLOCK_SUCCESSED;//解锁成功,返回200
			}
			else
			{
				nMsgType = PTZ_UNLOCK_FAILED;//用户未锁定，返回400
				printf("设备未被锁定，无需解锁\n");
			}
		}
		else
		{
			nMsgType = PTZ_UNLOCK_FAILED;//设备没有被锁定，返回400
			printf("设备未被锁定，无需解锁\n");
		}
	}
	else
	{
		//控制
		printf("用户[%s:%d]请求控制云台[%s]\n", sUserID.c_str(), nUserLevel, sDeviceID.c_str());
		if (itMap == m_LockedDeviceMap.end())
		{
			nMsgType = PTZ_CONTROL_SUCCESSED;//设备未被锁定，直接控制并返回200
		}
		else
		{
			if (sUserID == itMap->second.sUserID)
			{
				nMsgType = PTZ_CONTROL_SUCCESSED;//设备被当前用户锁定，直接控制并返回200
			}
			else
			{
				if (nUserLevel > itMap->second.nUserLevel)
				{
					nMsgType = PTZ_CONTROL_SUCCESSED;//当前用户权限较高，可操作设备，返回200
				}
				else
				{
					nMsgType = PTZ_CONTROL_FAILED;//当前用户权限较低，无法操作设备，返回400
					printf("设备被锁定，无法操作\n");
				}
			}
		}
	}

	//回应信息200
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}

	switch(nMsgType)
	{
	case PTZ_LOCK_SUCCESSED:    //锁定成功,返回200并将用户插入锁定列表
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
	case PTZ_UNLOCK_SUCCESSED:  //解锁成功,返回200
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg,200,szAns,dwttag);
			DWORD dwLen = (DWORD)strlen(szAns);
			szAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
		}
		break;
	case PTZ_CONTROL_SUCCESSED: //控制成功,返回200并向设备发送控制信息
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg,200,szAns,dwttag);
			DWORD dwLen = (DWORD)strlen(szAns);
			szAns[dwLen] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
			PTZControlSend(addr, msg, sAddr);
		}
		break;
	case PTZ_LOCK_FAILED_LOCKED://重复锁定,返回480
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 480, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_LOCK_FAILED_LOW:   //权限低，无法锁定，返回481
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 481, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_UNLOCK_FAILED:		//解锁失败,返回482
		{
			char szAns[DATA_BUFSIZE] = {0};
			xsip_message_build_answer(msg, 482, szAns, dwttag);
			DWORD nLenANS = (DWORD)strlen(szAns);
			szAns[nLenANS] = '\0';
			m_pSIPProtocol->sendBuf(addr, szAns, nLenANS);
		}
		break;
	case PTZ_CONTROL_FAILED:    //控制失败,返回483
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: 镜头[%s]停止播放, 控制失败.", sDeviceID.c_str());
    }
    if(PlayInfo.nCameraType != 20)     //走RTP流媒体PTZ控制, 信令发给RTP流媒体
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
        m_pSIPProtocol->sendBuf(sockin, szPTZ, dwLen);		//走SIP控制
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "----向RTP流媒体[%s]发送PTZ控制信令.", PlayInfo.sRTPUri.c_str());
    }
    else           //国标流程PTZ控制, 控制信令发给设备
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
		m_pSIPProtocol->sendBuf(sockin, szPTZ, dwLen);		//走SIP控制
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "----向设备[%s]发送PTZ控制信令.", sAddr.c_str());
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
                printf("添加新的守望ID[%s]\n", sDeviceID.c_str());
            }
        }
        else if(sCmdType == "UNINITIAL")
        {
            if(it != m_mapInitialCamera.end())
            {
                m_mapInitialCamera.erase(it);        
                printf("删除守望ID[%s]\n", sDeviceID.c_str());
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
            
            printf("更新云台控制时间, 镜头ID[%s]\n", sDeviceID.c_str());
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "未找到匹配录像回放控制, CallID[%s]!", sCallID.c_str());
        return -1;
    }

	string sBody = xsip_get_body(msg);
	if (sBody == "")
	{
		xsip_message_build_answer(msg, 400, szAnswer, dwttag);
		dwLen = (DWORD)strlen(szAnswer); 
		szAnswer[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szAnswer, dwLen);
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Info控制信息未携带sdp, CallID[%s] .", sCallID.c_str());
		return -1;
	}

	int nType = xsip_get_rtsp_type(sBody.c_str());
    string sType = nType == XSIP_RTSP_PAUSE ? "Pause" : (nType == XSIP_RTSP_REPLAY ? "Replay" : (nType == XSIP_RTSP_SCALE ? "Scale" : (nType == XSIP_RTSP_RANGEPLAY ? "Range" : "Teardown")));
	if (nType != XSIP_RTSP_TEARDOWN)
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到回放控制请求[%s].", sType.c_str());
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
        m_pSIPProtocol->sendBuf(sockin, szAnswer, dwLen);		//向流媒体服务转发
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
	    m_pSIPProtocol->sendBuf(sockin, szAnswer, dwLen);		//向设备转发
    }
    m_pRTPManager->AddPlaybackCseq(sCallID);

	return 0;
}
int CSIPServer::ReplayMediaStatusRecv(SOCKADDR_IN &addr, char *msg)
{
    char szAns[DATA_BUFSIZE] = {0};
    string sCallID = xsip_get_call_id(msg);
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到媒体流结束消息, CallID[%s].", sCallID.c_str());

    string sSdp = xsip_get_body(msg);
    PLAYINFO PlayInfo;
    RESPONSETYPE rep = m_pRTPManager->GetAnswerType(sCallID, &PlayInfo);
    if(rep == REPCAMERASEND)  //设备发送MediaStatus消息
    {
        xsip_message_build_answer(msg, 200, szAns);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);        //回复设备200 OK

        //向用户转发MediaStatus
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向用户[%s]转发MediaStatus消息, CallID[%s].", PlayInfo.sMediaRecverID.c_str(), PlayInfo.sCallID1.c_str());

        m_pRTPManager->ReduceRTPTotal(PlayInfo.sCallID1);
        SendByeMsg(PlayInfo);
    }
    else
    {
        xsip_message_build_answer(msg, 400, szAns);
        DWORD dwLen = (DWORD)strlen(szAns);
        szAns[dwLen] = '\0';
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: 设备MediaStatus信令无匹配记录, CallID[%s].", sCallID.c_str());
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
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//回复200OK

	string sDeviceID = xsip_get_to_url(msg);
	sDeviceID = xsip_get_url_username((char *)sDeviceID.c_str());

	string sAddr = GetDeviceAddr(sDeviceID.c_str());
    if(sAddr == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 镜头[%s]未找到相关联SIP信息.", sDeviceID.c_str());
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
	m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//转发给设备或下级平台

	return 0;
}

int CSIPServer::RecordInfoMsgRecv(SOCKADDR_IN &addr, char *msg)
{   
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, 
        "---------------------------------------------------\r\n收到录像文件检索消息.");

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
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "没有XML消息体!");
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
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复400 BAD
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "XML消息体不正确!");
		return -1;
	}
	xsip_message_build_answer(msg, 200, szAns, dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //回复200 OK

	char szRecord[DATA_BUFSIZE] ={0};
	string sDeviceID = sID;
	string sFrom = xsip_to_init(sDeviceID.c_str(),m_sSIPServerIP.c_str(),m_nSIPServerPort);

    CAMERAINFO CameraInfo;
	string sAddr = GetDeviceAddr(sDeviceID.c_str(), XSIP_INVITE_PLAYBACK, &CameraInfo);			//判断是否是本地镜头，否则还回上级地址
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "***Warning: 查找镜头[%s]录像信息, 查询地址失败!", sDeviceID.c_str());
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
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: 连接设备[%s:%s:%d]失败",sDVRType.c_str(),
                    CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort);
                return -1;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^连接设备[%s:%s:%d]成功.",sDVRType.c_str(),
                    CameraInfo.sDeviceIP.c_str(), CameraInfo.nDevicePort);
            }

            sST = sST.replace(10, 1, " ");
            sET = sET.replace(10, 1, " ");
            RECORDFILE pRecinfo[100];
            int nCount = 0;
            nCount = getRecordFileEx(nLoginId, 0, (char*)sST.c_str(), (char*)sET.c_str(), pRecinfo, 100);
            if (nCount < 0)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "****Warning: 取镜头[%s]录像信息失败[%s  %s].", 
                    sID.c_str(), sST.c_str(), sET.c_str());
                return -1;
            }
            else
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "^^取镜头[%s]录像信息成功.\n"
                                                            "**[%s  %s]返回数目[%d].", 
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

            //替换客户端发送过来的xml信息中sn字段           
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向设备[%s]发送录像查询请求......", sAddr.c_str());
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
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//SDP 错误 回复400 BAD
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
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//回复200 OK
	
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
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szResponse, dwLen);			//转发      
		
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
				g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "转发录像目录信息完毕[%d]!", iter->second.nNum);
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
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);	//回复400 BAD
		printf("没有找到匹配的会话标识!");
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户[%s]取消接收Smart报警消息!", sUserID.c_str());
        SmartUserMap::iterator SmartIt = m_mapSmartUser.find(sUserID);
        if(SmartIt != m_mapSmartUser.end())
        {
            m_mapSmartUser.erase(SmartIt);
        }
    }
    else if(sEnable == "true")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户[%s]开始接收Smart报警消息!", sUserID.c_str());
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "用户[%s]发送消息错误!", sUserID.c_str());
    }
    LeaveCriticalSection(&m_SmartLock);

    return 0;
}
int CSIPServer::DeviceSmartMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到设置设备Smart功能消息!");

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
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复400 BAD
        return -1;
    }
    xsip_message_build_answer(msg,200,szAns,dwToTag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);			//回复200 OK

    SOCKADDR_IN sockin = {0};
    char szDvrinfo[DATA_BUFSIZE] ={0};
    string sDeviceID = sID;
    string sAddr = GetDeviceAddr(sDeviceID.c_str()); //查找设备地址
    if (sAddr == "")
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 镜头[%s]未找到相关联信息.", sDeviceID.c_str());
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
        m_mapSmartUser.insert(make_pair(sUserID, SmartUser));       //如果用户设置了Smart功能, 则将此用户加入Smart报警通知列表
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
        m_pSIPProtocol->sendBuf(sockin, szDvrinfo, dwLen);		//向设备|下级转发
    }

    return 0;
}
int CSIPServer::DeviceSmartAnswerRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到设备Smart报警消息!");

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
        m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //回复400 BAD

        return 0;
    }
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "设备[%s]Smart报警消息, Time[%s].", sDeviceID.c_str(), sSmartTime.c_str());
    xsip_message_build_answer(msg, 200, szAns, dwttag);
    dwLen = (DWORD)strlen(szAns);
    szAns[dwLen] = '\0';
    m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//回复200 OK

    EnterCriticalSection(&m_SmartLock);
    SmartUserMap::iterator SmartIt = m_mapSmartUser.begin();
    for(; SmartIt != m_mapSmartUser.end(); SmartIt ++)          //收到的Smart报警信息, 转发给每一个接收报警信息的用户
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
        m_pSIPProtocol->sendBuf(sockAddr, szAns, (DWORD)strlen(szAns)); //转发给上级|用户
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
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到设备信息查询消息!");

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
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复400 BAD
		return -1;
	}
	xsip_message_build_answer(msg,200,szAns,dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);			//回复200 OK

	SOCKADDR_IN sockin = {0};
	char szDvrinfo[DATA_BUFSIZE] ={0};
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str()); //查找设备地址
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 镜头[%s]未找到相关联信息.", sDeviceID.c_str());
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
		m_SessionMap.insert(SessionMap::value_type(sSN, info));	//保存会话
		LeaveCriticalSection(&m_sLock);

		xsip_message_build_request(szDvrinfo,"MESSAGE",NULL,sAddr.c_str(),NULL,sCallID.c_str(),nCeq,dwFmTag);
		xsip_set_content_type(szDvrinfo, XSIP_APPLICATION_XML);
		xsip_set_body(szDvrinfo, (char *)sBody.c_str(), (int)sBody.size());
		dwLen = (DWORD)strlen(szDvrinfo);
		szDvrinfo[dwLen] = '\0';
		SetDestAddr(sAddr, &sockin);
		m_pSIPProtocol->sendBuf(sockin, szDvrinfo, dwLen);		//向设备|下级转发
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
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen); //回复400 BAD

        return 0;
	}
	xsip_message_build_answer(msg,200,szAns,dwttag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//回复200 OK

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
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szAns, (DWORD)strlen(szAns)); //转发给上级|用户
		iter = m_SessionMap.erase(iter);
	}
	else
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "设备信息查询回应, xml SN[%s]未找到对应会话信息.", sSN.c_str());
		LeaveCriticalSection(&m_sLock);
		return -1;
	}
	LeaveCriticalSection(&m_sLock);
	return 0;
}

int CSIPServer::DeviceStatusMsgRecv(SOCKADDR_IN &addr, char *msg)
{
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到设备状态查询消息!");

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
		m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//SDP 错误 回复400 BAD
		return -1;
	}
	xsip_message_build_answer(msg, 200, szAns, dwToTag);
	dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//回复200 0K

	SOCKADDR_IN sockin = {0};
	char szStatus[DATA_BUFSIZE] ={0};
	char szBody[MAX_CONTENT_LEN] = {0};
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str()); 
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 镜头[%s]未找到相关联信息.", sDeviceID.c_str());
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
	    m_pSIPProtocol->sendBuf(sockin, szStatus, dwLen);		//向上级|用户转发
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
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//回复400 BAD
	}
	EnterCriticalSection(&m_sLock);
	SessionMap::iterator iter = m_SessionMap.find(sSN);
	if (iter != m_SessionMap.end())
	{
		xsip_message_build_answer(msg, 200, szResponse, iter->second.dwTTag);
		DWORD dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//回复200 OK

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
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szResponse, dwLen);			//转发上级|用户

		iter = m_SessionMap.erase(iter);
	}
	else
	{
		xsip_message_build_answer(msg, 404, szResponse, sTag);
		DWORD dwLen = (DWORD)strlen(szResponse);
		szResponse[dwLen] = '\0';
		m_pSIPProtocol->sendBuf(addr, szResponse, dwLen);				//回复400 BAD
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
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);	//回复200OK

	int nceq = xsip_get_cseq_number(msg);
	string sSN = xsip_get_uri(msg,"<SN>","</SN>",10);
	string sID = xsip_get_uri(msg,"<DeviceID>","</DeviceID>", MAX_USERNAME_LEN);
	string sDeviceID = sID;
	string sAddr = GetDeviceAddr(sDeviceID.c_str());
	if (sAddr == "")
	{
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "Error: 设备[%s]未找到相关联SIP服务信息.", sDeviceID.c_str());
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
		m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);	//回复应答MESSAGE
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
	m_pSIPProtocol->sendBuf(addr, szMsg, dwLen);	//回复应答MESSAGE

	string sCmd = xsip_get_uri(msg, "<GuardCmd>","</GuardCmd>", 12);
	if (sCmd == "SetGuard")
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "镜头[%s]收到布防控制消息!", sID);
		SESSIONINFO info;
		info.sTitle = "GUARD";
		info.sAddr = sAddr;		//设备地址
		info.dwFTag = dwFmTag;
		info.dwTTag = dwToTag;
		info.sDeviceID = sID;
		info.request = msg;		
		memcpy(&info.sockAddr, &addr, sizeof(SOCKADDR_IN));	//客户端地址;

		EnterCriticalSection(&m_sLock);
		m_SessionMap.insert(SessionMap::value_type(sSN, info));
		LeaveCriticalSection(&m_sLock);
	}
	else if (sCmd == "ResetGuard")
	{
		g_LogRecorder.WriteDebugLogEx(__FUNCTION__,  "镜头[%s]收到撤防控制消息!", sID);
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
	m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//转发

	return 0;
}

int CSIPServer::DeviceAlarmMsgRecv(SOCKADDR_IN &addr, char *msg)
{
	g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到报警通知消息!");
	char szAns[DATA_BUFSIZE] = {0};
	string dwttag = xsip_get_to_tag(msg);
	if (dwttag == "")
	{
		dwttag = xsip_get_tag();
	}
	xsip_message_build_answer(msg,200,szAns,dwttag);
	DWORD dwLen = (DWORD)strlen(szAns);
	szAns[dwLen] = '\0';
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//回复200 OK

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
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//回复应答 MESSAGE

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
		m_pSIPProtocol->sendBuf(iter->second.sockAddr, szAns, (DWORD)strlen(szAns));		//转发客户端
	}
	else
	{
		//g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "无效的会话标识[%s]!<", sSN.c_str());
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
	g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "收到报警复位消息!");
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
	m_pSIPProtocol->sendBuf(addr, szAns, dwLen);		//回复200 OK

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
		string sAddr = iter->second.sAddr; //设备地址
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
			m_pSIPProtocol->sendBuf(sockin, szAns, (DWORD)strlen(szAns));		//转发
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
		//g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "无效的会话标识[%s]!<", sSN.c_str());
		iter = m_SessionMap.begin();
		for (;iter!=m_SessionMap.end();iter++)
		{
			if (iter->second.sTitle == "GUARD")
			{
				string sAddr = iter->second.sAddr; //设备地址
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
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到手动录像控制消息!");
    }
    else
    {
        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "收到录像关闭控制消息!");
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
        if (nHeartbit >= 60)                     //注册保活时间到, 重新注册.
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
                    if(itSup->second->nNoAnsKeepalive >= KEEPALIVENOANS)     //向上级发送心跳次数无回应, 则重新注册
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "向上级[%s]发送心跳信息无回应次数[%d], 重新注册.", itSup->second->pDeviceUri, KEEPALIVENOANS);

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
                    else                                                  //否则继续发送心跳
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
                else                                      //尚未注册成功则发送注册
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

    Sleep(1000 * 15);     //15S后开始每小时一次的检测设备状态
    int nTimeOut = 200;   //设备注册/心跳超时时间(S), 默认200s
	vector<string> UserClearCode;
	
	while(WAIT_OBJECT_0 != WaitForSingleObject(m_hStopEvent, 0))
	{
		UserClearCode.clear();
		EnterCriticalSection(&m_vLock);
		DVRINFOMAP::iterator itDev = m_mapDevice.begin();
		if (m_mapDevice.size() > 0)
		{
			//printf("\n>>>>>>>>>>>>>>>>>>>>检测设备状态信息<<<<<<<<<<<<<<<<<<<<\n");
			for (; itDev != m_mapDevice.end(); itDev++)
			{
				time_t tCurrentTime;
				tCurrentTime = time(&tCurrentTime);
				//printf("正在检测设备信息[%s]\n", m_mapDevice[i].addr.c_str());
				//printf("注册时间:%s", ctime(&(m_mapDevice[i].tRegistTime)));
				//printf("当前时间:%s", ctime(&tCurrentTime));
				//printf("当前距离上次注册时间间隔:%d\n", tCurrentTime - m_mapDevice[i].tRegistTime);
				//*******判断是用户还是设备*********
				string sDevID = itDev->second->pDeviceCode;
				//**********************************
				if (itDev->second->bReg && (tCurrentTime - itDev->second->tRegistTime > nTimeOut))
				{					
					itDev->second->bReg = false;                    

					if (itDev->second->nNetType != 4 && itDev->second->nNetType != 7 && (IsCamera(sDevID) || IsDevice(sDevID)))
					{
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,"设备[%s]注册超时.", itDev->second->pDeviceUri);
						if (!UpdateDeviceStatus(itDev->second->pDeviceUri, false))
						{
							g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新设备在线状态失败[%s].", itDev->second->pDeviceUri);
						}
						else
						{
							g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新设备在线状态成功[%s][ON->OFF].", itDev->second->pDeviceUri);
						}
					}
                    else if(IsPlatform(sDevID))
                    {
                        g_LogRecorder.WriteDebugLogEx(__FUNCTION__,"下级平台[%s]注册超时.", itDev->second->pDeviceUri);
                        if (!UpdatePlatformStatus(itDev->second->pDeviceUri, false))
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新平台所属设备在线状态失败[%s]\n", itDev->second->pDeviceUri);
                        }
                        else
                        {
                            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "更新平台所属设备在线状态成功[%s:ON->OFF]\n", itDev->second->pDeviceUri);
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
			//printf(">>>>>>>>>>>>>>>>>>>>检测完毕<<<<<<<<<<<<<<<<<<<<\n\n");
		}
		LeaveCriticalSection(&m_vLock);
		
		vector<string>::iterator userIt = UserClearCode.begin();
		while(userIt != UserClearCode.end())
		{
			printf("用户[%s]注册超时.\n", (*userIt).c_str());
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
                        "****Error: RTP流媒体[%s]无应答, 状态置为不在线!",  (*rtpit)->sRTPUri.c_str());

                    (*rtpit)->bOnLine = false;

                    //清理RTP流媒体转发的相关信息                   
                    printf("-------------------------------------\n");
                    g_LogRecorder.WriteErrorLogEx(__FUNCTION__,
                        "***Warning: 清理RTP流媒体[%s]相关转发流信息.", (*rtpit)->sRTPUri.c_str());
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
    } while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 2000));  //每2S检测一次状态
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
                    "RTP流媒体[%s]有应答, 状态置为在线!", (*rtpit)->sRTPUri.c_str());
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
        //清理录像查询时，由于丢失了录像结果包， 造成没有正确的删除m_SessionMap里保存的数据
        time_t tCurrentTime;
        tCurrentTime = time(&tCurrentTime);

        EnterCriticalSection(&m_sLock);
        SessionMap::iterator itsm = m_SessionMap.begin();
        while(itsm != m_SessionMap.end())
        {
            if((itsm->second.sTitle == "RECORDINFO" || itsm->second.sTitle == "CATALOGSUB")  && tCurrentTime - itsm->second.recvTime > 60 * 20)
            {
                g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "会话[%s]超时Title[%s]删除.", itsm->first.c_str(), itsm->second.sTitle.c_str());
                itsm = m_SessionMap.erase(itsm);
            }
            else
            {
                itsm ++;
            }
        }

        LeaveCriticalSection(&m_sLock);       
    } while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 300 * 1000));  //每5min检测一次状态
}

bool CSIPServer::ClearUserStream(char * sDevID)
{
    printf("--------------------------------------\n");
    g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "清理用户[%s]资源.", sDevID);

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
                printf("回到守望点ID[%s]\n", it->first.c_str());
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
    printf("需要守望的镜头:\r\n");
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
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "连接数据库失败!>错误信息：%s", p.msg);
            g_LogRecorder.WriteDebugLogEx(__FUNCTION__, "错误代码：%d", p.code);
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
    while (WAIT_TIMEOUT == WaitForSingleObject(m_hStopEvent, 1200 * 1000))//每1200S重新获取一次
    {
        g_LogRecorder.WriteInfoLogEx(__FUNCTION__, "=======重新读取数据库获取服务与设备相关信息!=======");

        EnterCriticalSection(&m_vLock);

        Init(true);

        
        LeaveCriticalSection(&m_vLock);
        g_LogRecorder.WriteInfoLog(__FUNCTION__, "=======重新读取数据库获取服务与设备相关信息完毕!=======");
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