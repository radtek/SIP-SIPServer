#ifndef _SIP_DEFINE_H_
#define _SIP_DEFINE_H_

#define MAX_CONTENT_LEN		1024*20    //����ı�����
#define MAX_USERNAME_LEN	20      //����û�����
#define MAX_HOST_LEN		15      //����ַ����
#define MAX_PORT_LEN		5       //���˿ڳ���
#define MAX_REALM_LEN		10      //����򳤶�
#define MAX_CSEQ_LEN		10      //�����ų���
#define MAX_SSRC_LEN		10      //���SSRCֵ����
#define MAX_ADDR_LEN		50      //����߼���ַ����
#define MAX_SUBJECT_LEN		84      //������ⳤ��
#define MAX_LIST_LEN		256     //���ͷ�����ݳ���
#define MAX_URI_LEN			256     //�����Դ��������
#define MAX_STR_LEN			32      //Ĭ���ַ�������
#define DEFAULT_LEN			512     //Ĭ���ı�����

typedef enum xsip_event_type
{
	XSIP_EVENT_COUNT,

	XSIP_DEVICE_REG,			//�豸ע����Ϣ
	XSIP_REGSUPSERVER_SUCCESS,		//ע��ɹ�
	XSIP_REGSUPSERVER_FAILURE,		//ע��ʧ��

	XSIP_CALL_INVITE,				//�㲥��Ϣ
	XSIP_CALL_PROCEEDING,			//�㲥�ȴ�
	XSIP_CALL_INVITEANSWERED,				//�㲥�ɹ�
	XSIP_CALL_FAILURE,				//�㲥ʧ��
	XSIP_CALL_ACK,					//ȷ����Ϣ
	XSIP_CALL_CANCEL,				//�㲥ȡ��
	XSIP_CANCEL_ANSWERED,			//ȡ���ɹ�
	XSIP_CANCEL_FAILURE,			//ȡ��ʧ��
	XSIP_INFO_ANSWERED,
	XSIP_INFO_FAILURE,
	XSIP_CALL_BYE,					//�ر���Ϣ
	XSIP_BYE_ANSWERED,				//�رճɹ�
	XSIP_BYE_FAILURE,				//�ر�ʧ��

	XSIP_INFO_PLAY,					//�طſ���
	XSIP_INFO_TEARDOWN,				//�طŹر�

	XSIP_MESSAGE_NEW,				//������Ϣ
	XSIP_MESSAGE_PROCEEDING,		//����ȴ�
	XSIP_MESSAGE_ANSWERED,			//����ɹ�
	XSIP_MESSAGE_FAILURE,			//����ʧ��

	XSIP_SUBSCRIPTION_NEW,			//������Ϣ
	XSIP_SUBSCRIPTION_PROCEEDING,	//���ĵȴ�
	XSIP_SUBSCRIPTION_ANSWERED,		//���ĳɹ�
	XSIP_SUBSCRIPTION_FAILURE,		//����ʧ��
	XSIP_SUBSCRIPTION_NOTIFY,		//֪ͨ��Ϣ
	XSIP_NOTIFICATION_ANSWERED,		//֪ͨ�ɹ�
	XSIP_NOTIFICATION_FAILURE,		//֪ͨʧ��

    XSIP_DEVICE_QUERYINFO           //�����ѯ��Ϣ

}EVENT_TYPE;

typedef enum xsip_message_type
{
	XSIP_MESSAGE_COUNT,
	XSIP_MESSAGE_ALARM,					//����֪ͨ
	XSIP_MESSAGE_ALARM_RESPONSE,		//������Ӧ
    XSIP_MESSAGE_AREADETECTION_QUERY,         //�������
    XSIP_MESSAGE_AREADETECTION_RESPONSE,         //��������Ӧ
    XSIP_MESSAGE_CROSSDETECTION_QUERY,         //�������
    XSIP_MESSAGE_CROSSDETECTION_RESPONSE,         //��������Ӧ
    XSIP_MESSAGE_RECVSMART,             //�ͻ��˽���smart��Ϣ
	XSIP_MESSAGE_KEEPALIVE,				//״̬֪ͨ
	XSIP_MESSAGE_KEEPALIVE_RESPHONSE,	//״̬��Ӧ
	XSIP_MESSAGE_PTZ_CMD,				//��̨����
	XSIP_MESSAGE_BOOT_CMD,				//Զ������
	XSIP_MESSAGE_RESTART_CMD,			//��������
	XSIP_MESSAGE_ALARM_CMD,				//������λ
	XSIP_MESSAGE_RECORD_CMD,			//¼�����
	XSIP_MESSAGE_GUARD_CMD,				//��������
	XSIP_MESSAGE_CMD_RESPONSE,			//���ƻ�Ӧ
	XSIP_MESSAGE_CATALOG_QUERY,			//Ŀ¼��ѯ
	XSIP_MESSAGE_CATALOG_RESPONSE,		//Ŀ¼��Ӧ
	XSIP_MESSAGE_DVRINFO_QUERY,			//DVR��ѯ
	XSIP_MESSAGE_DVRINFO_RESPONSE,		//DVR��Ӧ
	XSIP_MESSAGE_DVRSTATUS_QUERY,		//DVR״̬��ѯ
	XSIP_MESSAGE_DVRSTATUS_RESPONSE,	//DVR״̬��Ӧ
	XSIP_MESSAGE_RECORDINFO,			//��Ƶ����
	XSIP_MESSAGE_RECORDINFO_RESPONSE,	//������Ӧ
	XSIP_MESSAGE_MEDIASTATUS,			//ý��״̬֪ͨ
	XSIP_MESSAGE_TIMEOUT_RESPHONSE		//��ʱ֪ͨ

}MESSAGE_TYPE;

typedef enum xsip_invite_type
{
	XSIP_INVITE_PLAY,
	XSIP_INVITE_PLAYBACK,
	XSIP_INVITE_DOWNLOAD,
	XSIP_INVITE_MEDIASERVER

}INVITE_TYPE;

typedef enum xsip_connect_type
{
	XSIP_RECVONLY,
	XSIP_SENDONLY,
	XSIP_SENDRECV,
	XSIP_NONE

}CONNECT_TYPE;

typedef enum xsip_order_type
{
	XSIP_RTSP_NONE,
	XSIP_RTSP_RANGEPLAY,
	XSIP_RTSP_PAUSE,
    XSIP_RTSP_SCALE,
    XSIP_RTSP_REPLAY,
	XSIP_RTSP_TEARDOWN
}RTSP_TYPE;

typedef enum xsip_subscrstate_type
{
	XSIP_UNKNOWN,
	XSIP_PENDING,
	XSIP_ACTIVE,
	XSIP_TERMINATED

}SS_TYPE;


typedef enum xsip_content_type
{
	XSIP_APPLICATION_NONE,
	XSIP_APPLICATION_XML,
	XSIP_APPLICATION_SDP,
	XSIP_APPLICATION_RTSP
	
}CONTENT_TYPE;

typedef enum xsip_ptz_type
{
	XSIP_PTZ_STOP,
	XSIP_PTZ_UP,
	XSIP_PTZ_DOWN,
	XSIP_PTZ_LEFT,
	XSIP_PTZ_RIGHT,
	XSIP_PTZ_FOCUS_IN,
	XSIP_PTZ_FOCUS_OUT,
	XSIP_PTZ_IRIS_IN,
	XSIP_PTZ_IRIS_OUT,
	XSIP_PTZ_ZOOM_IN,
	XSIP_PTZ_ZOOM_OUT,

	XSIP_PTZ_SET,
	XSIP_PTZ_GOTO,
	XSIP_PTZ_DELETE,

	XSIP_PTZ_LEFTUP,
	XSIP_PTZ_LEFTDOWN,
	XSIP_PTZ_RIGHTUP,
	XSIP_PTZ_RIGHTDOWN,

	XSIP_PTZ_ERROR

}PTZTYPE;

/*******ָ����**************************************/

#define CMD_PTZ_STOP			0x00	//��ֹ̨ͣ
#define CMD_PTZ_UP				0x08	//��̨����
#define CMD_PTZ_DOWN			0x04	//��̨����
#define CMD_PTZ_LEFT			0x02	//��̨����
#define CMD_PTZ_RIGHT			0x01	//��̨����
#define CMD_PTZ_LU				0x0a	//��̨����
#define CMD_PTZ_LD				0x06	//��̨����
#define CMD_PTZ_RU				0x09	//��̨����
#define CMD_PTZ_RD				0x05	//��̨����
#define CMD_PTZ_ZOOM_IN			0x10	//��ͷ�Ŵ�
#define CMD_PTZ_ZOOM_OUT		0x20	//��ͷ��С

#define CMD_PTZ_IRIS_IN			0x48	//��Ȧ�Ŵ�
#define CMD_PTZ_IRIS_OUT		0x44	//��Ȧ��С
#define CMD_PTZ_FOCUS_IN		0x42	//�ۼ���
#define CMD_PTZ_FOCUS_OUT		0x41	//�ۼ�Զ
#define CMD_PTZ_FI_STOP         0x40    //��̨FIֹͣ

#define CMD_PTZ_SET				0x81	//����Ԥ��λ
#define CMD_PTZ_GOTO			0x82	//����Ԥ��λ
#define CMD_PTZ_DELETE			0x83	//ɾ��Ԥ��λ

#endif
