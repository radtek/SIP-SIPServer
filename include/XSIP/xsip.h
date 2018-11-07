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
	void   xsip_user_agent_init(const char *useragent);							//��ʼ�������������
	string xsip_from_init(const char *username, const char *host, int port);	//��ʼ��from�ֶ�
	string xsip_to_init(const char *username, const char *host, int port);		//��ʼ��to�ֶ�
	string xsip_proy_init(const char *host, int port);							//��ʼ���ϼ�SIP��������ַ
	string xsip_call_id_init(unsigned long number, const char *host = NULL);	//��ʼ��call-id�ֶ�
	string xsip_call_id_init(string number = "", const char *host = NULL);	
	string xsip_contact_init(const char *username, const char *host, int port);	//��ʼ��contact�ֶ�
	string xsip_ssrc_init(int type, unsigned long ssrc);						//type: 0 ʵ�� 1 ��ʷ
	string xsip_subject_init(const char *to, const char * sSSRC, const char *from, int nHandle);	//��ʼ��subject�ֶ�

public:
	//��ʼ��XSIP����
	int xsip_init_t(const char* username, const char *password, const char* ip, int port);

	//����ע����Ϣ expiresΪ��ʱΪע����Ϣ
	int xsip_register_build_initial(char *reg,const char *proxy, const char *to, string callid="",int cseq=1,string ftag="",int expires=3600,const char *from=NULL,const char *contact =NULL);
	
	//�����Զ�����Ϣ
	int xsip_message_build_request(char *message,const char *method,const char *proxy=NULL,const char *to=NULL,const char *from=NULL,const char *callid=NULL,int cseq=20,string ftag="",string ttag="");

	//�����Զ�����Ϣ��Ӧ
	int xsip_message_build_answer(char *message, int status, char *answer, string tag = "");

	//�����Զ�����ϢӦ��
	int xsip_message_build_response(char *message, char *response);

	//���ɶ�����Ϣ
	int xsip_subscribe_build_initial(char *subscribe, const char *to, const char *from=NULL, char *et="presence", int expires=90);

	//���ɶ���Ϣ��Ӧ
	int xsip_subscribe_build_answer(char *subscribe, int status, char *answer, string ttag = "");

	//����֪ͨ��Ϣ
	int xsip_subscribe_build_notify(char *notify, const char *to,string callid="",int cseq=21,string ttag="", string ftag="",const char *from=NULL,const char *et="presence");

	//����֪ͨ��Ϣ��Ӧ
	int xsip_notify_build_answer(char *notify, int status, char *answer);

	//���ɵ㲥��Ϣ
	int xsip_call_build_invite(char *invite,const char *to,const char *subject,const char *from=NULL,string ftag="",string callid="");

	//ȡ���㲥��Ϣ
	int xsip_call_build_cancel(char *cancel,const char*to,const char *from,string ftag,string callid,int ncseq=20);

	//���ɵ㲥��Ϣ��Ӧ
	int xsip_call_build_answer(char *invite, int status, char *answer, string ttag="");

	//����ȷ����Ϣ
	int xsip_call_build_ack(char *answer, char *ack);

	//���ɹر���Ϣ
    int xsip_call_build_bye(char *answer, char *bye, bool bCancel = false, int nCseq = 0);
	int xsip_call_build_bye(char *bye, const char *to,string callid="",string ttag="", string ftag="",int cseq=21,const char *from=NULL);

	//���ɹر���Ϣ��Ӧ
	int xsip_bye_build_answer(char *bye, int status, char *answer);

	int xsip_info_build_initial(char *message,const char *method,const char *proxy=NULL,const char *to=NULL,const char *from=NULL,const char *callid=NULL,int cseq=20,string ftag="",string ttag="");

	//����SDP���� 
	//type: 0 ʵʱ 1 ��ʷ 2 ����, mode: 0 recvonly 1 sendonly 2 sendrecv, time:��ʼʱ�� ����ʱ��(/��), mamuf: DAHUA HIKVISION H3C H264 MPEG4
	int xsip_sdp_build_initial(char *sdp,char *username,const char *ip,int port,int type=0,int mode=0,char *mamuf=NULL,char *uri=NULL,char *ssrc=NULL,char *time="0 0",char *fm=NULL, int nNetType = 1);

	//����PTZָ��
	//���� msg-�յ����豸������Ϣ, speed-����PTZ�����ٶ�
	//���� ��sipdefine
	PTZTYPE xsip_ptzcmd_parse(char *msg, long *speed);

public:
	int  MSG_IS_STATUS(char *msg);					//�Ƿ��ǻ�Ӧ
	bool MSG_IS_MSG(char *msg, const char *head);	//�Ƿ�������
	bool MSG_IS_REGISTER(char *msg);				//�Ƿ���ע��
	bool MSG_IS_NOTIFY(char *msg);					//�Ƿ���֪ͨ
	bool MSG_IS_SUBSCRIBE(char *msg);				//�Ƿ��Ƕ���
	bool MSG_IS_INVITE(char *msg);					//�Ƿ��ǵ㲥
	bool MSG_IS_ACK(char *msg);						//�Ƿ���ȷ��
	bool MSG_IS_BYE(char *msg);						//�Ƿ��ǽ���
	bool MSG_IS_MESSAGE(char *msg);					//�Ƿ�����Ϣ
	bool MSG_IS_CANCEL(char *msg);					//�Ƿ���ȡ��
	bool MSG_IS_INFO(char *msg);					//�Ƿ��ǿ���
	bool MSG_IS_STATUS_1XX(char *msg);				//1XX��Ӧ
	bool MSG_IS_STATUS_2XX(char *msg);				//2XX��Ӧ
	bool MSG_IS_STATUS_3XX(char *msg);				//3XX��Ӧ
	bool MSG_IS_STATUS_4XX(char *msg);				//4XX��Ӧ
	bool MSG_IS_STATUS_5XX(char *msg);				//5XX��Ӧ
    bool MSG_IS_QUERYINFO(char *msg);				//�Ƿ��Ƿ�����Ϣ��ѯ

    
public:
	string xsip_set_uri(char *msg, const char *Field, const char *pos, const char*value);
	string xsip_set_proxy(char *msg, const char *proxy);					//���ý��շ����ַ
	string xsip_set_via_addr(char *msg, const char *ip, int port);			//����via�ֶ�
	string xsip_set_Route(char *msg, const char route);						//����Route�ֶ�
	string xsip_set_from(char *msg, const char *from);						//����from�ֶ�
	string xsip_set_to(char *msg, const char *to);							//����to�ֶ�
	string xsip_set_call_id(char *msg, const char *callid);					//����callid�ֶ�
	string xsip_set_contact(char *msg, const char *contact);				//����contact�ֶ�
	string xsip_set_content_type(char *msg, const char *content);			//����content_type�ֶ�
	string xsip_set_content_type(char *msg, int type);						//type: 1 xml 2 sdp 3 rtsp !���븽����Ϣʱ���������
	string xsip_set_authorization(char *msg, const char *authorization);	//����authorization�ֶ�
	string xsip_set_www_authenticate(char *msg, const char *authenticate);	//����www_authenticate�ֶ�
	string xsip_set_note(char *msg, const char *note);						//����note�ֶ�
	string xsip_set_date(char *msg, const char *date);						//����date�ֶ�
	string xsip_set_expires(char *msg, int expires);						//����expires��
	string xsip_set_event(char *msg, const char *et);						//����event�ֶ�
	string xsip_set_subject(char *msg, const char *subject);				//����subject�ֶ�
	string xsip_set_monitor_user_identity(char *msg, const char *cmd);
	string xsip_set_subscriptoin_state(char *msg, int ss=2, int ex=90, int re=0);//����subscriptoin_state�ֶ�
	string xsip_set_body(char *msg, char *body, int len);					//����Content������Ϣ
    string xsip_set_body_sn(char * body, char * sn, int len);                  //��body���滻sn

	string xsip_set_sdp_v(const char *sdp, const char *item, const char *value);
	string xsip_set_sdp_o(const char *sdp, const char *value);				//�滻SDP_Oֵ
	string xsip_set_sdp_s(const char *sdp, const char *value);				//�滻SDP_Sֵ
	string xsip_set_sdp_c(const char *sdp, const char *value);				//�滻SDP_Cֵ
	string xsip_set_sdp_t(const char *sdp, const char *value);
	string xsip_set_sdp_m_port(const char *sdp, int port);					//�滻SDP_Mֵ
	string xsip_set_sdp_a(const char *sdp, const char *value);				//�滻SDP_A����ģʽֵ
	string xsip_set_sdp_u(const char *sdp, const char *value);				//����SDP_Uֵ
	string xsip_set_sdp_y(const char *sdp, const char *value);				//����SDP_Yֵ
	string xsip_set_sdp_f(const char *sdp, const char *value);				//����SDP_Fֵ
	string xsip_set_sdp_f(const char *sdp, int nResolution);
    string xsip_set_sdp_downloadspeed(const char *sdp, const char *value);				//����downloadspeedֵ

	string xsip_set_sdp_o_c(const char *sdp, const char *value);
	string xsip_get_sdp_o_c(const char *sdp);

public:
	EVENT_TYPE xsip_get_event_type(char *message);
	MESSAGE_TYPE xsip_get_message_type(char *message);
	CONTENT_TYPE xsip_get_content_type(char *message);
	string xsip_get_uri(char *message, const char *sBegin, const char *sEnd, int nMaxLen);
	string xsip_get_list(char *message, const char *sBegin, const char *sHead, const char *sTail, const char * sEnd, int nMaxLen);
	
	unsigned long xsip_get_num();
    string        xsip_get_tag();									//���������
    string        xsip_get_CallIDNum();									//����CallID�����
	string        xsip_get_nonce();									//���������
	string        xsip_get_url_username(char *url);					//���URI���ID
	string        xsip_get_url_host(char *url);						//���URI���IP
	int           xsip_get_url_port(char *url);						//���URI��Ķ˿�
	string        xsip_get_via(char *message);						//�����Ϣ���VIA����
	string		  xsip_get_via_username(char *message);				//�����Ϣ���VIAĿ�ı���
	string        xsip_get_via_addr(char *message);					//�����Ϣ���VIA���յ�ַ����
	string        xsip_get_from(char *message);						//�����Ϣ���FROM����
	string        xsip_get_from_url(char *message);					//�����Ϣ���FROM-URI����
	string        xsip_get_from_tag(char *message);					//�����Ϣ���FROM-TAG����
	string        xsip_get_to(char *message);						//�����Ϣ���TO����
	string        xsip_get_to_url(char *message);					//�����Ϣ���TO-URI����
	string        xsip_get_to_tag(char *message);					//�����Ϣ���TO-TAG����
	string        xsip_get_call_id(char *message);					//�����Ϣ���CALL_ID����
	unsigned long xsip_get_call_id_num(char *message);				//�����Ϣ���CALL_ID-NUM����
	string        xsip_get_cseq(char *message);						//�����Ϣ���CSEQ����
	int           xsip_get_cseq_number(char *message);				//�����Ϣ���CSEQ���к�
	string        xsip_get_cseq_method(char *message);				//�����Ϣ���CSEQ��Ϣͷ
	string        xsip_get_contact(char *message);					//�����Ϣ���CONTACT����
	string        xsip_get_contact_url(char *message);				//�����Ϣ���CONTACT-URI����
	string        xsip_get_authorization_username(char *message);	//�����Ϣ���AUTHORIZATION-USERNAME����
	string        xsip_get_authorization_realm(char *message);		//�����Ϣ���AUTHORIZATION-REALM����
	string        xsip_get_authorization_nonce(char *message);		//�����Ϣ���AUTHORIZATION-NONCE����
	string        xsip_get_authorization_uri(char *message);		//�����Ϣ���AUTHORIZATION-URI����
	string        xsip_get_authorization_response(char *message);	//�����Ϣ���AUTHORIZATION-RESPHONE����
	string        xsip_get_authorization_algorithm(char *message);	//�����Ϣ���AUTHORIZATION-ALGORITHM����
	string        xsip_get_www_authenticate_realm(char *message);	//�����Ϣ���WWW_AUTHENTICATE-REALM����
	string        xsip_get_www_authenticate_nonce(char *message);	//�����Ϣ���WWW_AUTHENTICATE-NONCE����
	string        xsip_get_note_nonce(char *message);				//�����Ϣ���NOTE-NONCE����
	string        xsip_get_note_algorithm(char *message);			//�����Ϣ���NOTE-ALGORITHM����
	string        xsip_get_date(char *message);						//�����Ϣ���DATE����
	int           xsip_get_expires(char *message);					//�����Ϣ���EXPIRES����
	string        xsip_get_event(char *message);					//�����Ϣ���EVENT����
	string        xsip_get_subject(char *message);					//�����Ϣ���SUBJECT����
	string		  xsip_get_subject_sendid(char *subject);
	string		  xsip_get_subject_recvid(char *subject);	
	string		  xsip_get_user_agent(char *message);
	string        xsip_get_body(char *message);						//�����Ϣ���CONTENT��������

	INVITE_TYPE   xsip_get_sdp_s_type(const char *sdp);				//���SDP�����������
	CONNECT_TYPE  xsip_get_sdp_a_mode(const char *sdp);				//���SDP�������ģʽ
	int           xsip_get_sdp_m_port(const char *sdp);				//���SDP��Ķ˿���Ϣ
	string        xsip_get_sdp_o_ip(const char *sdp);				//���SDP��ĵ�ַ��Ϣ
	string        xsip_get_sdp_o_id(const char *sdp);				//���SDP��ĻỰID
	string        xsip_get_sdp_t_time(const char *sdp);				//���SDP���ʱ����Ϣ
	string        xsip_get_sdp_u_url(const char *sdp);				//���SDP���������Ϣ
	string        xsip_get_sdp_y_ssrc(const char *sdp);				//���SDP���SSRCֵ
	void          xsip_get_sdp_a_load(const char *sdp, int *load, char *type);

	RTSP_TYPE     xsip_get_rtsp_type(const char *rtsp);				//���RTSP��Ŀ�������
	double        xsip_get_rtsp_scale(const char *rtsp);			//���RTSP��Ĳ����ٶ�
	long          xsip_get_rtsp_npt(const char *rtsp);				//���RTSP����Ϸ�λ��

public:
	string xsip_build_auth_response(string username, string realm, string password, string nonce, string uri);
	bool xsip_check_auth_response(string response, string username, string realm, string password, string nonce, string uri);

public:
	unsigned int	m_nCSeq;		//���к�
	unsigned short	m_nPort;		//����˿�
	string			m_sHost;		//�����ַ
	string			m_sUsername;	//����ID
	string			m_sPassword;	//��������
	string			m_sFrom;		//����From�ֶ�
	string			m_sContact;		//����Contact�ֶ�
	string			m_sUserAgent;	//���������
	string			m_sRealm;		//��������
	string			m_sProxy;		//����ע����

}XSIP, *LPXSIP;
