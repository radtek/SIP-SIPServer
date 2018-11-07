#ifndef   _NVSSDKDEF_H
#define   _NVSSDKDEF_H
//V1.5 版本    (与V1.4版本一样头文件)
//#include "NVSTypeDefine.h"

typedef struct _tagCLInfo
{
    char sChannelName[32];
    unsigned char nChannelNum;
    char sChannelNo[44];
}CLInfo;

#define DVR_DATA_INIT    1 //预备数据
#define DVR_DATA_STREAM  2 //流数据

//#define DVR_RECORDFILE_ALL      0
//#define DVR_RECORDFILE_MANUAL   1
//#define DVR_RECORDFILE_WARNING  2
//#define DVR_RECORDFILE_TIME     3

#define XY_PROPERTY_ISSUPPORT_PACK 0//查询是需要发送原始包(即不可分包) ,返回true:是  false:否
#define XY_PROPERTY_ISHAVEHEAD     1//查询是有包头 true:是  false:否

typedef  void(CALLBACK  *LPCAPTURESTREAMCALLBACK)(LONG  identify,int nDataType,BYTE* pBuffer,int nBufferSize,void* pUser);

typedef  void(CALLBACK  *LPGETRECORDFILECALLBACK)(LONG  identify,bool bValidate,char* sFileName,char* startTime,char* endTime,int nFileSize,void* pUser);

typedef  void(CALLBACK  *LPDOWNLOADRECORDFILECALLBACK)(LONG  identify,char* sFileName,int nLen,void* pUser);

typedef  void(CALLBACK * LPSTREAMBUFFERCALLBACK)(int nPort,void* pUser);

typedef  void(CALLBACK *LPDRAWCBCALLLBACK)(LONG  identify, LONG lRealHandle, DWORD hdc, void *pUser);

//YUV回调函数定义
/*
Param:  pCameraID:  镜头国标20位编码
pBuf: 流数据
nSize: 流数据长度
nWidth: 视频流的宽
nHeight: 视频流的高
nStamp: 时间戳
return:无
*/
typedef  void(CALLBACK* LPYUVCALLBACK)(char * pCameraID, unsigned char *pBuf, long nSize, long nWidth, long nHeight, long nStamp);

//文件结束回调
typedef  void(CALLBACK* pFileOverCB)(void * pUser);

typedef struct{
    LONG nWidth;  //画面宽，单位像素。如果是音频数据则为0；
    LONG nHeight; //画面高。如果是音频数据则为0；
    LONG nStamp;  //时标信息，单位毫秒。
    LONG nType;   //数据类型，T_AUDIO16，T_RGB32， T_YV12；
    LONG nFrameRate; //编码时产生的图像帧率。
}FRAME_INFO_D;
typedef void (CALLBACK *LPDecCBFun)(LONG nPort,char * pBuf,LONG nSize,FRAME_INFO_D * pFrameInfo, int nCameraID);

typedef struct _recordFile
{
    int nType;		    //0=按文件名播放;1=按时间播放
    char sFileName[256];
    char startTime[20];//格式：2006-11-06 10:10:00
    char endTime[20];//格式：2006-11-06 10:10:00
    int  nSize;
} RECORDFILE;

typedef struct _infoCamera
{
    DWORD cameraId ;	  //镜头ID
    char cameraName[50];  //镜头名称

}INFOCAMERA;

typedef struct _requestStreamState
{
    int nCameraId;
    char sStreamServerIP[20];//IP地址
}REQUESTSTREAMSTATE;

typedef struct _StreamServerWorkState
{
    int ChannelLoad ; //加载通道个数
    int MonitorLoad ; //客户端连接个数
    int SendLoad;	  //发送路数
    int MaxSendCount; //最大发送路数
    int MaxChannel;   //最大通道连接数
    char sStreamServerIP[20];
}STREAMSERVERWORKSTATE;


typedef int RECORDQUERYTYPE;

//record file type
#define RECORD_FILE_ALL             0   //全部录像
#define RECORD_FILE_TIME            1   //定时录像
#define RECORD_FILE_MOVE            2   //移动侦测
#define RECORD_FILE_ALARM           3   //报警录像
#define RECORD_FILE_ALARMORDYNAMIC  4   //报警或者动测
#define RECORD_FILE_ALARMANDDYNAMIC 5   //动测报警
#define RECORD_FILE_COMMAND         6   //命令触发
#define RECORD_FILE_MANUAL          7   //手动触发
#define RECORD_FILE_OUTER           8   //外部报警
#define RECORD_FILE_CARD            9   //卡号查询
#define RECORD_FILE_GROUP           10   //组合查询
#define RECORD_FILE_ALLALARM        11   //所有报警_用于大华
//目前收录了海康和大华的类型,继续封装其它的SDK时可以继续增加




//云台控制命令编码
#define CAMERA_PAN_UP        1        //云台向上
#define CAMERA_PAN_DOWN      2        //云台向下
#define CAMERA_PAN_LEFT      3        //云台向左
#define CAMERA_PAN_RIGHT     4        //云台向右
#define CAMERA_FOCUS_IN      5        //焦点前调
#define CAMERA_FOCUS_OUT     6        //焦点后调
#define CAMERA_IRIS_IN       7        //光圈调大
#define CAMERA_IRIS_OUT      8        //光圈调小
#define CAMERA_ZOOM_IN       9        //倍率调大
#define CAMERA_ZOOM_OUT      10       //倍率调小
#define CAMERA_LIGHT_CTRL    11       //灯控制
#define CAMERA_BRUSH_CTRL    12       //雨刷控制
#define CAMERA_HEATER_CTRL   13       //加热器控制
#define CAMERA_AUX_CTRL      14       //辅助设备控制
#define CAMERA_AUTO_TURN	 15		  //云台自动旋转

//云台预置命令代码
#define PTZ_PRESET_SET       1        //设置预置点
#define PTZ_PRESET_DELETE    2        //删除预置点
#define PTZ_PRESET_GOTO      3        //转到预置点

//回放控制命令
#define REPLAY_CTRL_PLAYPAUSE       1//暂停播放
#define REPLAY_CTRL_PLAYRESTART     2//恢复播放
#define REPLAY_CTRL_PLAYFAST        3//快放
#define REPLAY_CTRL_PLAYSLOW        4//慢放
#define REPLAY_CTRL_PLAYNORMAL      5//正常速度
#define REPLAY_CTRL_PLAYFRAME       6//单帧放
#define REPLAY_CTRL_PLAYSTARTAUDIO  7//打开声音
#define REPLAY_CTRL_PLAYSTOPAUDIO   8//关闭声音
#define REPLAY_CTRL_PLAYAUDIOVOLUME 9//调节音量
#define REPLAY_CTRL_PLAYSETPOS      10//改变文件回放的进度
#define REPLAY_CTRL_PLAYGETPOS      11//获取文件回放的进度
#define REPLAY_CTRL_PLAYGETTIME     12//获取当前已经播放的时间
#define REPLAY_CTRL_PLAYGETFRAME    13//获取当前已经播放的帧数
#define REPLAY_CTRL_GETTOTALFRAMES  14//获取当前播放文件总的帧数
#define REPLAY_CTRL_GETTOTALTIME    15//获取当前播放文件总的时间
#define REPLAY_CTRL_PLAYSTART       16//开始播放
#define REPLAY_CTRL_PLAYSTOP        17//停止播放
#define REPLAY_CTRL_GETFILESIZE     18//获取文件总大小
#define REPLAY_CTRL_PLAYSETPOSBY1440 19//以1440的粒度返回进度
#define REPLAY_CTRL_PLAYGETOSDTIME	 20//获取当前播放的OSD时间

//本地播放控制命令
#define LOCPLAY_CTRL_PLAYPAUSE       1//暂停播放
#define LOCPLAY_CTRL_PLAYRESTART     2//恢复播放
#define LOCPLAY_CTRL_PLAYFAST        3//快放
#define LOCPLAY_CTRL_PLAYSLOW        4//慢放
#define LOCPLAY_CTRL_PLAYNORMAL      5//正常速度
#define LOCPLAY_CTRL_PLAYFRAME       6//单帧放
#define LOCPLAY_CTRL_PLAYSTARTAUDIO  7//打开声音
#define LOCPLAY_CTRL_PLAYSTOPAUDIO   8//关闭声音
#define LOCPLAY_CTRL_PLAYAUDIOVOLUME 9//调节音量
#define LOCPLAY_CTRL_PLAYSETPOS      10//改变文件回放的进度
#define LOCPLAY_CTRL_PLAYGETPOS      11//获取文件回放的进度
#define LOCPLAY_CTRL_PLAYGETTIME     12//获取当前已经播放的时间
#define LOCPLAY_CTRL_PLAYGETFRAME    13//获取当前已经播放的帧数
#define LOCPLAY_CTRL_GETTOTALFRAMES  14//获取当前播放文件总的帧数
#define LOCPLAY_CTRL_GETTOTALTIME    15//获取当前播放文件总的时间
#define LOCPLAY_CTRL_PLAYSETFRAME    16//设置当前播放文件帧数
#define LOCPLAY_CTRL_PLAYSETPOSBY1440	17//以1440的粒度返回进度
#define LOCPLAY_CTRL_PLAYGETOSDTIME		18//获取当前播放的OSD时间

//远程配置命令
#define CONFIG_GET_DEVICECFG		100		//获取设备参数
#define CONFIG_SET_DEVICECFG		101		//设置设备参数
#define CONFIG_GET_NETCFG			102		//获取网络参数
#define CONFIG_SET_NETCFG			103		//设置网络参数
#define CONFIG_GET_PICCFG			104		//获取图象参数
#define CONFIG_SET_PICCFG			105		//设置图象参数
#define CONFIG_GET_COMPRESSCFG		106		//获取压缩参数
#define CONFIG_SET_COMPRESSCFG		107		//设置压缩参数
#define CONFIG_GET_RECORDCFG		108		//获取录像时间参数
#define CONFIG_SET_RECORDCFG		109		//设置录像时间参数
#define CONFIG_GET_DECODERCFG		110		//获取解码器参数
#define CONFIG_SET_DECODERCFG		111		//设置解码器参数
#define CONFIG_GET_RS232CFG 		112		//获取232串口参数
#define CONFIG_SET_RS232CFG			113		//设置232串口参数
#define CONFIG_GET_ALARMINCFG 		114		//获取报警输入参数
#define CONFIG_SET_ALARMINCFG		115		//设置报警输入参数
#define CONFIG_GET_ALARMOUTCFG 		116		//获取报警输出参数
#define CONFIG_SET_ALARMOUTCFG		117		//设置报警输出参数
#define CONFIG_GET_TIMECFG 			118		//获取DVR时间
#define CONFIG_SET_TIMECFG			119		//设置DVR时间
#define CONFIG_GET_PREVIEWCFG 		120		//获取预览参数
#define CONFIG_SET_PREVIEWCFG		121		//设置预览参数
#define CONFIG_GET_VIDEOOUTCFG 		122		//获取视频输出参数
#define CONFIG_SET_VIDEOOUTCFG		123		//设置视频输出参数
#define CONFIG_GET_USERCFG 			124		//获取用户参数
#define CONFIG_SET_USERCFG			125		//设置用户参数
#define CONFIG_GET_EXCEPTIONCFG 	126		//获取异常参数
#define CONFIG_SET_EXCEPTIONCFG		127		//设置异常参数
#define CONFIG_GET_ZONEANDDST		128		//获取时区和夏时制参数
#define CONFIG_SET_ZONEANDDST		129		//设置时区和夏时制参数
#define CONFIG_GET_SHOWSTRING		130		//获取叠加字符参数
#define CONFIG_SET_SHOWSTRING		131		//设置叠加字符参数
#define CONFIG_GET_EVENTCOMPCFG		132		//获取事件触发录像参数
#define CONFIG_SET_EVENTCOMPCFG		133		//设置事件触发录像参数
#define CONFIG_GET_FTPCFG			134		//获取抓图的FTP参数(基线)
#define CONFIG_SET_FTPCFG			135		//设置抓图的FTP参数(基线)


/*******************全局错误码 begin**********************/
//1、海康
#define HK_DVR_NOERROR 					1000	//没有错误
#define HK_DVR_PASSWORD_ERROR 			1001	//用户名密码错误
#define HK_DVR_NOENOUGHPRI 				1002	//权限不足
#define HK_DVR_NOINIT 					1003	//没有初始化
#define HK_DVR_CHANNEL_ERROR 			1004	//通道号错误
#define HK_DVR_OVER_MAXLINK 			    1005	//连接到DVR的客户端个数超过最大
#define HK_DVR_VERSIONNOMATCH			1006	//版本不匹配
#define HK_DVR_NETWORK_FAIL_CONNECT		1007	//连接服务器失败
#define HK_DVR_NETWORK_SEND_ERROR		1008	//向服务器发送失败
#define HK_DVR_NETWORK_RECV_ERROR		1009	//从服务器接收数据失败
#define HK_DVR_NETWORK_RECV_TIMEOUT		1010	//从服务器接收数据超时
#define HK_DVR_NETWORK_ERRORDATA		    1011	//传送的数据有误
#define HK_DVR_ORDER_ERROR				1012	//调用次序错误
#define HK_DVR_OPERNOPERMIT				1013	//无此权限
#define HK_DVR_COMMANDTIMEOUT			1014	//DVR命令执行超时
#define HK_DVR_ERRORSERIALPORT			1015	//串口号错误
#define HK_DVR_ERRORALARMPORT			1016	//报警端口错误
#define HK_DVR_PARAMETER_ERROR 			1017  //参数错误
#define HK_DVR_CHAN_EXCEPTION			1018	//服务器通道处于错误状态
#define HK_DVR_NODISK					1019	//没有硬盘
#define HK_DVR_ERRORDISKNUM				1020	//硬盘号错误
#define HK_DVR_DISK_FULL				    1021	//服务器硬盘满
#define HK_DVR_DISK_ERROR				1022	//服务器硬盘出错
#define HK_DVR_NOSUPPORT				    1023	//服务器不支持
#define HK_DVR_BUSY						1024	//服务器忙
#define HK_DVR_MODIFY_FAIL				1025	//服务器修改不成功
#define HK_DVR_PASSWORD_FORMAT_ERROR	    1026	//密码输入格式不正确
#define HK_DVR_DISK_FORMATING			1027	//硬盘正在格式化，不能启动操作
#define HK_DVR_DVRNORESOURCE			    1028	//DVR资源不足
#define	HK_DVR_DVROPRATEFAILED			1029  //DVR操作失败
#define HK_DVR_OPENHOSTSOUND_FAIL 		1030  //打开PC声音失败
#define HK_DVR_DVRVOICEOPENED 			1031  //服务器语音对讲被占用
#define	HK_DVR_TIMEINPUTERROR			1032  //时间输入不正确
#define	HK_DVR_NOSPECFILE				1033  //回放时服务器没有指定的文件
#define HK_DVR_CREATEFILE_ERROR			1034	//创建文件出错
#define	HK_DVR_FILEOPENFAIL				1035  //打开文件出错
#define	HK_DVR_OPERNOTFINISH			    1036  //上次的操作还没有完成
#define	HK_DVR_GETPLAYTIMEFAIL			1037  //获取当前播放的时间出错
#define	HK_DVR_PLAYFAIL					1038  //播放出错
#define HK_DVR_FILEFORMAT_ERROR			1039  //文件格式不正确
#define HK_DVR_DIR_ERROR				    1040	//路径错误
#define HK_DVR_ALLOC_RESOURCE_ERROR		1041  //资源分配错误
#define HK_DVR_AUDIO_MODE_ERROR			1042	//声卡模式错误
#define HK_DVR_NOENOUGH_BUF				1043	//缓冲区太小
#define HK_DVR_CREATESOCKET_ERROR		1044	//创建SOCKET出错
#define HK_DVR_SETSOCKET_ERROR			1045	//设置SOCKET出错
#define HK_DVR_MAX_NUM					1046	//个数达到最大
#define HK_DVR_USERNOTEXIST				1047	//用户不存在
#define HK_DVR_WRITEFLASHERROR			1048  //写FLASH出错
#define HK_DVR_UPGRADEFAIL				1049  //DVR升级失败
#define HK_DVR_CARDHAVEINIT				1050  //解码卡已经初始化过
#define HK_DVR_PLAYERFAILED				1051	//调用播放库中某个函数失败
#define HK_DVR_MAX_USERNUM				1052  //设备端用户数达到最大
#define HK_DVR_GETLOCALIPANDMACFAIL		1053  //获得客户端的IP地址或物理地址失败
#define HK_DVR_NOENCODEING				1054	//该通道没有编码
#define HK_DVR_IPMISMATCH				1055	//IP地址不匹配
#define HK_DVR_MACMISMATCH				1056	//MAC地址不匹配
#define HK_DVR_UPGRADELANGMISMATCH		1057	//升级文件语言不匹配
#define HK_DVR_MAX_PLAYERPORT			1058	//播放器路数达到最大
#define HK_DVR_NOSPACEBACKUP			    1059	//备份设备中没有足够空间进行备份
#define HK_DVR_NODEVICEBACKUP			1060	//没有找到指定的备份设备
#define HK_DVR_PICTURE_BITS_ERROR		1061	//图像素位数不符，限24色
#define HK_DVR_PICTURE_DIMENSION_ERROR	1062	//图片高*宽超限， 限128*256
#define HK_DVR_PICTURE_SIZ_ERROR		    1063	//图片大小超限，限100K
#define HK_DVR_LOADPLAYERSDKFAILED		1064	//载入当前目录下Player Sdk出错
#define HK_DVR_LOADPLAYERSDKPROC_ERROR	1065	//找不到Player Sdk中某个函数入口
#define HK_DVR_LOADDSSDKFAILED			1066	//载入当前目录下DSsdk出错
#define HK_DVR_LOADDSSDKPROC_ERROR		1067	//找不到DsSdk中某个函数入口
#define HK_DVR_DSSDK_ERROR				1068	//调用硬解码库DsSdk中某个函数失败
#define HK_DVR_VOICEMONOPOLIZE			1069	//声卡被独占
#define HK_DVR_JOINMULTICASTFAILED		1070	//加入多播组失败
#define HK_DVR_CREATEDIR_ERROR			1071	//建立日志文件目录失败
#define HK_DVR_BINDSOCKET_ERROR			1072	//绑定套接字失败
#define HK_DVR_SOCKETCLOSE_ERROR		    1073	//socket连接中断，此错误通常是由于连接中断或目的地不可达
#define HK_DVR_USERID_ISUSING			1074	//注销时用户ID正在进行某操作
#define HK_DVR_SOCKETLISTEN_ERROR		1075	//监听失败
#define HK_DVR_PROGRAM_EXCEPTION		    1076	//程序异常
#define HK_DVR_WRITEFILE_FAILED			1077	//写文件失败
#define HK_DVR_FORMAT_READONLY			1078  //禁止格式化只读硬盘
#define HK_DVR_WITHSAMEUSERNAME		    1079  //用户配置结构中存在相同的用户名
#define HK_DVR_DEVICETYPE_ERROR          1080  /*导入参数时设备型号不匹配*/
#define HK_DVR_LANGUAGE_ERROR            1081  /*导入参数时语言不匹配*/
#define HK_DVR_PARAVERSION_ERROR         1082  /*导入参数时软件版本不匹配*/
#define HK_DVR_IPCHAN_NOTALIVE           1083  /*预览时外接IP通道不在线*/
#define HK_DVR_RTSP_SDK_ERROR			1084	/*加载高清IPC通讯库StreamTransClient失败*/
#define HK_DVR_CONVERT_SDK_ERROR		    1085	/*加载转码库CVT_StdToHik失败*/
#define HK_DVR_IPC_COUNT_OVERFLOW		1086  /*超出最大的ip接入通道数*/

#define  HK_PLAYM4_NOERROR					    1500	//no error
#define	 HK_PLAYM4_PARA_OVER				        1501	//input parameter is invalid;
#define  HK_PLAYM4_ORDER_ERROR				    1502	//The order of the function to be called is error.
#define	 HK_PLAYM4_TIMER_ERROR				    1503	//Create multimedia clock failed;
#define  HK_PLAYM4_DEC_VIDEO_ERROR			    1504	//Decode video data failed.
#define  HK_PLAYM4_DEC_AUDIO_ERROR			    1505	//Decode audio data failed.
#define	 HK_PLAYM4_ALLOC_MEMORY_ERROR		    1506	//Allocate memory failed.
#define  HK_PLAYM4_OPEN_FILE_ERROR			    1507	//Open the file failed.
#define  HK_PLAYM4_CREATE_OBJ_ERROR		        1508	//Create thread or event failed
#define  HK_PLAYM4_CREATE_DDRAW_ERROR		    1509	//Create DirectDraw object failed.
#define  HK_PLAYM4_CREATE_OFFSCREEN_ERROR        1510	//failed when creating off-screen surface.
#define  HK_PLAYM4_BUF_OVER			            1511	//buffer is overflow
#define  HK_PLAYM4_CREATE_SOUND_ERROR	        1512	//failed when creating audio device.
#define	 HK_PLAYM4_SET_VOLUME_ERROR	            1513	//Set volume failed
#define  HK_PLAYM4_SUPPORT_FILE_ONLY	            1514	//The function only support play file.
#define  HK_PLAYM4_SUPPORT_STREAM_ONLY	        1515	//The function only support play stream.
#define  HK_PLAYM4_SYS_NOT_SUPPORT		        1516	//System not support.
#define  HK_PLAYM4_FILEHEADER_UNKNOWN            1517	//No file header.
#define  HK_PLAYM4_VERSION_INCORRECT	            1518	//The version of decoder and encoder is not adapted.
#define  HK_PALYM4_INIT_DECODER_ERROR            1519	//Initialize decoder failed.
#define  HK_PLAYM4_CHECK_FILE_ERROR	            1520	//The file data is unknown.
#define  HK_PLAYM4_INIT_TIMER_ERROR	            1521	//Initialize multimedia clock failed.
#define	 HK_PLAYM4_BLT_ERROR			            1522	//Blt failed.
#define  HK_PLAYM4_UPDATE_ERROR		            1523	//Update failed.
#define  HK_PLAYM4_OPEN_FILE_ERROR_MULTI         1524   //openfile error, streamtype is multi
#define  HK_PLAYM4_OPEN_FILE_ERROR_VIDEO         1525   //openfile error, streamtype is video
#define  HK_PLAYM4_JPEG_COMPRESS_ERROR           1526   //JPEG compress error
#define  HK_PLAYM4_EXTRACT_NOT_SUPPORT           1527	//Don't support the version of this file.
#define  HK_PLAYM4_EXTRACT_DATA_ERROR            1528	//extract video data failed.

//2、大华

/*******************全局错误码 end**********************/


typedef struct _alarmInfo
{
    unsigned long ip;
    int  alarmType;
    int  alarmFlag;
} ALARMINFO;

typedef struct _logInfo
{
    char sTime[20];//格式：2006-11-06 10:10:00
    char log[1024];
} LOGINFO;

typedef int LOGQUERYTYPE;
typedef int LOGERRORTYPE;

//log file type
#define LOG_FILE_ALL             0   //全部日志
#define LOG_FILE_TIME            1   //按时间查询
#define LOG_FILE_TYPE            2   //按类型查询
#define LOG_FILE_TIMETYPE        3   //按时间类型查询

//按类型查询时,错误类型定义:
#define LOG_FILE_TYPE_ALARM      0   //报警
#define LOG_FILE_TYPE_EXCEPTION  1   //异常
#define LOG_FILE_TYPE_OPERATION  2   //操作
#define LOG_FILE_TYPE_OTHER      3   //其它类型
//目前收录了海康和大华的类型,继续封装其它的SDK时可以继续增加


typedef  void(CALLBACK * LPALARMCALLBACK)(ALARMINFO* pAlarmInfo);

/*! 透明通道回调函数 */
typedef void(CALLBACK* LPSERIALDATACALLBACK)(char* pRecDataBuffer,DWORD dwBufSize,int nCameraID);


//查询设备支持属性(queryPorperySupport函数使用)
typedef int XYPROPROPERTY;
#define PROPERTY_ISSUPPORT_PACK 0//查询是需要发送原始包(即不可分包) ,返回true:是  false:否
#define PROPERTY_ISHAVEHEAD     1//查询是有包头 true:是  false:否



 #define REAL_STREAM  1 //实时流
 #define DVRFILE_STREAM  2 //DVR文件流
 #define STOREFILE_STREAM 3 //录像文件流
//视频文件播放控制
#define PLAY_NOMAL                   1//正常播放
#define PLAY_FAST                    2//快进
#define PLAY_SLOW                    3//慢放
#define PLAY_ONEBYONE                4//单帧播放
//
typedef struct __RecordFileInfo
{
    char fileName[128];
    char startTime[64];
    char endTime[64];
}RecordFileInfo;

//2011-11-23, 接口参数列表
//连接DVR
typedef struct CONNECT_PARAM
{
    DWORD size;//可根据此参数（结构体大小来兼容版本差异）
    const char* sIP;
    int nPort;
    const char* sType; //设备类型
    const char* sUser;
    const char* sPwd;
    int nChannel;
    int nModuleType;//模块类型(流媒体,控制...)
    char chReser[32]; //可根据此参数(快速扩展)
    //CONNECT_PARAM()
    //{
    //	ZeroMemory(this, sizeof CONNECT_PARAM);
    //}
}*PCONNECT_PARAM;

//*virtual  bool */captureStream(CAPTURE_PARAM *pParam) = 0;
typedef struct CAPTURE_PARAM
{
    DWORD size;
    LPCAPTURESTREAMCALLBACK lpCallBack;
    void* pUser;
    int nGetVideoType;
    char chReser[32];
    //CAPTURE_PARAM()
    //{
    //	ZeroMemory(this, sizeof(CAPTURE_PARAM));
    //}
}*PCAPTURE_PARAM;
//virtual  bool controlPTZSpeedWithChannel(CONTROL_PARAM * pParam) = 0;
typedef struct CONTROL_PARAM
{
    DWORD size;
    int nCmd;
    bool bEnable;
    int nSpeed;
    int nChannel;
    char chReser[32];
    //CONTROL_PARAM()
    //{
    //	ZeroMemory(this, sizeof(CONTROL_PARAM));
    //}
}*PCONTROL_PARAM;

//硬解插件（硬解库）需求库（以HK为例需要海康通讯库和硬解库）
//示例：
//bool initialPlayer(INITPY_PARAM *pIParam, CONNECT_PARAM *pCParam ); //硬解单独使用或软解复用此接口
typedef struct INITPY_PARAM
{
    DWORD size;
    int nPort;
    int nDecodeType;
    int nSpeed;
    BYTE* buff;
    int nSize;
    int nType;
    char chReser[32];

    //INITPY_PARAM()
    //{
    //	ZeroMemory(this, sizeof(INITPY_PARAM));
    //}
}*PINITPY_PARAM;

//由于多路解码器的解码实际是远程解码，非本地，还要应用到CONNECT_PARAM参数，还要使用本地硬解接口。
//客户端使用（软解库）需求库（以HK为例需要海康软解库）
//示例：
//bool startPlayer4Standard(STARTPYSTD_PARAM *pParam); //与硬解并用

typedef struct STARTPYSTD_PARAM
{
    DWORD size;
    HWND hWnd;
    LPDecCBFun lpDecCBFun;
    int nCameraID;
    char chReser[32];
    //STARTPYSTD_PARAM()
    //{
    //	ZeroMemory(this, sizeof(STARTPYSTD_PARAM));
    //}
}* PSTARTPYS_PARAM;

//bool locPlay_OpenFile(LOCPY_OPENF_PARAM *pParam); //与软解并用
typedef struct LOCPY_OPENF_PARAM
{
    DWORD size;
    int nPort;
    int nDecodeType;
    int hWnd;
    char* fileName;
    char chReser[32];
    //LOCPY_OPENF_PARAM()
    //{
    //	ZeroMemory(this, sizeof(LOCPY_OPENF_PARAM));
    //}

}* PLOCPY_OPENF_PARAM_PARAM;

#define MODULE_CONNECT   0
#define MODULE_STREAM    1
#define MODULE_CONTROL   2
#define MODULE_PLAYER	 10

namespace MODULENAME
{
    const char* const MODULE_NAME[20] = {"Connect","Stream","Control","","","","","","","","Player"};
}

#define PLAY_ONEBYONEBACK            5//单帧回放
#define PLAY_BACK                    6//后退

#define RENDER_BY_OPENGL	0x01		//使用OpenGL渲染图像
#define RENDER_BY_D3D		0x02		//使用D3D渲染图像

/*
MediaInf结构说明：
此结构用于本地文件播放时，用来获取文件的媒体信息
XY_OpenFile后，传入XY_GetMediaInfo()即可获取相关信息
*/
typedef struct _MediaInf{
	char fileformat[32]; //文件格式
	char codeType[32];   //编码格式
	char colorType[32];  //色彩空间 比如：yuv420p
	int  bit_rate;		 //码率
	int  frame_rate;     //帧率
	int  v_width;        //图像宽度
	int  v_height;		 //图像高度
}MediaInf; 
#endif//_NVSSDKDEF_H
