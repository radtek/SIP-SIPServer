#pragma once
#include <string>
using namespace std;

class CParamRead
{
public:
	CParamRead(void);
public:
	~CParamRead(void);
public:
	bool InitParam();
	string GetDir();
	void ReadParameter(string sSection, string sEntry, string &sValue, string sFileName = "");
	void ReadParameter(string sSection, string sEntry, int &nValue, string sFileName = "");
	bool ReadParameter(string sSection, string sEntry, void * pValue, int nSize, string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, string sValue,  string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, int iValue,  string sFileName = "");
	bool WriteParameter(string sSection, string sEntry, void * pValue, int nSize, string sFileName = "");
public:
    string m_sPath;         //����·��
	string m_sProfileName;	//����·��

	string m_sDBServer;		//���ݿ������
	string m_sDBName;		//���ݿ���
	string m_sDBUid;		//�û���
	string m_sDBPwd;		//�û�����
	int m_nDBDriver;		//���ݿ�����

    //�������ݿ���Ϣ
    string m_sLGDBServer;	//���ݿ������
    string m_sLGDBName;		//���ݿ���
    string m_sLGDBUid;		//�û���
    string m_sLGDBPwd;		//�û�����
    //����

    string m_sSIPServerID;
    string m_sLocalPassword;
    string m_sLocalIP;
    int m_nLocalPort;

	BOOL m_bAuthenticate;	//�Ƿ�Ҫ��֤
	BOOL m_bSubList;		//�Ƿ����¼�����Ŀ¼		

    int m_nSearchDevice;   //�Ƿ��ѯ�¼��豸Ŀ¼
    int m_nSearchPlat;     //�Ƿ��ѯ�¼�ƽ̨Ŀ¼
    int m_nWriteDB;        //�Ƿ�Ŀ¼��Ϣд�����ݿ�

    int m_bFileStream;      //�Ƿ������ļ���ý��
    int m_bISINITIAL;       //�Ƿ�������������
    int m_nInitialTime;     //������ʱʱ��(S)

    string m_sAssignLowServer;
    string m_sAssignRTPServer;
    int m_nNetType;

	string m_sSuperiorServer;		//SIP��������ַ
    BOOL m_bRegSupServer;		//�Ƿ�Ҫ���ϼ�ע��
    string m_sRegSupUsername;   //���ϼ�ע���û���
    string m_sRegSupPassword;   //���ϼ�ע������


    

};
