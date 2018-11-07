#pragma once

#include "xsipdefine.h"
#include "xmd5.h"
#include <string>
using namespace std;

typedef class xsip
{
public:
	xsip(void);
public:
	~xsip(void);
public:
	void   xsip_user_agent_init(const char *useragent);							//初始化代理服务名称
	string xsip_from_init(const char *username, const char *host, int port);	//初始化from字段
	string xsip_to_init(const char *username, const char *host, int port);		//初始化to字段
	string xsip_proy_init(const char *host, int port);							//初始化上级SIP服务器地址
	string xsip_call_id_init(unsigned long number, const char *host = NULL);	//初始化call-id字段
	string xsip_call_id_init(string number = "", const char *host = NULL);	
	string xsip_contact_init(const char *username, const char *host, int port);	//初始化contact字段
	string xsip_ssrc_init(int type, unsigned long ssrc);						//type: 0 实行 1 历史
	string xsip_subject_init(const char *to, const char * sSSRC, const char *from, int nHandle);	//初始化subject字段

public:
	//初始化XSIP参数
	int xsip_init_t(const char* username, const char *password, const char* ip, int port);

	//生成注册消息 expires为零时为注销消息
	int xsip_register_build_initial(char *reg,const char *proxy, const char *to, string callid="",int cseq=1,string ftag="",int expires=3600,const char *from=NULL,const char *contact =NULL);
	
	//生成自定义消息
	int xsip_message_build_request(char *message,const char *method,const char *proxy=NULL,const char *to=NULL,const char *from=NULL,const char *callid=NULL,int cseq=20,string ftag="",string ttag="");

	//生成自定义消息回应
	int xsip_message_build_answer(char *message, int status, char *answer, string tag = "");

	//生成自定义消息应答
	int xsip_message_build_response(char *message, char *response);

	//生成订阅消息
	int xsip_subscribe_build_initial(char *subscribe, const char *to, const char *from=NULL, char *et="presence", int expires=90);

	//生成订阅息回应
	int xsip_subscribe_build_answer(char *subscribe, int status, char *answer, string ttag = "");

	//生成通知消息
	int xsip_subscribe_build_notify(char *notify, const char *to,string callid="",int cseq=21,string ttag="", string ftag="",const char *from=NULL,const char *et="presence");

	//生成通知消息回应
	int xsip_notify_build_answer(char *notify, int status, char *answer);

	//生成点播消息
	int xsip_call_build_invite(char *invite,const char *to,const char *subject,const char *from=NULL,string ftag="",string callid="");

	//取消点播消息
	int xsip_call_build_cancel(char *cancel,const char*to,const char *from,string ftag,string callid,int ncseq=20);

	//生成点播消息回应
	int xsip_call_build_answer(char *invite, int status, char *answer, string ttag="");

	//生成确认消息
	int xsip_call_build_ack(char *answer, char *ack);

	//生成关闭消息
    int xsip_call_build_bye(char *answer, char *bye, bool bCancel = false, int nCseq = 0);
	int xsip_call_build_bye(char *bye, const char *to,string callid="",string ttag="", string ftag="",int cseq=21,const char *from=NULL);

	//生成关闭消息回应
	int xsip_bye_build_answer(char *bye, int status, char *answer);

	int xsip_info_build_initial(char *message,const char *method,const char *proxy=NULL,const char *to=NULL,const char *from=NULL,const char *callid=NULL,int cseq=20,string ftag="",string ttag="");

	//生成SDP描述 
	//type: 0 实时 1 历史 2 下载, mode: 0 recvonly 1 sendonly 2 sendrecv, time:起始时间 结束时间(/秒), mamuf: DAHUA HIKVISION H3C H264 MPEG4
	int xsip_sdp_build_initial(char *sdp,char *username,const char *ip,int port,int type=0,int mode=0,char *mamuf=NULL,char *uri=NULL,char *ssrc=NULL,char *time="0 0",char *fm=NULL, int nNetType = 1);

	//解析PTZ指令
	//参数 msg-收到的设备控制消息, speed-返回PTZ控制速度
	//返回 见sipdefine
	PTZTYPE xsip_ptzcmd_parse(char *msg, long *speed);

public:
	int  MSG_IS_STATUS(char *msg);					//是否是回应
	bool MSG_IS_MSG(char *msg, const char *head);	//是否是请求
	bool MSG_IS_REGISTER(char *msg);				//是否是注册
	bool MSG_IS_NOTIFY(char *msg);					//是否是通知
	bool MSG_IS_SUBSCRIBE(char *msg);				//是否是订阅
	bool MSG_IS_INVITE(char *msg);					//是否是点播
	bool MSG_IS_ACK(char *msg);						//是否是确认
	bool MSG_IS_BYE(char *msg);						//是否是结束
	bool MSG_IS_MESSAGE(char *msg);					//是否是消息
	bool MSG_IS_CANCEL(char *msg);					//是否是取消
	bool MSG_IS_INFO(char *msg);					//是否是控制
	bool MSG_IS_STATUS_1XX(char *msg);				//1XX回应
	bool MSG_IS_STATUS_2XX(char *msg);				//2XX回应
	bool MSG_IS_STATUS_3XX(char *msg);				//3XX回应
	bool MSG_IS_STATUS_4XX(char *msg);				//4XX回应
	bool MSG_IS_STATUS_5XX(char *msg);				//5XX回应
    bool MSG_IS_QUERYINFO(char *msg);				//是否是服务信息查询

    
public:
	string xsip_set_uri(char *msg, const char *Field, const char *pos, const char*value);
	string xsip_set_proxy(char *msg, const char *proxy);					//设置接收服务地址
	string xsip_set_via_addr(char *msg, const char *ip, int port);			//设置via字段
	string xsip_set_Route(char *msg, const char route);						//设置Route字段
	string xsip_set_from(char *msg, const char *from);						//设置from字段
	string xsip_set_to(char *msg, const char *to);							//设置to字段
	string xsip_set_call_id(char *msg, const char *callid);					//设置callid字段
	string xsip_set_contact(char *msg, const char *contact);				//设置contact字段
	string xsip_set_content_type(char *msg, const char *content);			//增加content_type字段
	string xsip_set_content_type(char *msg, int type);						//type: 1 xml 2 sdp 3 rtsp !插入附加信息时先设置这个
	string xsip_set_authorization(char *msg, const char *authorization);	//增加authorization字段
	string xsip_set_www_authenticate(char *msg, const char *authenticate);	//增加www_authenticate字段
	string xsip_set_note(char *msg, const char *note);						//增加note字段
	string xsip_set_date(char *msg, const char *date);						//增加date字段
	string xsip_set_expires(char *msg, int expires);						//增加expires段
	string xsip_set_event(char *msg, const char *et);						//增加event字段
	string xsip_set_subject(char *msg, const char *subject);				//增加subject字段
	string xsip_set_monitor_user_identity(char *msg, const char *cmd);
	string xsip_set_subscriptoin_state(char *msg, int ss=2, int ex=90, int re=0);//增加subscriptoin_state字段
	string xsip_set_body(char *msg, char *body, int len);					//插入Content附加信息
    string xsip_set_body_sn(char * body, char * sn, int len);                  //在body中替换sn

	string xsip_set_sdp_v(const char *sdp, const char *item, const char *value);
	string xsip_set_sdp_o(const char *sdp, const char *value);				//替换SDP_O值
	string xsip_set_sdp_s(const char *sdp, const char *value);				//替换SDP_S值
	string xsip_set_sdp_c(const char *sdp, const char *value);				//替换SDP_C值
	string xsip_set_sdp_t(const char *sdp, const char *value);
	string xsip_set_sdp_m_port(const char *sdp, int port);					//替换SDP_M值
	string xsip_set_sdp_a(const char *sdp, const char *value);				//替换SDP_A连接模式值
	string xsip_set_sdp_u(const char *sdp, const char *value);				//插入SDP_U值
	string xsip_set_sdp_y(const char *sdp, const char *value);				//插入SDP_Y值
	string xsip_set_sdp_f(const char *sdp, const char *value);				//插入SDP_F值
	string xsip_set_sdp_f(const char *sdp, int nResolution);
    string xsip_set_sdp_downloadspeed(const char *sdp, const char *value);				//插入downloadspeed值

	string xsip_set_sdp_o_c(const char *sdp, const char *value);
	string xsip_get_sdp_o_c(const char *sdp);

public:
	EVENT_TYPE xsip_get_event_type(char *message);
	MESSAGE_TYPE xsip_get_message_type(char *message);
	CONTENT_TYPE xsip_get_content_type(char *message);
	string xsip_get_uri(char *message, const char *sBegin, const char *sEnd, int nMaxLen);
	string xsip_get_list(char *message, const char *sBegin, const char *sHead, const char *sTail, const char * sEnd, int nMaxLen);
	
	unsigned long xsip_get_num();
    string        xsip_get_tag();									//生成随机数
    string        xsip_get_CallIDNum();									//生成CallID随机数
	string        xsip_get_nonce();									//生成随机串
	string        xsip_get_url_username(char *url);					//获得URI里的ID
	string        xsip_get_url_host(char *url);						//获得URI里的IP
	int           xsip_get_url_port(char *url);						//获得URI里的端口
	string        xsip_get_via(char *message);						//获得消息里的VIA内容
	string		  xsip_get_via_username(char *message);				//获得消息里的VIA目的编码
	string        xsip_get_via_addr(char *message);					//获得消息里的VIA接收地址内容
	string        xsip_get_from(char *message);						//获得消息里的FROM内容
	string        xsip_get_from_url(char *message);					//获得消息里的FROM-URI内容
	string        xsip_get_from_tag(char *message);					//获得消息里的FROM-TAG内容
	string        xsip_get_to(char *message);						//获得消息里的TO内容
	string        xsip_get_to_url(char *message);					//获得消息里的TO-URI内容
	string        xsip_get_to_tag(char *message);					//获得消息里的TO-TAG内容
	string        xsip_get_call_id(char *message);					//获得消息里的CALL_ID内容
	unsigned long xsip_get_call_id_num(char *message);				//获得消息里的CALL_ID-NUM内容
	string        xsip_get_cseq(char *message);						//获得消息里的CSEQ内容
	int           xsip_get_cseq_number(char *message);				//获得消息里的CSEQ序列号
	string        xsip_get_cseq_method(char *message);				//获得消息里的CSEQ消息头
	string        xsip_get_contact(char *message);					//获得消息里的CONTACT内容
	string        xsip_get_contact_url(char *message);				//获得消息里的CONTACT-URI内容
	string        xsip_get_authorization_username(char *message);	//获得消息里的AUTHORIZATION-USERNAME内容
	string        xsip_get_authorization_realm(char *message);		//获得消息里的AUTHORIZATION-REALM内容
	string        xsip_get_authorization_nonce(char *message);		//获得消息里的AUTHORIZATION-NONCE内容
	string        xsip_get_authorization_uri(char *message);		//获得消息里的AUTHORIZATION-URI内容
	string        xsip_get_authorization_response(char *message);	//获得消息里的AUTHORIZATION-RESPHONE内容
	string        xsip_get_authorization_algorithm(char *message);	//获得消息里的AUTHORIZATION-ALGORITHM内容
	string        xsip_get_www_authenticate_realm(char *message);	//获得消息里的WWW_AUTHENTICATE-REALM内容
	string        xsip_get_www_authenticate_nonce(char *message);	//获得消息里的WWW_AUTHENTICATE-NONCE内容
	string        xsip_get_note_nonce(char *message);				//获得消息里的NOTE-NONCE内容
	string        xsip_get_note_algorithm(char *message);			//获得消息里的NOTE-ALGORITHM内容
	string        xsip_get_date(char *message);						//获得消息里的DATE内容
	int           xsip_get_expires(char *message);					//获得消息里的EXPIRES内容
	string        xsip_get_event(char *message);					//获得消息里的EVENT内容
	string        xsip_get_subject(char *message);					//获得消息里的SUBJECT内容
	string		  xsip_get_subject_sendid(char *subject);
	string		  xsip_get_subject_recvid(char *subject);	
	string		  xsip_get_user_agent(char *message);
	string        xsip_get_body(char *message);						//获得消息里的CONTENT附加内容

	INVITE_TYPE   xsip_get_sdp_s_type(const char *sdp);				//获得SDP里的请求类型
	CONNECT_TYPE  xsip_get_sdp_a_mode(const char *sdp);				//获得SDP里的连接模式
	int           xsip_get_sdp_m_port(const char *sdp);				//获得SDP里的端口信息
	string        xsip_get_sdp_o_ip(const char *sdp);				//获得SDP里的地址信息
	string        xsip_get_sdp_o_id(const char *sdp);				//获得SDP里的会话ID
	string        xsip_get_sdp_t_time(const char *sdp);				//获得SDP里的时间信息
	string        xsip_get_sdp_u_url(const char *sdp);				//获得SDP里的连接信息
	string        xsip_get_sdp_y_ssrc(const char *sdp);				//获得SDP里的SSRC值
	void          xsip_get_sdp_a_load(const char *sdp, int *load, char *type);

	RTSP_TYPE     xsip_get_rtsp_type(const char *rtsp);				//获得RTSP里的控制类型
	double        xsip_get_rtsp_scale(const char *rtsp);			//获得RTSP里的播放速度
	long          xsip_get_rtsp_npt(const char *rtsp);				//获得RTSP里的拖放位置

public:
	string xsip_build_auth_response(string username, string realm, string password, string nonce, string uri);
	bool xsip_check_auth_response(string response, string username, string realm, string password, string nonce, string uri);

public:
	unsigned int	m_nCSeq;		//序列号
	unsigned short	m_nPort;		//服务端口
	string			m_sHost;		//服务地址
	string			m_sUsername;	//服务ID
	string			m_sPassword;	//服务密码
	string			m_sFrom;		//服务From字段
	string			m_sContact;		//服务Contact字段
	string			m_sUserAgent;	//服务代理名
	string			m_sRealm;		//服务域名
	string			m_sProxy;		//服务注册名

}XSIP, *LPXSIP;
