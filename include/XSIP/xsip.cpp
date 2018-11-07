#include "StdAfx.h"
#include "xsip.h"
#include <stdlib.h>
#include <math.h>
#include <time.h>

#pragma  warning(disable:4267)
#pragma warning (disable:4996)

xsip::xsip(void)
: m_nCSeq(1)
, m_nPort(0)
{
	xsip_user_agent_init("Xinyi");
}

xsip::~xsip(void)
{

}

void xsip::xsip_user_agent_init(const char *useragent)
{
	m_sUserAgent = useragent;
}

string xsip::xsip_from_init(const char *username, const char *host, int port)
{
	char szFrom[512] = {0};
	sprintf_s(szFrom, sizeof(szFrom), "sip:%s@%s:%d", username, host, port);
	return szFrom;
}

string xsip::xsip_to_init(const char *username, const char *host, int port)
{
	return xsip_from_init(username, host, port);
}

string xsip::xsip_proy_init(const char *host, int port)
{
	char szProy[512] = {0};
	sprintf_s(szProy, sizeof(szProy), "sip:%s:%d", host, port);
	return szProy;
}

string xsip::xsip_call_id_init(unsigned long number, const char *host)
{
	char szCallid[512] = {0};
	if (host == NULL)
	{
		sprintf_s(szCallid, sizeof(szCallid), "%u", number);
	}
	else
	{
		sprintf_s(szCallid, sizeof(szCallid), "%u@%s", number, host);
	}
	return szCallid;
}

string xsip::xsip_call_id_init(string number, const char *host)
{
	char szCallid[512] = {0};
	if (number == "")
	{
		number = xsip_get_tag();
	}
	if (host == NULL)
	{
		sprintf_s(szCallid, sizeof(szCallid), "%s", number.c_str());
	}
	else
	{
		sprintf_s(szCallid, sizeof(szCallid), "%s@%s", number.c_str(), host);
	}
	return szCallid;
}

string xsip::xsip_contact_init(const char *username, const char *host, int port)
{
	return xsip_from_init(username, host, port);
}

string xsip::xsip_subject_init(const char *to, const char * sSSRC, const char *from, int nHandle)
{
	char szSubject[512] = {0};
	string dest, sour;
	if (strlen(to) > 20)
	{
		dest = xsip_get_url_username((char *)to);
	}
	else
	{
		dest = to;
	}
	if (strlen(from) > 20)
	{
		sour = xsip_get_url_username((char *)from);
	}
	else
	{
		sour = from;
	}
	sprintf_s(szSubject, sizeof(szSubject), "%s:%s,%s:%d", dest.c_str(), sSSRC, sour.c_str(), nHandle);
	m_nCSeq++;
	return szSubject;
}

string xsip::xsip_ssrc_init(int type, unsigned long ssrc)
{
    string sRealm(m_sUsername, 3, 5);
	char szSSRC[12] = {0};
	switch (type)
	{
	case XSIP_INVITE_PLAY:
		{
			sprintf_s(szSSRC, sizeof(szSSRC), "0%s%04u", sRealm.c_str(), ssrc);
			break;
		}
	case XSIP_INVITE_PLAYBACK:case XSIP_INVITE_DOWNLOAD:
		{
			sprintf_s(szSSRC, sizeof(szSSRC), "1%s%04u", sRealm.c_str(), ssrc);
			break;
		}
	default:
		return "";
	}
	return szSSRC;
}

int xsip::xsip_init_t(const char* username, const char *password, const char* ip, int port)
{
	m_sUsername = username;
	m_sPassword = password;
	m_sRealm = m_sUsername.substr(0, 10);
	m_sHost = ip;
	m_nPort = port;
	m_sFrom = xsip_from_init(username, ip, port);
	m_sContact = xsip_contact_init(username, ip, port);
	return 0;
}

int xsip::xsip_message_build_request(char *message, const char *method, const char *proxy, const char *to,
                                     const char *from, const char *callid, int cseq, string ftag, string ttag)
{
	string sCallid;
	if (to == NULL)
	{
		to = m_sFrom.c_str();
	}
	if (proxy == NULL)
	{
		proxy = to;
	}
	if (from == NULL)
	{
		from = m_sFrom.c_str();
	}
	if (callid == NULL)
	{
		sCallid = xsip_call_id_init(xsip_get_CallIDNum(), m_sHost.c_str());
	}
	else
	{
		sCallid = callid;
	}
	char szTo[512] = {0};
	if (ttag == "")
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>", to);
	}
	else
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>;tag=%s", to, ttag.c_str());
	}
	if (ftag == "")
	{
		ftag = xsip_get_tag();
	}
	try
	{	
        sprintf(message,	
            "%s %s SIP/2.0\r\n"
            "Via: SIP/2.0/UDP %s:%d;rport;branch=z9hG4bK%s\r\n"
            "From: <%s>;tag=%s\r\n"
            "To: %s\r\n"
            "Call-ID: %s\r\n"
            "CSeq: %d %s\r\n"
            "Max-Forwards: 70\r\n"
            "User-Agent: %s\r\n"
            "Content-Length: 0\r\n\r\n",
            method, proxy, m_sHost.c_str(), m_nPort, xsip_get_tag().c_str(), from, ftag.c_str(), szTo, sCallid.c_str(), cseq, method, m_sUserAgent.c_str());

    } 
	catch(...)
	{
		return -1;
	}
	return 0;
}

int xsip::xsip_info_build_initial(char *message,const char *method,const char *proxy ,const char *to,const char *from,const char *callid,int cseq,string ftag,string ttag)
{
	string sCallid;
	if (to == NULL)
	{
		to = m_sFrom.c_str();
	}
	if (proxy == NULL)
	{
		proxy = to;
	}
	if (from == NULL)
	{
		from = m_sFrom.c_str();
	}
	if (callid == NULL)
	{
		sCallid = xsip_call_id_init(xsip_get_CallIDNum());
	}
	else
	{
		sCallid = callid;
	}
	char szTo[512] = {0};
	if (ttag == "")
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>", to);
	}
	else
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>;tag=%s", to, ttag.c_str());
	}
	if (ftag == "")
	{
		ftag = xsip_get_tag();
	}
	try
	{
		sprintf(message,	
			"%s %s SIP/2.0\r\n"
			"Via: SIP/2.0/UDP %s:%d;branch=z9hG4bK%s\r\n"
			"From: <%s>;tag=%s\r\n"
			"To: %s\r\n"
			"Call-ID: %s\r\n"
			"CSeq: %d %s\r\n"
			"Max-Forwards: 70\r\n"
			"User-Agent: %s\r\n"
			"Content-Length: 0\r\n\r\n",
			method, proxy, m_sHost.c_str(), m_nPort, xsip_get_tag().c_str(), from, ftag.c_str(), szTo, sCallid.c_str(), cseq, method, m_sUserAgent.c_str());
	}
	catch(...)
	{
		return -1;
	}
	return 0;
}

int xsip::xsip_register_build_initial(char *reg,const char *proxy,const char *to,string callid,int cseq,string ftag, int expires,const char *from,const char *contact)
{
	int nRet = 0;
	string strCallid;
	if (contact == NULL)
	{
		contact = m_sContact.c_str();
	}
	if (callid == "")
	{
		strCallid = xsip_call_id_init(xsip_get_CallIDNum(), m_sHost.c_str());
	}
	else
	{
		strCallid = callid;
	}
	nRet = xsip_message_build_request(reg, "REGISTER", proxy, to, NULL, strCallid.c_str(), cseq, ftag);
	xsip_set_to(reg, m_sFrom.c_str());
	xsip_set_contact(reg, contact);
	xsip_set_expires(reg, expires);
	int nLen = (int)strlen(reg);
	reg[nLen] = '\0';
	return nRet;
}

int xsip::xsip_message_build_answer(char *message, int status, char *answer, string tag)
{
	string strVia = xsip_get_via(message);
	string strFrom = xsip_get_from(message);
	string strTo  = xsip_get_to_url(message);
	string strCallid = xsip_get_call_id(message);
	string strCSeq = xsip_get_cseq(message);
	if (strVia == ""||strFrom == ""||strTo == ""||strCallid == ""||strCSeq == "")
	{
		return -1;
	}
	char szTo[128] = {0};
	if (tag == "")
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>", strTo.c_str());
	}
	else
	{
		sprintf_s(szTo, sizeof(szTo), "<%s>;tag=%s", strTo.c_str(), tag.c_str());
	}
	string strAns;
	if (status == 100)
	{
		strAns = "Trying";					//尝试连接
	}
	else if (status == 101)
	{
		strAns = "Dialog Establishement";	//对话建立
	}
	else if (status == 200)
	{
		strAns = "OK";						//成功
	}
	else if (status == 201)
	{
		strAns = "Canceling";				//取消
		status = 200;
	}
	else if(status == 400)
	{
		strAns = "Bad Request";				//消息错误
	}
	else if (status == 401)
	{
		strAns = "Unauthorized";			//未认证
	}
	else if (status == 403)
	{
		strAns = "Forbidden";				//禁止
	}
	else if (status == 404)
	{
		strAns = "Not Found";				//没有找到
	}
	else if (status == 408)
	{
		strAns = "Request Timeout";			//消息超时
	}
	else if (status == 457)
	{
		strAns = "Invalid Range";			//无效范围
	}
	else if (status == 482)
	{
		strAns = "Loop Detected";			//循环消息 
	}
	else if (status == 481)
	{
		strAns = "Dialog/Transaction Does Not Exist";//没有匹配会话
	}
	else if (status == 486)
	{
		strAns = "Busy Here";				//用户超出
	}
	else if (status == 487)
	{
		strAns = "Request Terminated";		//请求终止
	}
	else if (status == 488)
	{
		strAns = "Not Acceptable Here";		//SDP错误
	}
	else if (status == 500)
	{
		strAns = "Server Internal Error";	//服务内部错误
	}
    else if(status == 503)
    {
        strAns = "Service Unavailable";
    }
	else if (status == 551)
	{
		strAns = "Option not supported";	//不支持操作
	}
	try
	{
		sprintf(answer,	
			"SIP/2.0 %d %s\r\n"
			"Via: %s\r\n"
			"From: %s\r\n"
			"To: %s\r\n"
			"Call-ID: %s\r\n"
			"CSeq: %s\r\n"
			"User-Agent: %s\r\n"
			"Content-Length: 0\r\n\r\n",
			status, strAns.c_str(), strVia.c_str(), strFrom.c_str(), szTo, strCallid.c_str(), strCSeq.c_str(), m_sUserAgent.c_str());
	}
	catch (...)
	{
		return -1;
	}
	return 0;
}

int xsip::xsip_message_build_response(char *message, char *response)
{
	string sMsgFrom = xsip_get_from_url(message);
	string sMsgTo = xsip_get_to_url(message);
	string sCallID = xsip_get_call_id(message);
	string dwFmTag = xsip_get_from_tag(message);
	string dwToTag = xsip_get_to_tag(message);
	int nCeq = xsip_get_cseq_number(message);
	xsip_message_build_request(response,"MESSAGE",sMsgTo.c_str(),sMsgFrom.c_str(),sMsgTo.c_str(),sCallID.c_str(),nCeq,dwToTag,dwFmTag);
	return 0;
}

int xsip::xsip_subscribe_build_initial(char *subscribe, const char *to, const char *from, char *et, int expires)
{
	int nRet = 0;
	nRet = xsip_message_build_request(subscribe,"SUBSCRIBE",to,to,from,NULL,20);
	xsip_set_contact(subscribe, m_sContact.c_str());
	xsip_set_expires(subscribe, expires);
	xsip_set_event(subscribe, et);
	return nRet;
}

int xsip::xsip_subscribe_build_answer(char *subscribe, int status, char *answer, string ttag)
{
	int nRet = 0;
	string strEvent = xsip_get_event(subscribe);
	nRet = xsip_message_build_answer(subscribe, status, answer, ttag);
	xsip_set_contact(answer, m_sContact.c_str());
	if (strEvent != "")
	{
		xsip_set_event(answer, strEvent.c_str());
	}
	return nRet;
}

int xsip::xsip_subscribe_build_notify(char *notify,const char *to,string callid,int cseq,string ttag,string ftag,const char *from,const char *et)
{
	int nRet = 0;
	string sCallid;
	if (callid == "")
	{
		sCallid = xsip_call_id_init(xsip_get_CallIDNum());
	}
	else
	{
		sCallid = callid;
	}
	nRet = xsip_message_build_request(notify,"NOTIFY",to,to,from,sCallid.c_str(),cseq,ftag,ttag);
	xsip_set_contact(notify, m_sContact.c_str());
	xsip_set_event(notify, et);
	xsip_set_subscriptoin_state(notify);
	return nRet;
}

int xsip::xsip_notify_build_answer(char *notify, int status, char *answer)
{
	string dwTag = xsip_get_to_tag(notify);
	if (dwTag == "")
	{
		dwTag = xsip_get_tag();
	}
	return xsip_message_build_answer(notify, status, answer, dwTag);
}

int xsip::xsip_call_build_invite(char *invite, const char *to, const char *subject, const char *from, string ftag, string callid)
{
	xsip_message_build_request(invite,"INVITE",to,to,from,callid.c_str(),20,ftag);
	xsip_set_contact(invite, m_sContact.c_str());
	xsip_set_subject(invite, subject);
	xsip_set_expires(invite, 120);
	return 0;
}

int xsip::xsip_call_build_cancel(char *cancel,const char*to,const char *from,string ftag,string callid,int ncseq)
{
	xsip_message_build_request(cancel,"CANCEL",to,to,from,callid.c_str(),ncseq,ftag);
	return 0;
}

int xsip::xsip_call_build_answer(char *invite, int status, char *answer, string ttag)
{
	if (ttag == "")
	{
		ttag = xsip_get_tag();
	}
	xsip_message_build_answer(invite, status, answer, ttag);
	string sContact = xsip_proy_init(m_sHost.c_str(), m_nPort);
	xsip_set_contact(answer, sContact.c_str());
	return 0;
}

int xsip::xsip_call_build_ack(char *answer, char *ack)
{
	string strVia = xsip_get_via(answer);
	string strFrom = xsip_get_from(answer);
	string strTo  = xsip_get_to(answer);
	string strContact = xsip_get_to_url(answer);
	string strCallid = xsip_get_call_id(answer);
	int nSCeq = xsip_get_cseq_number(answer);
	sprintf(ack, 
		"ACK %s SIP/2.0\r\n"
		"Via: %s\r\n"
		"From: %s\r\n"
		"To: %s\r\n"
		"Call-ID: %s\r\n"
		"CSeq: %d ACK\r\n"
		"Contact: <%s>\r\n"
		"Max-Forwards: 70\r\n"
		"User-Agent: %s\r\n"
		"Content-Length: 0\r\n\r\n",
		strContact.c_str(), strVia.c_str(), strFrom.c_str(), strTo.c_str(), strCallid.c_str(), nSCeq,
		m_sContact.c_str(), m_sUserAgent.c_str());
	return 0;
}
int xsip::xsip_call_build_bye(char *answer, char *bye, bool bCancel, int nCamCseq)
{
    string strVia = xsip_get_via(answer);
    string strFrom = xsip_get_from(answer);
    string strTo  = xsip_get_to(answer);
    string strContact = xsip_get_to_url(answer);
    string strCallid = xsip_get_call_id(answer);
    int nSCeq = xsip_get_cseq_number(answer);
    string sMethod = bCancel ? "CANCEL" : "BYE";
    int nAdd = nCamCseq == 0 ? 1 : nCamCseq + 1;
    sprintf(bye, 
        "%s %s SIP/2.0\r\n"
        //"Via: %s\r\n"
        "Via: SIP/2.0/UDP %s:%d;rport;branch=z9hG4bK%s\r\n"
        "From: %s\r\n"
        "To: %s\r\n"
        "Call-ID: %s\r\n"
        "CSeq: %d %s\r\n"
        "Contact: <%s>\r\n"
        "Max-Forwards: 70\r\n"
        "User-Agent: %s\r\n"
        "Content-Length: 0\r\n\r\n",
        sMethod.c_str(), strContact.c_str(), /* strVia.c_str(),*/m_sHost.c_str(), m_nPort, xsip_get_tag().c_str(), strFrom.c_str(), strTo.c_str(), strCallid.c_str(), nSCeq + nAdd, sMethod.c_str(),
        m_sContact.c_str(), m_sUserAgent.c_str());
    return 0;
}
int xsip::xsip_call_build_bye(char *bye, const char *to,string callid,string ttag,string ftag,int cseq,const char *from)
{
	int nRet = 0;
	string sCallid;
	if (callid == "")
	{
		sCallid = xsip_call_id_init(xsip_get_CallIDNum());
	}
	else
	{
		sCallid = callid;
	}
	nRet = xsip_message_build_request(bye,"BYE",to,to,from,sCallid.c_str(),cseq,ftag,ttag);
	xsip_set_contact(bye, m_sContact.c_str());
	return nRet;
}

int xsip::xsip_bye_build_answer(char *bye, int status, char *answer)
{
	int nRet = 0;
	string ttag = xsip_get_to_tag(bye);
	nRet = xsip_message_build_answer(bye, status, answer, ttag);
	return 0;
}

int xsip::xsip_sdp_build_initial(char *sdp,char *username,const char *ip,int port,int type,int mode,char *mamuf,char *uri,char *ssrc,char *time,char *fm, int nNetType)
{
    string sNetStream = nNetType == 1 ? "RTP/AVP" : "TCP/RTP/AVP" ;
	string sPlay,sMode;
	if (username == NULL)
	{
		username = (char *)m_sUsername.c_str();
	}
	switch (type)
	{
	case XSIP_INVITE_PLAY:
		sPlay = "Play";
		break;
	case XSIP_INVITE_PLAYBACK:
		sPlay = "Playback";
		break;
	case XSIP_INVITE_DOWNLOAD:
		sPlay = "Download";
		break;
	default:
		return -1;
	}
	switch (mode)
	{
	case XSIP_SENDONLY:
		sMode = "sendonly";
		break;
	case XSIP_RECVONLY:
		sMode = "recvonly";
		break;
	case XSIP_SENDRECV:
		sMode = "sendrecv";
		break;
	default:
		return -1;
	}
	if (mamuf == NULL)
	{
        sprintf(sdp,
            "v=0\r\n"
            "o=%s 0 0 IN IP4 %s\r\n"
            "s=%s\r\n"
            "c=IN IP4 %s\r\n"
            "t=%s\r\n"
            "m=video %d %s 96 98 97\r\n"
            "a=%s\r\n"
            "a=rtpmap:96 PS/90000\r\n"
            "a=rtpmap:98 H264/90000\r\n"
            "a=rtpmap:97 MPEG4/90000\r\n",
            username, ip, sPlay.c_str(), ip, time, port, sNetStream.c_str(), sMode.c_str());
	}
	else
	{
		int pt = 96;
		if (strcmp(mamuf, "MPEG4") == 0)
		{
			pt = 97;
		}
		else if(strcmp(mamuf, "H264") == 0)
		{
			pt = 98;
		}

        sprintf(sdp,
            "v=0\r\n"
            "o=%s 0 0 IN IP4 %s\r\n"
            "s=%s\r\n"
            "c=IN IP4 %s\r\n"
            "t=%s\r\n"
            "m=video %d %s 96\r\n"
            "a=%s\r\n"
            "a=rtpmap:%d %s/90000\r\n",
            username, ip, sPlay.c_str(), ip, time, port, sNetStream.c_str(), sMode.c_str(), pt, mamuf);   
	}
	if (uri != NULL)
	{
		string str = xsip_set_sdp_u(sdp, uri);
		memcpy(sdp, str.c_str(), str.size());
	}
	if (ssrc != NULL)
	{
		string str = xsip_set_sdp_y(sdp, ssrc);
		memcpy(sdp, str.c_str(), str.size());
	}
	if (fm != NULL)
	{
		string str = xsip_set_sdp_f(sdp, fm);
		memcpy(sdp, str.c_str(), str.size());
	}
	int nSize = (int)strlen(sdp);
	return nSize;
}

PTZTYPE xsip::xsip_ptzcmd_parse(char *msg, long *speed)
{
	string sBody = xsip_get_body(msg);
	string sCmd = xsip_get_uri((char*)sBody.c_str(), "<PTZCmd>", "</PTZCmd>", 16);
	if (sCmd == "")
	{
		return XSIP_PTZ_ERROR;
	}
	long bit1,bit2,bit3,bit4,bit5,bit6,bit7,bit8;
	sscanf(sCmd.c_str(), "%02X%02X%02X%02X%02X%02X%02X%02X", &bit1,&bit2,&bit3,&bit4,&bit5,&bit6,&bit7,&bit8);
	if (bit1 == 0xA5 && bit2 == 0x0F)
	{
		if ((bit1+bit2+bit3+bit4+bit5+bit6+bit7)%0x100 != bit8)	//校验
		{
			return XSIP_PTZ_ERROR;
		}
		switch (bit4)
		{
		case CMD_PTZ_STOP:
			{
				return XSIP_PTZ_STOP;
			}
		case CMD_PTZ_FI_STOP:
			{
				return XSIP_PTZ_STOP;
			}
		case CMD_PTZ_UP:
			{
				*speed = bit6 >> 4;
				return XSIP_PTZ_UP;
			}
		case CMD_PTZ_DOWN:
			{
				*speed = bit6 >> 4;
				return XSIP_PTZ_DOWN;
			}
		case CMD_PTZ_LEFT:
			{
				*speed = bit5 >> 4;
				return XSIP_PTZ_LEFT;
			}
		case CMD_PTZ_RIGHT:
			{
				*speed = bit5 >> 4;
				return XSIP_PTZ_RIGHT;
			}
		case CMD_PTZ_LU:
			{
				*speed = ((bit5>>4)+(bit6>>4))/0x02;
				return XSIP_PTZ_LEFTUP;
			}
		case CMD_PTZ_LD:
			{
				*speed = ((bit5>>4)+(bit6>>4))/0x02;
				return XSIP_PTZ_LEFTDOWN;
			}
		case CMD_PTZ_RU:
			{
				*speed = ((bit5>>4)+(bit6>>4))/0x02;
				return XSIP_PTZ_RIGHTUP;
			}
		case CMD_PTZ_RD:
			{
				*speed = ((bit5>>4)+(bit6>>4))/0x02;
				return XSIP_PTZ_RIGHTDOWN;
			}
		case CMD_PTZ_ZOOM_IN:
			{
				*speed = bit7 >> 4;
				return XSIP_PTZ_ZOOM_IN;
			}
		case CMD_PTZ_ZOOM_OUT:
			{
				*speed = bit7 >> 4;
				return XSIP_PTZ_ZOOM_OUT;
			}
		case CMD_PTZ_FOCUS_IN:
			{
				*speed = bit5 >> 4;
				return XSIP_PTZ_FOCUS_IN;
			}
		case CMD_PTZ_FOCUS_OUT:
			{
				*speed = bit5 >> 4;
				return XSIP_PTZ_FOCUS_OUT;
			}
		case CMD_PTZ_IRIS_IN:
			{
				*speed = bit6 >> 4;
				return XSIP_PTZ_IRIS_IN;
			}
		case CMD_PTZ_IRIS_OUT:
			{
				*speed = bit6 >> 4;
				return XSIP_PTZ_IRIS_OUT;
			}
		case CMD_PTZ_SET:
			{
				*speed = bit6;
				return XSIP_PTZ_SET;
			}
		case CMD_PTZ_GOTO:
			{
				*speed = bit6;
				return XSIP_PTZ_GOTO;
			}
		case CMD_PTZ_DELETE:
			{
				*speed = bit6;
				return XSIP_PTZ_DELETE;
			}
		default:
			{
				return XSIP_PTZ_STOP;
			}
		}
	}
	return XSIP_PTZ_ERROR;
}

bool xsip::MSG_IS_MSG(char *msg, const char *head)
{
	string strMsg = msg;
	int nPos = strMsg.find(head);
	if (nPos == 0)
	{
		return true;
	}
	return false;
}
bool xsip::MSG_IS_REGISTER(char *msg)
{
	return MSG_IS_MSG(msg, "REGISTER");
}
bool xsip::MSG_IS_QUERYINFO(char *msg)				//是否是服务信息查询
{
    if(string(msg).find("QuertInfo") != string::npos)
    {
        return true;
    }
    return false;
}

bool xsip::MSG_IS_NOTIFY(char *msg)
{
	return MSG_IS_MSG(msg, "NOTIFY");
}
bool xsip::MSG_IS_SUBSCRIBE(char *msg)
{
	return MSG_IS_MSG(msg, "SUBSCRIBE");
}
bool xsip::MSG_IS_INVITE(char *msg)
{
	return MSG_IS_MSG(msg, "INVITE");
}
bool xsip::MSG_IS_ACK(char *msg)
{
	return MSG_IS_MSG(msg, "ACK");
}
bool xsip::MSG_IS_BYE(char *msg)
{
	return MSG_IS_MSG(msg, "BYE");
}
bool xsip::MSG_IS_MESSAGE(char *msg)
{
	return MSG_IS_MSG(msg, "MESSAGE");
}
bool xsip::MSG_IS_CANCEL(char *msg)
{
	return MSG_IS_MSG(msg, "CANCEL");
}
bool xsip::MSG_IS_INFO(char *msg)
{
	return MSG_IS_MSG(msg, "INFO");
}
int xsip::MSG_IS_STATUS(char *msg)
{
	string strMsg = msg;
	int nPos = strMsg.find("SIP/2.0 ");
	if (nPos == 0)
	{
		nPos += strlen("SIP/2.0 ");
		string strStatus = strMsg.substr(nPos, 3);
		return atoi(strStatus.c_str());
	}
	return false;
}
bool xsip::MSG_IS_STATUS_1XX(char *msg)
{
	int nStatus = MSG_IS_STATUS(msg);
	if (nStatus > 0 && nStatus < 200)
	{
		return true;
	}
	return false;
}
bool xsip::MSG_IS_STATUS_2XX(char *msg)
{
	int nStatus = MSG_IS_STATUS(msg);
	if (nStatus > 199 && nStatus < 300)
	{
		return true;
	}
	return false;
}
bool xsip::MSG_IS_STATUS_3XX(char *msg)
{
	int nStatus = MSG_IS_STATUS(msg);
	if (nStatus > 299 && nStatus < 400)
	{
		return true;
	}
	return false;
}
bool xsip::MSG_IS_STATUS_4XX(char *msg)
{
	int nStatus = MSG_IS_STATUS(msg);
	if (nStatus > 399 && nStatus < 500)
	{
		return true;
	}
	return false;
}

bool xsip::MSG_IS_STATUS_5XX(char *msg)
{
	int nStatus = MSG_IS_STATUS(msg);
	if (nStatus > 499 && nStatus < 600)
	{
		return true;
	}
	return false;
}

string xsip::xsip_set_uri(char *msg, const char *Field, const char *pos, const char *value)
{
	char szField[DEFAULT_LEN] = {0};
	sprintf_s(szField, sizeof(szField), "%s: %s\r\n", Field, value);
	string strMsg = msg;
	int nBegin = 0;
	nBegin = strMsg.find(pos);
	if (nBegin != string::npos)
	{
		string str = strMsg.insert(nBegin, szField);      
		strcpy(msg, str.c_str());
		return str;
	}
	return "";
}

string xsip::xsip_set_proxy(char *msg, const char *proxy)
{
	string strMsg = msg;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find("sip:");
	if (nBegin != string::npos)
	{
		nEnd = strMsg.find(" SIP/2.0", nBegin);
		if (nEnd != string::npos)
		{
			nLen = nEnd - nBegin;
			return strMsg.replace(nBegin, nLen, proxy);
		}
		return "";
	}
	return "";
}

string xsip::xsip_set_via_addr(char *msg, const char *ip, int port)
{
	string sMsg = msg;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char szAddr[512] = {0};
	sprintf_s(szAddr, sizeof(szAddr), "%s:%d", ip, port);
	nBegin = sMsg.find("Via: SIP/2.0/UDP ");
	if (nBegin != string::npos)
	{
		nBegin += (int)strlen("Via: SIP/2.0/UDP ");
		nEnd = sMsg.find(';', nBegin);
		if (nEnd != string::npos)
		{
			nEnd = sMsg.find(';', nEnd);
			nLen = nEnd - nBegin;
			if (nLen > 0 && nLen < MAX_LIST_LEN)
			{
				string str = sMsg.replace(nBegin, nLen, szAddr);
				memcpy(msg, str.c_str(), str.size());
				return str;
			}
		}
		return "";
	}
	return "";
}

string xsip::xsip_set_Route(char *msg, const char route)
{
	string sRoute = "<";
	sRoute += route;
	sRoute += ">";
	return xsip_set_uri(msg, "Route", "From:", sRoute.c_str());
}

string xsip::xsip_set_from(char *msg, const char *from)
{
	string strMsg = msg;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find("From: <sip:");
	if (nBegin != string::npos)
	{
		nBegin = strMsg.find("sip:", nBegin);
		nEnd = strMsg.find('>', nBegin);
		nLen = nEnd - nBegin;
		if (nLen >=0 && nLen < MAX_LIST_LEN)
		{
			string str =  strMsg.replace(nBegin, nLen, from);
			memcpy(msg, str.c_str(), str.size());
			return str;
		}
	}
	return "";
}

string xsip::xsip_set_to(char *msg, const char *to)
{
	string strMsg = msg;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find("To: <sip:");
	if (nBegin != string::npos)
	{
		nBegin = strMsg.find("sip:", nBegin);
		nEnd = strMsg.find('>', nBegin);
		nLen = nEnd - nBegin;
		if (nLen >=0 && nLen < MAX_LIST_LEN)
		{
			string str =  strMsg.replace(nBegin, nLen, to);
			memcpy(msg, str.c_str(), str.size());
			return str;
		}
	}
	return "";
}

string xsip::xsip_set_call_id(char *msg, const char *callid)
{
	string strMsg = msg;
	char enter[3] = {13, 10, 0};
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find("Call-ID: ");
	if (nBegin != string::npos)
	{
		nBegin += strlen("Call-ID: ");
		nEnd = strMsg.find(enter, nBegin);
		nLen = nEnd - nBegin;
		if (nLen <= 0 || nLen > MAX_LIST_LEN)
		{
			return strMsg;
		}
		string str = strMsg.replace(nBegin, nLen, callid);
		memcpy(msg, str.c_str(), str.size());
		return str;
	}
	return strMsg;
}

string xsip::xsip_set_contact(char *msg, const char *contact)
{
	string strMsg = msg;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find("Contact: <");
	if (nBegin != string::npos)
	{
		nBegin += (int)strlen("Contact: <");
		nEnd = strMsg.find('>', nBegin);
		nLen = nEnd - nBegin;
		if (nLen > 0 && nLen < MAX_LIST_LEN)
		{
			string str = strMsg.replace(nBegin, nLen, contact);
			memcpy(msg, str.c_str(), str.size());
			return str;
		}
	}
	char enter[3] = {13, 10, 0};
	char szContact[128] = {0};
	sprintf_s(szContact, sizeof(szContact), "Contact: <%s>\r\n", contact);
	nBegin = strMsg.find("CSeq:");
	if (nBegin != string::npos)
	{
		nBegin = strMsg.find(enter, nBegin) + 2;
		string str = strMsg.insert(nBegin, szContact);
		memcpy(msg, str.c_str(), str.size());
		return str;
	}
	return "";
}

string xsip::xsip_set_content_type(char *msg, const char *content)
{
	return xsip_set_uri(msg, "Content-Type", "Content-Length:", content);
}

string xsip::xsip_set_content_type(char *msg, int type)
{
	string sType;
	if (type == XSIP_APPLICATION_XML)
	{
		sType = "Application/MANSCDP+xml";
	}
	else if (type == XSIP_APPLICATION_SDP)
	{
		sType = "application/sdp";
	}
	else if (type == XSIP_APPLICATION_RTSP)
	{
		sType = "Application/MANSRTSP";
		//sType = "Application/RTSP";
	}
	else
	{
		return "";
	}
	return xsip_set_content_type(msg, sType.c_str());
}

string xsip::xsip_set_authorization(char *msg, const char *authorization)
{
	return xsip_set_uri(msg, "Authorization", "Max-Forwards:", authorization);
}

string xsip::xsip_set_www_authenticate(char *msg, const char *authenticate)
{
	return xsip_set_uri(msg, "WWW-Authenticate", "Content-Length:", authenticate);
}

string xsip::xsip_set_note(char *msg, const char *note)
{
	return xsip_set_uri(msg, "Note", "Content-Length:", note);
}

string xsip::xsip_set_event(char *msg, const char *et)
{
	return xsip_set_uri(msg, "Event", "Content-Length:", et);
}

string xsip::xsip_set_date(char *msg, const char *date)
{
	return xsip_set_uri(msg, "Date", "Content-Length:", date);
}

string xsip::xsip_set_expires(char *msg, int expires)
{
	char szExpiress[MAX_STR_LEN] = {0};
	itoa(expires, szExpiress, 10);
	return xsip_set_uri(msg, "Expires", "Content-Length:", szExpiress);
}

string xsip::xsip_set_subject(char *msg, const char *subject)
{
	return xsip_set_uri(msg, "Subject", "Content-Length:", subject);
}

string xsip::xsip_set_monitor_user_identity(char *msg, const char *cmd)
{
	char szField[MAX_STR_LEN] = {0};
	sprintf_s(szField, sizeof(szField), "operation=%s,extparam=0", cmd);
	return xsip_set_uri(msg, "Monitor-User-Identity", "Content-Length:", szField);
}

string xsip::xsip_set_subscriptoin_state(char *msg, int ss, int ex, int re)
{
	string sState = "";
	switch (ss)
	{
	case XSIP_PENDING:
		sState = "pending";
		break;
	case XSIP_ACTIVE:
		sState = "active";
		break;
	case XSIP_TERMINATED:
		sState = "terminated";
		break;
	default:
		return "";
	}
	char szState[DEFAULT_LEN] = {0};
	sprintf_s(szState, sizeof(szState), "Subscriptoin_State: %s;expires=%d;retry-after=%d\r\n", sState.c_str(),ex, re);
	string strMsg = msg;
	int nBegin = 0;
	nBegin = strMsg.find("Content-Length:");
	if (nBegin != string::npos)
	{
		string str = strMsg.insert(nBegin, szState);
		memcpy(msg, str.c_str(), str.size());
		return str;
	}
	return "";
}
string xsip::xsip_set_body_sn(char * body, char * sn, int len)                  //在body中替换sn
{
    string sBody(body);
    int nPos1 = sBody.find("<SN>");
    if(string::npos == nPos1)
    {
        return "";
    }
    nPos1 += strlen("<SN>");

    int nPos2 = sBody.find("</SN>", nPos1);
    if(string::npos == nPos2)
    {
        return "";
    }

    sBody.erase(nPos1, nPos2 - nPos1);
    sBody.insert(nPos1, sn, len);
    return sBody;
}

string xsip::xsip_set_body(char *msg, char *body, int len)
{
	char szLen[6] = {0};
	sprintf_s(szLen, sizeof(szLen), "%5d", len);
	string strMsg = msg;
	int nBegin = 0;
	nBegin = strMsg.find("Content-Length:");
	if (nBegin != string::npos)
	{
		nBegin = strMsg.find('0', nBegin);
		strMsg.replace(nBegin, 1, szLen);
		string str = strMsg.append(body);
		memcpy(msg, str.c_str(), str.size());
		return str;
	}
	return "";
}

string xsip::xsip_set_sdp_v(const char *sdp, const char *item, const char *value)
{
	string sSdp = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
    char enter[3] = {13, 10, 0};
    char line[2] = {10, 0};
	nBegin = sSdp.find(item);
	if (nBegin != string::npos)
	{
		nBegin += (int)strlen(item);
		nEnd = sSdp.find(enter, nBegin);
        if(nEnd == -1)
        {
            nEnd = sSdp.find(line, nBegin);
        }
		nLen = nEnd - nBegin;
		if (nLen > 0)
		{
			return sSdp.replace(nBegin, nLen, value);
		}
	}
	return sSdp;
}

string xsip::xsip_get_sdp_o_c(const char *sdp)
{
	string sSdp = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char enter[3] = {13, 10, 0};
	char line[2] = {10, 0};
	nBegin = sSdp.find("IN IP4 ");
	if (nBegin != string::npos)
	{
		nBegin += strlen("IN IP4 ");
		nEnd = sSdp.find(enter, nBegin);
		nLen = nEnd - nBegin;
		if (nLen > 0 && nLen < 16)
		{
			return sSdp.substr(nBegin, nLen);
		}
		nEnd = sSdp.find(line,nBegin);
		nLen = nEnd - nBegin;
		if (nLen > 0 && nLen < 16)
		{
			return sSdp.substr(nBegin, nLen);
		}
		return "";
	}
	return "";
}

string xsip::xsip_set_sdp_o_c(const char *sdp, const char *value)
{
	string sSdp = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char enter[3] = {13, 10, 0};
    char line[2] = {10, 0};
	nBegin = sSdp.find("IN IP4 ");
	if (nBegin != string::npos)
	{
		nBegin += strlen("IN IP4 ");
		nEnd = sSdp.find(enter, nBegin);
        if(nEnd == -1)
        {
            nEnd = sSdp.find(line, nBegin);
        }
		nLen = nEnd - nBegin;
		if (nLen > 0 && nLen < 16)
		{
			return sSdp.replace(nBegin, nLen, value);
		}
        else
        {
            char enter[2] = {10, 0};
            nEnd = sSdp.find(enter, nBegin);
            nLen = nEnd - nBegin;
            if (nLen > 0 && nLen < 16)
            {
                return sSdp.replace(nBegin, nLen, value);
            }
        }
		return "";
	}
	return "";
}

string xsip::xsip_set_sdp_o(const char *sdp, const char *value)
{
	return xsip_set_sdp_v(sdp, "o=", value);
}
string xsip::xsip_set_sdp_s(const char *sdp, const char *value)
{
	return xsip_set_sdp_v(sdp, "s=", value);
}
string xsip::xsip_set_sdp_u(const char *sdp, const char *value)
{
	string sSdp = sdp;
	string sUrl = value;
	sUrl += "\r\n";
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char enter[3] = {13, 10, 0};
    char line[2] = {10, 0};
	nBegin = sSdp.find("u=");
	if (nBegin != string::npos)
	{
		return xsip_set_sdp_v(sdp, "u=", value);
	}
	nBegin = sSdp.find("s=");
	if (nBegin != string::npos)
	{
		nBegin = sSdp.find(enter, nBegin);
        if(nBegin == -1)
        {
            nBegin = sSdp.find(line, nBegin);
            nBegin += strlen(line);
        }
        else
        {
            nBegin += strlen(enter);
        }
		       
		string tmp = "u=";
		tmp += sUrl;
		return sSdp.insert(nBegin, tmp);
	}
	return sdp;
}
string xsip::xsip_set_sdp_c(const char *sdp, const char *value)
{
	char szValue[128] = {0};
	sprintf_s(szValue, sizeof(szValue), "IN IP4 %s", value);
	return xsip_set_sdp_v(sdp, "c=", szValue);
}

string xsip::xsip_set_sdp_t(const char *sdp, const char *value)
{
	return xsip_set_sdp_v(sdp,"t=", value);
}

string xsip::xsip_set_sdp_m_port(const char *sdp, int port)
{
	char szPort[8] = {0};
	itoa(port, szPort, 10);
	string sSdp = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = sSdp.find("m=");
	if (nBegin != string::npos)
	{
		nBegin = sSdp.find(" ", nBegin);
		nBegin += 1;
		nEnd = sSdp.find(" ", nBegin);
		nLen = nEnd - nBegin;
		if (nLen <=0 || nLen > 6)
		{
			return "";
		}
		return sSdp.replace(nBegin, nLen, szPort);
	}
	return sSdp;
}
string xsip::xsip_set_sdp_a(const char *sdp, const char *value)
{
	return xsip_set_sdp_v(sdp, "a=", value);
}

string xsip::xsip_set_sdp_y(const char *sdp, const char *value)
{
	string sSdp = sdp;
	char szValue[DEFAULT_LEN] = {0};
	sprintf_s(szValue, sizeof(szValue), "y=%s\r\n", value);
	
	int nBegin = sSdp.find("y=");
	if (nBegin != string::npos)
	{
		return xsip_set_sdp_v(sdp, "y=", value);	
	}
	return sSdp.insert(sSdp.size(),szValue);
}
	

string xsip::xsip_set_sdp_f(const char *sdp, const char *value)
{
	string sSdp = sdp;
	char szValue[DEFAULT_LEN] = {0};
	sprintf_s(szValue, sizeof(szValue), "f=%s\r\n", value);
	int nBegin = sSdp.find("f=");
	if (nBegin != string::npos)
	{
		return xsip_set_sdp_v(sdp, "f=", value);
	}
	return sSdp.insert(sSdp.size(),szValue);
}
string  xsip::xsip_set_sdp_downloadspeed(const char *sdp, const char *value)				//插入downloadspeed值
{
    string sSdp = sdp;
    char szValue[DEFAULT_LEN] = {0};
    sprintf_s(szValue, sizeof(szValue), "a=downloadspeed: %s\r\n", value);
    int nBegin = sSdp.find("a=downloadspeed");
    if (nBegin != string::npos)
    {
        return xsip_set_sdp_v(sdp, "a=downloadspeed", szValue);
    }
    return sSdp.insert(sSdp.size(),szValue);
}

string xsip::xsip_set_sdp_f(const char *sdp, int nResolution)
{
	char szValue[DEFAULT_LEN] = {0};
	switch (nResolution)
	{
	case 0:
		return sdp;
	case 1:
		sprintf_s(szValue,sizeof(szValue),"v/4/1/25/1/256a/1/8/1");
		break;
	case 2:
		sprintf_s(szValue,sizeof(szValue),"v/4/2/25/1/512a/1/8/1");
		break;
	case 3:
		sprintf_s(szValue,sizeof(szValue),"v/2/3/25/1/768a/1/8/1");
		break;
	case 4:
		sprintf_s(szValue,sizeof(szValue),"v/2/4/25/1/1024a/1/8/1");
		break;
	case 5:
		sprintf_s(szValue,sizeof(szValue),"v/2/5/25/1/1536a/1/8/1");
		break;
	case 6:
		sprintf_s(szValue,sizeof(szValue),"v/2/6/25/1/2048a/1/8/1");
		break;
	default:
		return sdp;
	}
	return xsip_set_sdp_f(sdp,szValue);
}

unsigned long xsip::xsip_get_num()
{
	static int nInit = 0;
    if (!nInit)
    {
        srand((unsigned)time(0));
        nInit = 1;
    }

    unsigned long ulOp = 220000000;
    static unsigned int nNum = rand() % ulOp;

	unsigned int ulTag = ulOp + nNum++;
    if(nNum > ulOp)
    {
        nNum = rand() % ulOp;
    }
	return ulTag;
}


string xsip::xsip_get_tag()
{
	unsigned long ulTag = xsip_get_num();
	char szTag[11] = {0};
	sprintf_s(szTag, sizeof(szTag), "%u", ulTag);
	return szTag;
}
string xsip::xsip_get_CallIDNum()
{
    static int nCallIDNumInit = 0;
    if (!nCallIDNumInit)
    {
        srand((unsigned)time(0));
        nCallIDNumInit = 1;
    }

    unsigned long ulOp = 440000000;
    static unsigned int nCallNum = rand() % ulOp;

    unsigned int ulCallID = ulOp + nCallNum++;
    if(nCallNum > ulOp)
    {
        nCallNum = rand() % ulOp;
    }

    char szCallID[11] = {0};
    sprintf_s(szCallID, sizeof(szCallID), "%u", ulCallID);
    return szCallID;
}

string xsip::xsip_get_nonce()
{
	XMD5 md5;
	md5.update(xsip_get_tag());
	return md5.toString().substr(0, 16);
}

EVENT_TYPE xsip::xsip_get_event_type(char *msg)
{
    if (MSG_IS_QUERYINFO(msg))
    {
        return XSIP_DEVICE_QUERYINFO;
    }
	if (MSG_IS_REGISTER(msg))
	{
		return XSIP_DEVICE_REG;
	}
	else if (MSG_IS_SUBSCRIBE(msg))
	{
		return XSIP_SUBSCRIPTION_NEW;
	}
	else if (MSG_IS_NOTIFY(msg))
	{
		return XSIP_SUBSCRIPTION_NOTIFY;
	}
	else if (MSG_IS_INVITE(msg))
	{
		return XSIP_CALL_INVITE;
	}
	else if (MSG_IS_CANCEL(msg))
	{
		return XSIP_CALL_CANCEL;
	}
	else if (MSG_IS_ACK(msg))
	{
		return XSIP_CALL_ACK;
	}
	else if (MSG_IS_INFO(msg))
	{
		//if(xsip_get_rtsp_type(xsip_get_body(msg).c_str()) == XSIP_RTSP_TEARDOWN)
		//{
		//	return XSIP_INFO_TEARDOWN;
		//}
		return XSIP_INFO_PLAY;
	}
	else if (MSG_IS_BYE(msg))
	{
		return XSIP_CALL_BYE;
	}
	else if (MSG_IS_MESSAGE(msg))
	{
		return XSIP_MESSAGE_NEW;
	}
	else if (MSG_IS_STATUS_1XX(msg))
	{
		if (xsip_get_cseq_method(msg) == "SUBSCRIBE")
		{
			return XSIP_SUBSCRIPTION_PROCEEDING;
		}
		else if (xsip_get_cseq_method(msg) == "INVITE")
		{
			return XSIP_CALL_PROCEEDING;
		}
	}
	else if (MSG_IS_STATUS_2XX(msg))
	{
		if (xsip_get_cseq_method(msg) == "REGISTER")
		{
			return XSIP_REGSUPSERVER_SUCCESS;
		}
		else if (xsip_get_cseq_method(msg) == "SUBSCRIBE")
		{
			return XSIP_SUBSCRIPTION_ANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "NOTIFY")
		{
			return XSIP_NOTIFICATION_ANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "INVITE")
		{
			return XSIP_CALL_INVITEANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "CANCEL")
		{
			return XSIP_CANCEL_ANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "INFO")
		{
			return XSIP_INFO_ANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "BYE")
		{
			return XSIP_BYE_ANSWERED;
		}
		else if (xsip_get_cseq_method(msg) == "MESSAGE")
		{
			return XSIP_MESSAGE_ANSWERED;
		}
	}
	else if (MSG_IS_STATUS_4XX(msg) || MSG_IS_STATUS_5XX(msg))
	{
		if (xsip_get_cseq_method(msg) == "REGISTER")
		{
			return XSIP_REGSUPSERVER_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "SUBSCRIBE")
		{
			return XSIP_SUBSCRIPTION_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "NOTIFY")
		{
			return XSIP_NOTIFICATION_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "INVITE")
		{
			return XSIP_CALL_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "CANCEL")
		{
			return XSIP_CANCEL_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "INFO")
		{
			return XSIP_INFO_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "BYE")
		{
			return XSIP_BYE_FAILURE;
		}
		else if (xsip_get_cseq_method(msg) == "MESSAGE")
		{
			return XSIP_MESSAGE_FAILURE;
		}
	}
	else
	{
		return XSIP_EVENT_COUNT;
	}
	return XSIP_EVENT_COUNT;
}

MESSAGE_TYPE xsip::xsip_get_message_type(char *message)
{
	string strMsg = message;
    if(strMsg == "")
    {
        printf("strMsg = NULL\n", strMsg.c_str());
    }
	if (strMsg.find("403 Forbidden") != string::npos)
	{
		return XSIP_MESSAGE_KEEPALIVE_RESPHONSE;
	}
	if (strMsg.find("408 Request Timeout") != string::npos)
	{
		return XSIP_MESSAGE_TIMEOUT_RESPHONSE;
	}
	strMsg = xsip_get_body(message);
	if (strMsg.find("<Response>") != string::npos)
	{
		if (strMsg.find(">DeviceControl<") != string::npos)
		{
			return XSIP_MESSAGE_CMD_RESPONSE;
		}
		//else if (strMsg.find(">Keepalive<") != string::npos || strMsg.find(">KeepAlive<") != string::npos)
		//{
		//	return XSIP_MESSAGE_KEEPALIVE_RESPHONSE;
		//}
		else if (strMsg.find(">Alarm<") != string::npos)
		{
			return XSIP_MESSAGE_ALARM_RESPONSE;
		}
		else if (strMsg.find(">RecordInfo<") != string::npos)
		{
			return XSIP_MESSAGE_RECORDINFO_RESPONSE;
		}
		else if (strMsg.find(">DeviceInfo<") != string::npos)
		{
			return XSIP_MESSAGE_DVRINFO_RESPONSE;
		}
		else if (strMsg.find(">DeviceStatus<") != string::npos)
		{
			return XSIP_MESSAGE_DVRSTATUS_RESPONSE;
		}
		else if (strMsg.find(">Catalog<") != string::npos)
		{
			return XSIP_MESSAGE_CATALOG_RESPONSE;
		}
	}
	else if (strMsg.find("<Notify>") != string::npos)
	{
        if (strMsg.find(">AreaDetection<") != string::npos)
        {
            return XSIP_MESSAGE_AREADETECTION_RESPONSE;
        }
        else if (strMsg.find(">CrossDetection<") != string::npos)
        {
            return XSIP_MESSAGE_CROSSDETECTION_RESPONSE;
        }
		else if (strMsg.find(">Keepalive<") != string::npos || strMsg.find(">KeepAlive<") != string::npos)
		{
			if (strMsg.find(">Catalog<") != string::npos)
			{
				return XSIP_MESSAGE_CATALOG_RESPONSE;
			}
			else if (strMsg.find(">RecordInfo<") != string::npos)
			{
				return XSIP_MESSAGE_RECORDINFO_RESPONSE;
			}
			else
			{
				return XSIP_MESSAGE_KEEPALIVE;
			}
		}
		else if (strMsg.find(">Alarm<") != string::npos)
		{
			return XSIP_MESSAGE_ALARM;
		}
		else if (strMsg.find(">MediaStatus<")  != string::npos)
		{
			return XSIP_MESSAGE_MEDIASTATUS;
		}
	}
	else if (strMsg.find("<Control>") != string::npos)
	{
		if (strMsg.find("<PTZCmd>") != string::npos || strMsg.find(">DeviceControl<") != string::npos)
		{
			return XSIP_MESSAGE_PTZ_CMD;
		}
		else if (strMsg.find("<TeleBoot>") != string::npos)
		{
			if (strMsg.find(">Boot<") != string::npos)
			{
				return XSIP_MESSAGE_BOOT_CMD;
			}
			else if (strMsg.find(">Restart<") != string::npos)
			{
				return XSIP_MESSAGE_RESTART_CMD;
			}			
		}
		else if (strMsg.find("<RecordCmd>") != string::npos)
		{
			return XSIP_MESSAGE_RECORD_CMD;
		}
		else if (strMsg.find("<GuardCmd>") != string::npos)
		{
			return XSIP_MESSAGE_GUARD_CMD;
		}
		else if (strMsg.find("<AlarmCmd>") != string::npos)
		{
			return XSIP_MESSAGE_ALARM_CMD;
		}
		
	}
	else if (strMsg.find("<Query>"))
	{
		if (strMsg.find(">RecordInfo<") != string::npos)
		{
			return XSIP_MESSAGE_RECORDINFO;
		}
		else if (strMsg.find(">Catalog<") != string::npos)
		{
			return XSIP_MESSAGE_CATALOG_QUERY;
		}
		else if (strMsg.find(">DeviceInfo<") != string::npos)
		{
			return XSIP_MESSAGE_DVRINFO_QUERY;
		}
		else if (strMsg.find(">DeviceStatus<") != string::npos)
		{
			return XSIP_MESSAGE_DVRSTATUS_QUERY;
		}
        else if (strMsg.find(">AreaDetection<") != string::npos)
        {
            return XSIP_MESSAGE_AREADETECTION_QUERY;
        }
        else if (strMsg.find(">CrossDetection<") != string::npos)
        {
            return XSIP_MESSAGE_CROSSDETECTION_QUERY;
        }
        else if (strMsg.find(">Smart<") != string::npos)
        {
            return XSIP_MESSAGE_RECVSMART;
        }
	}
	return XSIP_MESSAGE_COUNT;
}

CONTENT_TYPE xsip::xsip_get_content_type(char *message)
{
	char enter[3] = {13, 10, 0};
	string sType = xsip_get_uri(message, "Content-Type: ", enter, MAX_LIST_LEN);
	if (sType == "")
	{
		return XSIP_APPLICATION_NONE;
	}
	if (sType.find("XML") != string::npos || sType.find("xml") != string::npos)
	{
		return XSIP_APPLICATION_XML;
	}
	else if (sType.find("SDP") != string::npos || sType.find("sdp") != string::npos)
	{
		return XSIP_APPLICATION_SDP;
	}
	else if (sType.find("RTSP") != string::npos || sType.find("rtsp") != string::npos)
	{
		return XSIP_APPLICATION_RTSP;
	}
	else
	{
		return XSIP_APPLICATION_NONE;
	}
}


string xsip::xsip_get_uri(char *message, const char *sBegin, const char *sEnd, int nMaxLen)
{
	string strMsg = message;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find(sBegin);
	if (nBegin != string::npos)
	{
		nBegin += strlen(sBegin);
		nEnd = strMsg.find(sEnd, nBegin);
		nLen = nEnd - nBegin;
		if (nLen <= 0 || (int)nLen > nMaxLen)
		{
			return "";
		}
		return strMsg.substr(nBegin, nLen);
	}
	return "";
}

string xsip::xsip_get_list(char *message, const char *sBegin, const char *sHead, const char *sTail, const char * sEnd, int nMaxLen)
{
	string strMsg = message;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.find(sBegin);
	if (nBegin != string::npos)
	{
		nBegin = strMsg.find(sHead, nBegin);
		if (nBegin == string::npos)
		{
			return "";
		}
		nEnd = strMsg.find(sTail, nBegin) - strlen(sEnd);
		nLen = nEnd - nBegin;
		if (nLen <= 0 || nLen > nMaxLen)
		{
			return "";
		}
		return strMsg.substr(nBegin, nLen);
	}
	return "";
}

int xsip::xsip_get_url_port(char *url)
{
	string strMsg = url;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strMsg.rfind(':');
	if (nBegin != string::npos)
	{
		nBegin += 1;
		nEnd = strMsg.size();
		nLen = nEnd - nBegin;
		if (nLen <= 0 || nLen > MAX_PORT_LEN)
		{
			return 0;
		}
		return atoi(strMsg.substr(nBegin, nEnd-nBegin).c_str());
	}
	return 0;
}

string xsip::xsip_get_url_username(char *url)
{
	return xsip_get_uri(url, "sip:", "@", MAX_USERNAME_LEN);
}

string xsip::xsip_get_url_host(char *url)
{
	return xsip_get_uri(url, "@", ":", MAX_HOST_LEN);
}

string xsip::xsip_get_via(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_list(message, "Via:", "SIP/2.0/UDP", enter, "", MAX_LIST_LEN); //"From:", "  ",
}

string xsip::xsip_get_via_username(char *message)
{
	return xsip_get_uri(message, "sip:", "@", MAX_USERNAME_LEN);
}

string xsip::xsip_get_via_addr(char *message)
{
	BOOL bRet = MSG_IS_STATUS(message);
	string from = "";
	if (bRet == FALSE)
	{
		from = xsip_get_from_url(message);
	}
	else
	{
		from = xsip_get_to_url(message);
	}
	string username = xsip_get_url_username((char *)from.c_str());
	string host = xsip_get_uri(message, "Via: SIP/2.0/UDP ", ";", 24);
    if(host == "")
    {
        host = xsip_get_uri(message, "VIA: SIP/2.0/UDP ", ";", 24);
    }
	string uri = "sip:";
	uri += username;
	uri += "@";
	uri += host;
	return uri;
}

string xsip::xsip_get_from(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_list(message, "From:", "<sip:", enter, "", MAX_LIST_LEN);	// "To:", "  ",
}

string xsip::xsip_get_from_url(char *message)
{
	string url;
	url =  xsip_get_list(message, "From:", "sip:", ">", "", MAX_LIST_LEN);
	if (url == "")
	{
		char enter[3] = {13, 10, 0};
		url = xsip_get_list(message, "From:", "sip:", enter, "", MAX_LIST_LEN);
	}
	return url;
}

string xsip::xsip_get_from_tag(char *message)
{
	char enter[3] = {13, 10, 0};
	string tag = xsip_get_uri(message, "tag=", enter, MAX_LIST_LEN);
	return tag;
}

string xsip::xsip_get_to(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_list(message, "To:", "<sip:", enter, "", MAX_LIST_LEN);	//"Call-ID:", "  ",
}

string xsip::xsip_get_to_url(char *message)
{
	string url;
	url =  xsip_get_list(message, "To:", "sip:", ">", "", MAX_LIST_LEN);
	if (url == "")
	{
		char enter[3] = {13, 10, 0};
		url = xsip_get_list(message, "To:", "sip:", enter, "", MAX_LIST_LEN);
	}
	return url;
}

string xsip::xsip_get_to_tag(char *message)
{
	char enter[3] = {13, 10, 0};
	string tag = xsip_get_list(message, "To:", "tag=", enter, "", MAX_LIST_LEN); // "Call-ID:", "  ",
	if (tag == "")
	{
		return "";
	}
	return tag.substr(strlen("tag="), tag.size()-strlen("tag="));
}

string xsip::xsip_get_call_id(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "Call-ID: ", enter, MAX_LIST_LEN);
}

unsigned long xsip::xsip_get_call_id_num(char *message)
{
	string sNum = xsip_get_uri(message, "Call-ID: ", "@", MAX_CSEQ_LEN);
	if (sNum == "")
	{
		char enter[3] = {13, 10, 0};
		sNum = xsip_get_uri(message, "Call-ID: ", enter, MAX_LIST_LEN);
	}
	return strtoul(sNum.c_str(), NULL, 10);
}

string xsip::xsip_get_cseq(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "CSeq: ", enter, MAX_LIST_LEN);
}

int xsip::xsip_get_cseq_number(char *message)
{
	string sCSeq = xsip_get_uri(message, "CSeq: ", " ", MAX_CSEQ_LEN);
	return atoi(sCSeq.c_str());
}

string xsip::xsip_get_cseq_method(char *message)
{
	string strMsg = message;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char enter[3] = {13, 10, 0};
	nBegin = strMsg.find("CSeq: ");
	if (nBegin != string::npos)
	{
		nBegin += strlen("CSeq: ");
		nBegin = strMsg.find(' ', nBegin) + 1;
		nEnd = strMsg.find(enter, nBegin);
		nLen = nEnd - nBegin;
		if (nLen <= 0 || nLen > MAX_LIST_LEN)
		{
			return "";
		}
		return strMsg.substr(nBegin, nLen);
	}
	return "";
}

string xsip::xsip_get_contact(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "Contact: ", enter, MAX_LIST_LEN);
}

string xsip::xsip_get_contact_url(char *message)
{
	string url = xsip_get_contact(message);
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = url.find('\"');
	if (nBegin == 0)
	{
		nBegin += 1;
		nEnd = url.find('\"', nBegin);
		nLen = nEnd - nBegin;
		if (nLen <=0 || nLen > MAX_USERNAME_LEN)
		{
			return "";
		}
		string username = url.substr(nBegin, nLen);
		nBegin = url.find("sip", nEnd);
		if (nBegin != string::npos)
		{
			nBegin += sizeof("sip");
			nEnd = url.find('>', nBegin);
			nLen = nEnd - nBegin;
			if (nLen <=0 || nLen > 24)
			{
				return "";
			}
			string host = url.substr(nBegin, nLen);
			url = "sip:" + username;
			url += "@";
			url += host;
			return url;
		}
		return "";
	}
	else
	{
		url =  xsip_get_list(message, "Contact:", "sip:", ">", "", MAX_LIST_LEN);
	}
	return url;
}

int xsip::xsip_get_expires(char *message)
{
	char enter[3] = {13, 10, 0};
	string strExpires = xsip_get_uri(message, "Expires: ", enter, MAX_LIST_LEN);
    if(strExpires == "")
    {
        strExpires = xsip_get_uri(message, "expires=", enter, MAX_LIST_LEN);
    }
	return atoi(strExpires.c_str());
}

string xsip::xsip_get_event(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "Event: ", enter, MAX_LIST_LEN);
}

string xsip::xsip_get_user_agent(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "User-Agent: ", enter, MAX_LIST_LEN);
}

string xsip::xsip_get_subject(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "Subject: ", enter, MAX_SUBJECT_LEN);
}

string xsip::xsip_get_subject_sendid(char *message)
{
	string subject = message;
	return subject.substr(0, 20);
}

string xsip::xsip_get_subject_recvid(char *subject)
{
	return xsip_get_uri(subject, ",", ":", MAX_USERNAME_LEN);
}

string xsip::xsip_get_authorization_username(char *message)
{
	return xsip_get_uri(message, "username=\"", "\"", MAX_USERNAME_LEN);
}

string xsip::xsip_get_authorization_realm(char *message)
{
	return xsip_get_uri(message, "realm=\"", "\"", MAX_REALM_LEN);
}

string xsip::xsip_get_authorization_nonce(char *message)
{
	return xsip_get_uri(message,"nonce=\"","\"",DEFAULT_LEN);
}

string xsip::xsip_get_authorization_uri(char *message)
{
	return xsip_get_uri(message,"uri=\"","\"",MAX_URI_LEN);
}

string xsip::xsip_get_authorization_response(char *message)
{
	return xsip_get_uri(message, "response=\"", "\"", MAX_STR_LEN);
}

string xsip::xsip_get_authorization_algorithm(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "algorithm=", enter, MAX_STR_LEN);
}

string xsip::xsip_get_www_authenticate_realm(char *message)
{
	return xsip_get_uri(message, "realm=\"", "\"", MAX_REALM_LEN);
}

string xsip::xsip_get_www_authenticate_nonce(char *message)
{
	return xsip_get_uri(message, "nonce=\"", "\"", DEFAULT_LEN);
}

string xsip::xsip_get_note_nonce(char *message)
{
	return xsip_get_uri(message, "nonce=\"", "\"", DEFAULT_LEN);
}

string xsip::xsip_get_note_algorithm(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "algorithm=", enter, MAX_STR_LEN);
}

string xsip::xsip_get_date(char *message)
{
	char enter[3] = {13, 10, 0};
	return xsip_get_uri(message, "Date:", enter, MAX_STR_LEN);
}

string xsip::xsip_get_body(char *message)
{
	string strMsg = message;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	char enter[3] = {13, 10, 0};

	int type = xsip_get_content_type(message);
	if (type == XSIP_APPLICATION_XML)
	{
		nBegin = strMsg.find("<?xml version=\"1.0\"");
		if (nBegin != string::npos)
		{
			nEnd = strMsg.size();
			nLen = nEnd - nBegin;
			if (nLen <= 0 || nLen > MAX_CONTENT_LEN)
			{
				return "";
			}
		}
        else 
        {
            return "";
        }
		return strMsg.substr(nBegin, nEnd-nBegin);
	}
	else if(type == XSIP_APPLICATION_SDP)
	{
		nBegin = strMsg.find("v=");
		if (nBegin != string::npos)
		{
			nEnd = strMsg.size();
			nLen = nEnd - nBegin;
			if (nLen <= 0 || nLen > MAX_CONTENT_LEN)
			{
				return "";
			}
			return strMsg.substr(nBegin, nEnd-nBegin);
		}
	}
	else
	{
		nBegin = strMsg.find("RTSP/1.0");
		if (nBegin != string::npos)
		{
			nBegin = strMsg.rfind(enter, nBegin);
			nBegin += 2;
			nEnd = strMsg.size();
			nLen = nEnd - nBegin;
			if (nLen <= 0 || nLen > MAX_CONTENT_LEN)
			{
				return "";
			}
			return strMsg.substr(nBegin, nEnd - nBegin);
		}
	}
	return "";
}

string xsip::xsip_get_sdp_o_id(const char *sdp)
{
	return xsip_get_uri((char *)sdp, "o=", " ", MAX_USERNAME_LEN);
}

string xsip::xsip_get_sdp_o_ip(const char *sdp)
{
	char enter[3] = {13, 10, 0};
	char line[2] = {10, 0};
	string str = xsip_get_uri((char *)sdp, "IP4 ", enter, MAX_HOST_LEN);
	if (str != "")
	{
		return str;
	}
	return xsip_get_uri((char *)sdp, "IP4", line, MAX_HOST_LEN);
}

INVITE_TYPE xsip::xsip_get_sdp_s_type(const char *sdp)
{
	//char enter[3] = {13, 10, 0};
	//char line[2] = {10, 0};
	//string str = xsip_get_uri((char *)sdp, "s=", enter, MAX_USERNAME_LEN);
	//if (str == "")
	//{
	//	str = xsip_get_uri((char *)sdp, "s=", line, MAX_USERNAME_LEN);
	//}
	//if (str == "Play" || str == "PLAY")
	//{
	//	return XSIP_INVITE_PLAY;
	//}
	//else if (str == "Playback" || str == "PLAYBACK")
	//{
	//	return XSIP_INVITE_PLAYBACK;
	//}
	//else if (str == "Download" || str == "DOWNLOAD")
	//{
	//	return XSIP_INVITE_DOWNLOAD;
	//}
	//else
	//{
	//	return XSIP_INVITE_MEDIASERVER;
	//}
	string str = sdp;
	if (str.find("s=Download") != string::npos || str.find("s=DOWNLOAD") != string::npos)
	{
		return XSIP_INVITE_DOWNLOAD;
	}
	else if (str.find("s=Playback") != string::npos || str.find("s=PLAYBACK") != string::npos)
	{
		return XSIP_INVITE_PLAYBACK;
	}
	else if (str.find("s=Play") != string::npos || str.find("s=PLAY") != string::npos)
	{
		return XSIP_INVITE_PLAY;
	}
	else
	{
		return XSIP_INVITE_MEDIASERVER;
	}
}

CONNECT_TYPE xsip::xsip_get_sdp_a_mode(const char *sdp)
{
	string strMsg = sdp;
	int nIndex = strMsg.find("recvonly");
	if (nIndex != string::npos)
	{
		return XSIP_RECVONLY;
	}
	nIndex = strMsg.find("sendonly");
	if (nIndex != string::npos)
	{
		return XSIP_SENDONLY;
	}
	nIndex = strMsg.find("sendrecv");
	if (nIndex != string::npos)
	{
		return XSIP_SENDRECV;
	}
	return XSIP_NONE;
}

string xsip::xsip_get_sdp_y_ssrc(const char *sdp)
{
	char enter[3] = {13, 10, 0};
	char line[2] = {10, 0};
	string str = xsip_get_uri((char *)sdp, "y=", enter, MAX_SSRC_LEN);
	if (str != "")
	{
		return str;
	}
	return xsip_get_uri((char *)sdp, "y=", line, MAX_SSRC_LEN);
}

string xsip::xsip_get_sdp_u_url(const char *sdp)
{
	char enter[3] = {13, 10, 0};
	char line[2] = {10, 0};
	string str = xsip_get_uri((char *)sdp, "u=", enter, MAX_URI_LEN);
	if (str != "")
	{
		return str;
	}
	return xsip_get_uri((char *)sdp, "u=", line, MAX_URI_LEN);
}

string xsip::xsip_get_sdp_t_time(const char *sdp)
{
	char enter[3] = {13, 10, 0};
	char line[2] = {10, 0};
	string str = xsip_get_uri((char *)sdp, "t=", enter, DEFAULT_LEN);
	if (str != "")
	{
		return str;
	}
	return xsip_get_uri((char *)sdp, "t=", line, DEFAULT_LEN);
}

int xsip::xsip_get_sdp_m_port(const char *sdp)
{
	string sPort = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = sPort.find("m=");
	if (nBegin != string::npos)
	{
		nBegin = sPort.find(' ', nBegin);
		if (nBegin == string::npos)
		{
			return 0;
		}
		nBegin += 1;
		nEnd = sPort.find(' ', nBegin);
		nLen = nEnd - nBegin;
		if (nLen <= 0 || (int)nLen > MAX_PORT_LEN)
		{
			return 0;
		}
		sPort =  sPort.substr(nBegin, nLen);
		return atoi(sPort.c_str());
	}
	return 0;
}

void xsip::xsip_get_sdp_a_load(const char *sdp, int *load, char *type)
{
	string strSdp = sdp;
	int nBegin = 0;
	int nEnd = 0;
	int nLen = 0;
	nBegin = strSdp.find("a=rtpmap");
	if (nBegin != string::npos)
	{
		nBegin += sizeof("a=rtpmap");
		nEnd = strSdp.find(' ', nBegin);
		nLen = nEnd - nBegin;
		if (nLen <=0 || nLen > 3)
		{
			return;
		}
		string sload = strSdp.substr(nBegin, nLen);
		*load = atoi(sload.c_str());
		nBegin = nEnd+1;
		nEnd = strSdp.find("/", nBegin);
		nLen = nEnd - nBegin;
		if (nLen <=0 || nLen > 24)
		{
			return;
		}
		string stype = strSdp.substr(nBegin, nLen);
		memcpy(type, stype.c_str(), stype.size());
	}
}

RTSP_TYPE xsip::xsip_get_rtsp_type(const char *rtsp)
{
	string strRtsp = rtsp;
	int nBegin = strRtsp.find(" ");
	if (nBegin != string::npos && nBegin <= MAX_REALM_LEN)
	{
        if (strRtsp.find("Scale") != string::npos)
		{
			return XSIP_RTSP_SCALE;
		}
		else if (strRtsp.find("PAUSE") != string::npos)
		{
			return XSIP_RTSP_PAUSE;
		}
        else if (strRtsp.find("npt=now-") != string::npos)
        {
            return XSIP_RTSP_REPLAY;
        }
        else if (strRtsp.find("Range") != string::npos)
        {
            return XSIP_RTSP_RANGEPLAY;
        }
		else if (strRtsp.find("TEARDOWN") != string::npos)
		{
			return XSIP_RTSP_TEARDOWN;
		}
		else 
		{
			return XSIP_RTSP_NONE;
		}
	}
	return XSIP_RTSP_NONE;
}

double xsip::xsip_get_rtsp_scale(const char *rtsp)
{
	char enter[3] = {13, 10, 0};
	string str = xsip_get_uri((char *)rtsp, "Scale: ", enter, MAX_CSEQ_LEN);
	if (str == "")
	{
		return 0;
	}
	return strtod(str.c_str(), NULL);
}

long xsip::xsip_get_rtsp_npt(const char *rtsp)
{
	string str = xsip_get_uri((char *)rtsp, "npt=", "-", MAX_CSEQ_LEN);
	if (str == "")
	{
		return 0;
	}
	return strtol(str.c_str(), NULL, 10);
}

string xsip::xsip_build_auth_response(string username, string realm, string password, string nonce, string uri)
{
	XMD5 md5H1;
	string str = username;
	str.append(":");
	str += realm;
	str.append(":");
	str += password;
	md5H1.update(str);
	string sH1 = md5H1.toString();

	XMD5 md5H2;
	str = "REGISTER:" + uri;
	md5H2.update(str);
	string sH2 = md5H2.toString();

	XMD5 md5H3;
	str = sH1 + ":" + nonce + ":" + sH2;
	md5H3.update(str);
	string response = md5H3.toString();
	
	return response;
}

bool xsip::xsip_check_auth_response(string response, string username, string realm, string password, string nonce, string uri)
{
	string answer = xsip_build_auth_response(username,realm,password,nonce,uri);
	if (response == answer)
	{
		return true;
	}
	return false;
}

