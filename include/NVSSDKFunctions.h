#pragma once
//V1.5 版本    (与V1.4版本一样头文件)
#include "NVSSDKDef.h"

//#ifdef NVSSDKENGINE_DLL
//#define NVSSDKENGINE_API extern "C" _declspec(dllexport)
//#else
//#define NVSSDKENGINE_API extern "C" _declspec(dllimport)
//#endif
#ifdef _WIN32
#define NVSSDKENGINE_API   extern "C" _declspec(dllexport)
#else
#define NVSSDKENGINE_API extern "C"
#endif


/* \file NVSSDKFunctions.h
\brief 接口定义

统一SDK各个接口定义
*/
#ifndef byte
typedef unsigned char byte;
#endif

// 连接DVR.
/*
\param sIP 地址.
\param nPort 端口.
\param sUser 用户名.
\param sPwd 密码.
\param nChannel 通道.
\param sType 解析库.
\return 连接句柄
        \-1:加载PlugIn.dll失败
        \-2:获取DVRHandler对象失败
        \-3:登录DVR失败
*/
NVSSDKENGINE_API LONG connectDVR(char* sIP, int nPort, char* sUser, char* sPwd, int nChannel,char* sType , BOOL &BRes);

NVSSDKENGINE_API LONG connectDVREx(PCONNECT_PARAM pCParam , BOOL &BRes);

// 取DLL标识.
/*
\param sType 解析库.
\return DLL连接句柄
*/
NVSSDKENGINE_API LONG getIdentify(char* sType);

// 释放连接.
/*
\param identify 连接时的句柄.
\return 无
*/
NVSSDKENGINE_API void disconnectDVR(LONG identify);

// 去除登录信息.
/*
\param identify 连接时的句柄.
\return 无
*/
NVSSDKENGINE_API void freeLoginInfo(LONG identify);

// 检测DVR.
/*
\param sIP 地址.
\param nPort 端口.
\param sUser 用户名.
\param sPwd 密码.
\param sType 解析库.
\param pInfo 返回信息缓冲.
\param nMaxCount 缓冲区最大个数.
\return 检测结果
*/
NVSSDKENGINE_API LONG checkDVR(char* sIP, int nPort, char* sUser, char* sPwd,char* sType,CLInfo pInfo[],int nMaxCount);

NVSSDKENGINE_API LONG checkDVREx(PCONNECT_PARAM pCParam,CLInfo pInfo[],int nMaxCount);
// 设置信息.
/*
\param sIP 地址.
\param nPort 端口.
\param sUser 用户名.
\param sPwd 密码.
\param sType 解析库.
\param pInfo 信息缓冲.
\param nMaxCount 缓冲区最大个数.
\return 无
*/
NVSSDKENGINE_API void setDVR(char* sIP, int nPort, char* sUser, char* sPwd,char* sType,CLInfo pInfo[],int nInfoCount);

NVSSDKENGINE_API void setDVREx(PCONNECT_PARAM pCParam,CLInfo pInfo[],int nInfoCount);
// 截取视频流.
/*
\param identify 连接时的句柄.
\param lpCallBack 回调函数指针.
\param pUser 承载对象指针.
\return 截取结果
*/
NVSSDKENGINE_API bool captureStream(LONG identify, LPCAPTURESTREAMCALLBACK lpCallBack, void* pUser, int nGetVideoType = 0);

NVSSDKENGINE_API bool captureStreamEx(LONG identify , PCAPTURE_PARAM pParam);
// 结束截取.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool stopCaptureStream(LONG identify);
// 云台控制.
/*
\param identify 连接时的句柄.
\param nCmd 控制命令.
\param bEnable 开关.
\return 操作结果
*/
NVSSDKENGINE_API bool controlPTZ(LONG identify,int nCmd, bool bEnable);
// 云台速度控制.
/*
\param identify 连接时的句柄.
\param nCmd 控制命令.
\param bEnable 开关.
\param nSpeed 速度
\return 操作结果
*/
NVSSDKENGINE_API bool controlPTZSpeed(LONG identify,int nCmd, bool bEnable,int nSpeed);
// 预置位设置.
/*
\param identify 连接时的句柄.
\param nCmd 控制命令.
\param bEnable 开关.
\return 操作结果
*/
NVSSDKENGINE_API bool presetPTZ(LONG identify, int nCmd, int nIndex);
// 抓图.
/*
\param hWnd 承载窗口句柄.
\param nWidth 宽.
\param nHeight 高.
\param sFilePath 文件路径.
\return 抓图结果
*/
NVSSDKENGINE_API bool capturePicture(HWND hWnd,int nWidth,int nHeight,char* sFilePath);
// 获取录像文件列表.
/*
\param identify 连接时的句柄.
\param nType 记录类型.
\param startTime 开启时间.
\param endTime 关闭时间.
\param pRecordFile 缓冲区地址.
\param nMaxFileCount 缓冲区大小.
\return 记录文件个数   0:无记录  -1：不支持该查找类型 -2:失败  -3:还未登录DVR
*/
NVSSDKENGINE_API int getRecordFileEx(LONG identify,RECORDQUERYTYPE nType,char* startTime,char* endTime,RECORDFILE pRecordFile[],int nMaxFileCount);
// 获取录像文件列表.
/*
\param identify 连接时的句柄.
\param nType 记录类型.
\param startTime 开启时间.
\param endTime 关闭时间.
\param nFileCount 记录文件个数.
\return 内容缓冲
*/
NVSSDKENGINE_API RECORDFILE* getRecordFile(LONG identify,RECORDQUERYTYPE nType,char* startTime,char* endTime,int* nFileCount);
// 下载录像文件.
/*
\param identify 连接时的句柄.
\param fileName 文件名.  ==文件名不为空，按文件名下载，文件名为空时按时间下载。==
\param startTime 开启时间.
\param endTime 关闭时间.
\param nFileSize 文件大小.
\param saveFile 存储文件路径.
\param lpCallBack 回调函数指针. ==此回调函数是为通知客户端下载进度的时候有用，
\                                 现在不用这种方式，使用的是客户端主动获取的方式，
\                                 所以这个回调函数指针设为空即可。==
\param pUser 承载对象指针.
\return 返回的下载句柄
        \-1:获取下载文件句柄失败
        \-2:还未登录DVR
*/
NVSSDKENGINE_API LONG downloadRecordFile(LONG identify,char* fileName,char* startTime, char* endTime,int nFileSize,char* saveFile,LPDOWNLOADRECORDFILECALLBACK lpCallBack,void* pUser);

NVSSDKENGINE_API LONG downloadRecordFileEx(LONG identify ,RECORDFILE RecordFile,char* saveFile,LPDOWNLOADRECORDFILECALLBACK lpCallBack,void* pUser);

// 停止下载录像文件.
/*
\param identify 连接时的句柄.
\param ldownFileHandle 下载文件句柄
\return 操作结果
*/
NVSSDKENGINE_API bool stopDownloadRecordFile(LONG identify,LONG ldownFileHandle);
// 远程回放.
/*
\param identify 连接时的句柄.
\param hWnd 窗口句柄.
\param fileName 文件名称. ==文件名不为空时按文件名播放，否则按时间播放==
\param startTime 开始时间.
\param endTime 结束时间.
\param nFileSize 文件大小.
\return 回放句柄
*/
NVSSDKENGINE_API LONG replayRecordFile(LONG identify,int hWnd,char* fileName,char* startTime, char* endTime,int nFileSize);

NVSSDKENGINE_API LONG replayRecordFileEx(LONG identify, int hWnd,RECORDFILE RecordFile);
// 远程回放控制.
/*
\param identify 连接时的句柄.
\param lReplayHandle 回播句柄.
\param nCmd 命令.
\param nInValue 输入参数.
\param outValue 输出参数.
\return 操作结果
*/
NVSSDKENGINE_API bool controlReplayRecordFile(LONG identify,LONG lReplayHandle,int nCmd,int nInValue,int* outValue);
// 停止远程回放.
/*
\param identify 连接时的句柄.
\param lReplayHandle 回播句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool stopReplayRecordFile(LONG identify,LONG lReplayHandle);
// 开始语音对讲.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool startTalk(LONG identify);
// 停止语音对讲.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool stopTalk(LONG identify);
// 启动PC端声音捕获.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool clientAudioStart(LONG identify);
// 停止PC端声音捕获.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool clientAudioStop(LONG identify);
// 添加一台DVR到可以接收PC机声音的组里.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool addDVR(LONG identify);
// 删除已经加到组的DVR.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool delDVR(LONG identify);
// 设置语音对讲PC端的音量.
/*
\param identify 连接时的句柄.
\param wVolume 音量.
\return 操作结果
*/
NVSSDKENGINE_API bool setVoiceComClientVolume(LONG identify,WORD wVolume);
// 显示远程设置界面.
/*
\param identify 连接时的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool showSettingPane(LONG identify);
// 初始化播放客户端.
/*
\param sType 解析库.
\param nPort 端口索引.
\param nDecodeType 解码类型.
\param buff 播放缓冲区.
\param nSize 缓冲区大小.
\param nType 流类型 0=实时流 1=文件流
\return 播放句柄
*/
NVSSDKENGINE_API LONG initialPlayer(char* sType,int nPort,int nDecodeType,BYTE* buff, int nSize, int nStreamType);

NVSSDKENGINE_API LONG initialPlayerEx(char* sType ,PINITPY_PARAM pIParam);
// 开始播放.
/*
\param identify 初始化返回的句柄.
\param hWnd 播放窗口句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool startPlayer(LONG identify,HWND hWnd);
/* 开始播放2
\param identify 初始化返回的句柄
\param hWnd 播放窗口句柄.
\
\return 操作结果
*/
NVSSDKENGINE_API bool startPlayer2(LONG identify,HWND hWnd,  LPDRAWCBCALLLBACK lpDrawCBFun, void *pUser);

//2007-04-23
// 开始播放并且获取标准格式视频流.
/*
\param identify 初始化返回的句柄.
\param hWnd 播放窗口句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool startPlayer4Standard(LONG identify,HWND hWnd,LPDecCBFun lpDecCBFun, int nCameraID);

NVSSDKENGINE_API bool startPlayer4StandardEx(LONG identify, PSTARTPYS_PARAM pParam);

// 填入流数据.
/*
\param identify 初始化返回的句柄.
\param buff 缓冲区.
\param nSize 缓冲区大小.
\return 操作结果
*/
NVSSDKENGINE_API bool addData(LONG identify,BYTE* buff, int nSize);
// 关闭播放.
/*
\param identify 初始化返回的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool stopPlayer(LONG identify);
// 本地文件回放.
/*
\param sType 解析库.
\param nPort 端口索引.
\param nDecodeType 解码类型.
\param hWnd 播放句柄.
\param fileName 文件明.
\return 操作结果
*/
NVSSDKENGINE_API LONG locPlay_OpenFile(char* sType,int nPort,int nDecodeType,int hWnd,char* fileName);

NVSSDKENGINE_API LONG locPlay_OpenFileEx(char* sType , PLOCPY_OPENF_PARAM_PARAM pParam);
// 本地文件控制.
/*
\param identify 初始化返回的句柄.
\param nCmd 命令.
\param nInValue 输入参数.
\param outValue 输出参数.
\return 操作结果
*/
NVSSDKENGINE_API bool locPlay_Control(LONG identify,int nCmd,int nInValue,int* outValue);
// 本地文件停止播放.
/*
\param identify 初始化返回的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool locPlay_Stop(LONG identify);
// 报警接口.
/*
\param sType 解析库.
\param nPort 端口索引.
\param nDecodeType 解码类型.
\param hWnd 播放句柄.
\param fileName 文件明.
\return 操作结果
*/
NVSSDKENGINE_API LONG alarm_startCapture(char* sType,int nPort,LPALARMCALLBACK lpCallBack);
// 停止报警.
/*
\param identify 初始化返回的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool alarm_stop(LONG identify);
// 查询日志接口.
/*
\param identify 初始化返回的句柄.
\param nQueryType 查询类型.
\param nErrorType 错误类型.
\param startTime 开始时间.
\param endTime 结束时间.
\param pLogInfo 日志缓冲.
\param nMaxLogCount 缓冲最大值.
\return 日志个数
*/
NVSSDKENGINE_API int log_QueryEx(LONG identify,LOGQUERYTYPE nQueryType, LOGERRORTYPE nErrorType,char* startTime,char* endTime,LOGINFO pLogInfo[],int nMaxLogCount);
// 查询日志接口.
/*
\param identify 初始化返回的句柄.
\param nQueryType 查询类型.
\param nErrorType 错误类型.
\param startTime 开始时间.
\param endTime 结束时间.
\param nLogCount 日志缓冲.
\return 日志缓冲
*/
NVSSDKENGINE_API LOGINFO* log_Query(LONG identify,LOGQUERYTYPE nQueryType, LOGERRORTYPE nErrorType,char* startTime,char* endTime,int* nLogCount);
// 调整视频参数.
/*
\param identify 初始化返回的句柄.
\param nBrightValue 亮度(取值为1-10).
\param nContrastValue 对比度(取值为1-10).
\param nSaturationValue 饱和度(取值为1-10).
\param nHueValue 色度(取值为1-10).
\return 操作结果
*/
NVSSDKENGINE_API bool setVideoEffect(LONG identify,DWORD dwBrightValue,DWORD dwContrastValue, DWORD dwSaturationValue,DWORD dwHueValue);
// 获取视频参数.
/*
\param identify 初始化返回的句柄.
\param nBrightValue 亮度(取值为1-10).
\param nContrastValue 对比度(取值为1-10).
\param nSaturationValue 饱和度(取值为1-10).
\param nHueValue 色度(取值为1-10).
\return 操作结果
*/
NVSSDKENGINE_API bool getVideoEffect(LONG identify,DWORD *pBrightValue,DWORD *pContrastValue, DWORD *pSaturationValue,DWORD *pHueValue);
// 设置设备结构.
/*
\param identify 初始化返回的句柄.
\param dwCommand 命令
\param lChannel 通道.
\param lpInBuffer 是一个传入的字符串指针（byStreamType@byResolution@byBitrateType@byPicQuality@dwVideoBitrate@dwVideoFrameRate$byStreamType@byResolution@byBitrateType@byPicQuality@dwVideoBitrate@dwVideoFrameRate）用$符号区分录像结构信息和网传信息结构,用@区分信息体内各个参数 的一段是录像结构信息  第二段是网络结构信息
\param dwInBufferSize 缓冲区大小.
\return 设置结果
*/
NVSSDKENGINE_API bool setDVRConfig(LONG identify, DWORD dwCommand,LONG lChannel,LPVOID lpInBuffer,DWORD dwInBufferSize);
// 获取设备结构.
/*
\param identify 初始化返回的句柄.
\param dwCommand 命令
\param lChannel 通道.
\param lpOutBuffer 是一个传出的字符串指针（byStreamType@byResolution@byBitrateType@byPicQuality@dwVideoBitrate@dwVideoFrameRate$byStreamType@byResolution@byBitrateType@byPicQuality@dwVideoBitrate@dwVideoFrameRate）用$符号区分录像结构信息和网传信息结构,用@区分信息体内各个参数 的一段是录像结构信息  第二段是网络结构信息
\param dwOutBufferSize 缓冲区大小.
\param lpBytesReturned 返回状态.
\return 获取结果
*/
NVSSDKENGINE_API bool getDVRConfig(LONG identify, DWORD dwCommand,LONG lChannel,LPVOID lpOutBuffer,DWORD dwOutBufferSize,LPDWORD lpBytesReturned);
// 将抓图得到的图像数据保存成BMP文件.
/*
\param sType 解析库.
\param pBuf 缓冲区地址.
\param nSize 缓冲大小.
\param nWidth 宽.
\param nHeight 高.
\param nType 解析库.
\param sFileName 文件名称.
\return 操作结果
*/
NVSSDKENGINE_API int  convertToBmpFile(char* sType,char * pBuf,LONG nSize,LONG nWidth,LONG nHeight,LONG nType,char *sFileName);
// 本地播放向后一帧.
/*
\param sType 解析库.
\param nPort 端口.
\return 操作结果
*/
NVSSDKENGINE_API int  oneByOneBack(char* sType,LONG nPort);
// 本地播放向前一帧.
/*
\param sType 解析库.
\param nPort 端口.
\return 操作结果
*/
NVSSDKENGINE_API int  oneByOne(char* sType,LONG nPort);
// 设置本地回放播放的时间进度.
/*
\param sType 解析库.
\param nPort 端口.
\param nTime 时刻.
\return 操作结果
*/
NVSSDKENGINE_API int  setPlayedTimeEx(char* sType,LONG nPort,DWORD nTime);
// 本地播放前设置回调函数.
/*
\param sType 解析库.
\param nPort 端口.
\param pFileRefDone 回调指针.
\param nUser 承载者
\return 操作结果
*/
NVSSDKENGINE_API int  setFileRefCallBack(char* sType,LONG nPort,void (__stdcall *pFileRefDone) (DWORD nPort,DWORD nUser),DWORD nUser);
// 本地解帧回调函数.
/*
\param sType 解析库.
\param nPort 端口.
\param pFileRefDone 回调指针.
\return 操作结果
*/
NVSSDKENGINE_API int  setDisplayCallBack(char* sType,LONG nPort, void (__stdcall *pFileRefDone)(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved));
// 保存回放的数据.
/*
\param identify 初始化返回的句柄.
\param lReplayHandle 回播句柄
\param sFileName 文件名
\return 获取结果
*/
NVSSDKENGINE_API bool playBackSaveData(LONG identify,LONG lReplayHandle,char *sFileName);
// 停止保存回放的数据.
/*
\param identify 初始化返回的句柄.
\param lReplayHandle 回播句柄
\return 获取结果
*/
NVSSDKENGINE_API bool stopPlayBackSave(LONG identify,LONG lReplayHandle);
// 获取下载的进度.
/*
\param identify 初始化返回的句柄.
\param lFileHandle 文件句柄.

\return ：-1表示失败，0-100:下载的进度，100表示下载结束，>100: 由于网络原因或DVR忙,下载异常终止
\         -2:下载文件句柄无效
*/
NVSSDKENGINE_API int getDownloadPos(LONG identify,LONG lFileHandle);
// 获取下载的进度.
/*
\param identify 初始化返回的句柄.
\param lFileHandle 文件句柄.
\param nFileSize : out 文件总大小(B)
\param nDownSize : out 当前下载文件大小(B)
\param nSpeed    : out 平均下载速度 (B/S)
\param nUsedTime : out 用时(S)
\param nRemainTime: out 剩余时间(S)
\return ：-1表示失败，0-100:下载的进度，100表示下载结束，>100: 由于网络原因或DVR忙,下载异常终止
*/
NVSSDKENGINE_API int getDownloadPosEx(LONG identify,LONG lFileHandle, int *nFileSize, int *nDownSize, int *nSpeed, int *nUsedTime, int *nRemainTime);
// 获取硬盘录像机工作状态.
/*
\param identify 初始化返回的句柄.
\param dvrWorkState 返回得工作状态字符串如下例
                    HK$0$1024000!1024000!0^1024000!1024000!0$0!0!0!50!2!2250004@2250008^0!0!0!50!2!2250004@2250008$0@0@0@0@0@0@0$0@0@0@0@0@0@0$0
                    用$来区分每段表示的意思
                    HK									海康
                    0										设备的状态,0-正常,1-CPU占用率太高,超过85%,2-硬件错误
                    1024000!1024000!0^1024000!1024000!0	表示有两个硬盘,每个硬盘参数(硬盘的容量 硬盘的剩余空间 硬盘的状态,按位 1-休眠，2-不正常)
                    0!0!0!50!2!2250004@2250008^0!0!0!50!2!2250004@2250008
                                                            表示有两个通道,每个通道信息如下
                                                                通道是否在录像,0-不录像,1-录像
                                                                连接的信号状态,0-正常,1-信号丢失
                                                                通道硬件状态,0-正常,1-异常,例如DSP死掉
                                                                实际码率
                                                                客户端连接的个数(这里举例是2个)
                                                                客户端的IP地址(这里有两个客户端,用 @ 把两个IP地址区分)
                    0@0@0@0@0@0@0							报警端口的状态,0-没有报警,1-有报警,用 @ 把每个端口区分
                    0@0@0@0@0@0@0							报警输出端口的状态,0-没有输出,1-有报警输出,用 @ 把每个端口区分
                    0										本地显示状态,0-正常,1-不正常
\param arrayLen 缓冲区长度.
\return 获取结果
*/
NVSSDKENGINE_API bool getDVRWorkState(LONG identify,char dvrWorkState[],int arrayLen);
// 通过解析服务器,获取硬盘录像机的动态IP地址.
/*
\param identify 初始化返回的句柄.
\param sServerIP 服务地址.
\param wServerPort 端口.
\param sDVRName DVR名称.
\param wDVRNameLen DVR名称长度.
\param sDVRSerialNumber 序列号.
\param wDVRNameLen 序列号长度.
\param sGetIP 获取连接IP.
\return 获取结果
*/
NVSSDKENGINE_API bool getDVRIPByResolveSvr(LONG identify,char *sServerIP,int wServerPort,char *sDVRName,int wDVRNameLen,char *sDVRSerialNumber,int wDVRSerialLen,char* sGetIP);
// 设置连接超时时间和连接尝试次数.
/*
\param identify 初始化返回的句柄.
\param dwWaitTime 等待时间.
\param dwTryTimes 尝试次数.
\return 设置结果
*/
NVSSDKENGINE_API bool setConnectTime(LONG identify,DWORD dwWaitTime,DWORD dwTryTimes);
// 启动监听程序，监听硬盘录像机发起的请求,接收硬盘录像机的信息.
/*
\param identify 初始化返回的句柄.
\param sLocalIP 本地地址.
\param wLocalPort 本地端口.
\return 操作结果
*/
NVSSDKENGINE_API bool startListen (LONG identify,char *sLocalIP,int wLocalPort);
// 停止监听程序.
/*
\param identify 初始化返回的句柄.
\return 操作结果
*/
NVSSDKENGINE_API bool stopListen(LONG identify);
// 通明通道控制指令.
/*
\param identify 初始化返回的句柄.
\param pBuf 指令1缓冲.
\param nLength 指令1长度.
\param pBuf2 指令2缓冲.
\param nLen2 指令2长度.
\return 发送结果
*/
NVSSDKENGINE_API bool serialSend(LONG identify,char* pBuf,DWORD nLength,char* pBuf2,DWORD nLen2);

// 通明通道发送控制指令_带回调函数
/*
\param identify 初始化返回的句柄.
\param pBuf 指令缓冲.
\param nLength 指令长度.
\param nCameraID 由调用者传值，在回调里传出，为区别回调中数据
\return 发送结果
*/
NVSSDKENGINE_API bool serialSendWithCallBack(LONG identify,char* pBuf,DWORD nLength,int nCameraID);
// 建立通明通道与关闭
/*
\param identify 初始化返回的句柄.
\param lSerialPort 串口号 1-232串口 2-485串口
\param pSerialCallBack 回调函数指针
\typedef void(CALLBACK* LPSERIALDATACALLBACK)(char* pRecDataBuffer,DWORD dwBufSize)
\回调函数说明：
\pRecvDataBuffer：存放接收到数据的缓冲区指针
\dwBufSize：缓冲区的大小
\param bOpen true 为开，false 为关
\return 发送结果
*/
NVSSDKENGINE_API bool buildSerialSend(LONG identify,LONG lSerialPort,LPSERIALDATACALLBACK pSerialCallBack,bool bOpen);
//2007-05-23
// 获取通道解码器地址.
/*
\param identify 初始化返回的句柄.
\param pBuf 指令1缓冲.
\param nLength 指令1长度.
\param pBuf2 指令2缓冲.
\param nLen2 指令2长度.
\return 发送结果
*/
NVSSDKENGINE_API int getDecoderAddress(LONG identify, LONG lChannelIndex);
// 抓图.
/*
\param identify 初始化返回的句柄.
\param sFilePath 文件路径.
\return 抓图结果
*/
NVSSDKENGINE_API bool capturePictureEx(LONG identify, char* sFilePath);
// 制作关键针
/*
\param identify 初始化返回的句柄.
\return 无
*/
NVSSDKENGINE_API void makeKey(LONG identify);

// 重设显示区域（一机多屏刷新接口）
/*
\param identify 初始化返回的句柄.
\return 无
*/
NVSSDKENGINE_API void reSetDDrawDevice(LONG identify, HWND hWnd);

// 控制摄像头视频声音
/*
\param identify 初始化返回的句柄.
\param bOpenSound 设置状态 0-关闭声音 1-打开声音
\return 开启声音是否成功
*/
NVSSDKENGINE_API bool controlCameraSound(LONG identify, bool bOpenSound);

//执行云台控制_带通道
NVSSDKENGINE_API bool controlPTZSpeedWithChannel(LONG identify,int nCmd, bool bEnable, int nSpeed, int nChannel) ;

NVSSDKENGINE_API bool controlPTZSpeedWithChannelEx(LONG identify, PCONTROL_PARAM pParam);

//执行预置位_带通道
NVSSDKENGINE_API bool presetPTZWithChannel(LONG identify, int nCmd, int nIndex, int nChannel);

//获取回放OSD时间
/*
\param identify 初始化返回的句柄
\param lReplayHandle 播放句柄
\param pOsdTime 获取的时间
\return 获取时间结果
*/
NVSSDKENGINE_API bool getPlayBackOsdTime(LONG identify , LONG  lReplayHandle, char *pOsdTime);

//功能：设置当前通道
/*
\param identify 初始化返回的句柄
\param nChannel 通道号
\return 无
*/
NVSSDKENGINE_API void setCurrentChannel(LONG identify ,int nChannel);

//获取错误代码
/*
\return DVR错误码
*/
NVSSDKENGINE_API DWORD getLastError(LONG identify);

//远程回放时抓图
/*
\param identify 初始化返回的句柄
\param lReplayHandle 播放句柄
\param pFileName 保存图片的文件名
\return 成功与否
*/
NVSSDKENGINE_API  bool playBackCaptureFile(LONG identify , LONG  lReplayHandle, char *pFileName);
//设置远程回放时的抓图模式
/*
\param identify 初始化返回的句柄
\param wCaptureMode：抓图模式 0-BMP， 1-JPEG
\return 成功与否
*/
NVSSDKENGINE_API bool setCapturePictureMode(LONG identify ,DWORD dwCaptureMode);
//查询属性支持
/*
\	详情见XYPROPROPERTY定义
*/




//获取第三方软件的镜头信息(华三)
/*
\param identify 初始化返回的句柄
\param nSize 结构数组长度,(结构个数)
\return 返回结构(一维)数组指针
*/
NVSSDKENGINE_API LONG getCameraInfo(LONG identify ,int *nSize);

//华三专用
NVSSDKENGINE_API void setCurrentCameraCode(LONG identify ,char* sCameraCode);


//设置播放窗口句柄(华三)
/*
\param identify 初始化返回的句柄
\param palyWindow播放窗口
\param nMaxCount窗口个数，最大16个
*/
NVSSDKENGINE_API int setPlayWindow(LONG identify ,HWND palyWindow[], int nMaxCount);

//播放镜头到窗口句柄(华三)
/*
\param identify 初始化返回的句柄
\param hwndPlay 播放句柄
\param nCameraID 播放镜头ID
\return true:成功; false:失败
*/
NVSSDKENGINE_API bool  startPlayerByCamera(LONG identify, HWND hwndPlay, int nCameraID);

//停止播放窗口句柄(华三)
/*
\同上
\
\param hwndPlay
\
*/
NVSSDKENGINE_API bool  stopPlayerByCamera(LONG identify, HWND hwndPlay, int nCameraID);


//查询属性支持
/*
\	详情见XYPROPROPERTY定义
*/
NVSSDKENGINE_API bool queryPorperySupport(LONG identify, XYPROPROPERTY propery);


/************************************************************************/
/*                         文件流媒体使用                               */
/************************************************************************/

//打开文件
/*
\param sFileName :文件名
*/
NVSSDKENGINE_API LONG openVideoFile(LONG identify, char *sFileName);

//读文件流
/*param pBuff : 数据缓冲指针(输入)
nLen  : 缓冲长充
pOutStreamType : 流类型(DVR_DATA_INIT为数据头,  DVR_DATA_STREAM)
*/

NVSSDKENGINE_API int readVideoFile(LONG identify, char *pBuff,int nLen, int *pOutStreamType);

//关闭文件
/*
空
*/
NVSSDKENGINE_API bool closeVideoFile(LONG identify);


//文件播放控制
/*
Param:  nCmd:文件操作指令

PLAY_NOMAL，       1     正常播放
PLAY_FAST，        2     快进
PLAY_SLOW，        3     慢放
PLAY_ONEBYONE,     4     单帧播放
PLAY_ONEBYONEBACK, 5     单帧后退
PLAY_BACK,         6     后退
\return :成功true;失败 false
*/
NVSSDKENGINE_API bool controlPlay(LONG identify, int nCmd,int param, int *outValue);


/************************************************************************/
/*                         文件流媒体使用__end                          */
/************************************************************************/



/************************************************************************/
/*                    支持流媒体转发历史回放                            */
/************************************************************************/
/* 捕获视频流
\ param identify : 初始化返回的句柄
\ param lpCallBack:视频流回调
\ param pUser     :用户指针
\ param nStreamType 流类型: StreamType = 0 DVR历史文件流 ; StreamType = 1(FilePlug处理) 存储文件流
\ param FileInfo  : 文件信息
\ param sType:    文件plugIn类型
*/
NVSSDKENGINE_API LONG captureFileStream(LONG identify, LPCAPTURESTREAMCALLBACK lpCallBack, void* pUser, int nStreamType, RECORDFILE FileInfo, char* sType);


NVSSDKENGINE_API void ClearPlayBuffer(LONG identify);

NVSSDKENGINE_API void pusePlayDecode(LONG identify,bool bPuse);
NVSSDKENGINE_API bool startRecord(LONG identify,  char* saveFile);

NVSSDKENGINE_API bool stopRecord(LONG identify);


/***************add by zjf for GB/T28181, Time: 2013-09-26*****************/


//取正在播放的镜头的句柄
/*
Param:  sCameraID: 镜头国标20位编码
bFlag: 无意义, 和另外一个GetIdentify区分
return:>0 成功, <=0失败*/
NVSSDKENGINE_API long GetIdentify(char * sCameraID, bool bFlag);

//设置YUV回调函数(对应解码库XY_SetStreamCallBackForVideo()接口)
/*
Param:  identify: 句柄
pCallBack: 回调函数, 定义见上面.
return:true 成功, false 失败*/
NVSSDKENGINE_API bool SetYUVCallBack(LONG identify, LPYUVCALLBACK pCallBack);

//局部放大(对应解码库XY_SetCropArea()接口)
/*
Param:  identify: 句柄
bFlag: 无意义
x1, y1, x2, y2分别为鼠标在播放窗体上的左上及右下的坐标, 全部为0时表示停止局部放大.
return:true 成功, false 失败*/
NVSSDKENGINE_API bool SetCropArea(LONG identify, bool bFlag = false, int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0);

//视频解码智能调节(根据nCmd参数, 分别对应解码库XY_SetClearFogParam(), XY_OpenOrCloseClearFog(), XY_OpenOrCloseInvert(), XY_OpenOrCloseSharpen()接口)
/*
Param:  identify: 句柄
nCmd: 智能调节的类型, 1为去雾, 2为反色, 3为锐化, 4为亮度调节
bFlag: true为执行智能调节, false为取消
x1, y1: 去雾时表示起始点坐标, 高度调节时为参数
x2, y2: 亮度调节表示终结点坐标.
return: 0成功, <0失败*/
NVSSDKENGINE_API bool SetDecodeParam(LONG identify, int nCmd, bool bFlag = false, int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0);

/****以下三个接口是为了实现在客户端播放实时视频时的15秒快速回放****/

//设置实时播放时保存流数据的时间, 默认15秒
/*
Param:  identify: 句柄
nSaveTime: 保存的时间(单位:秒）
return: true成功, false失败*/
NVSSDKENGINE_API bool SetStreamSaveTime(LONG identify, int nSaveTime = 15);

//回放保存的视频流
/*
Param:  identify: 句柄;
hWnd: 窗口句柄
return: true成功, false失败*/
NVSSDKENGINE_API bool PlayBackSaveStream(LONG identify, HWND hWnd);

//停止回放保存的视频流
/*
Param:  identify: 句柄
return: true成功, false失败*/
NVSSDKENGINE_API bool StopPlayBackSaveStream(LONG identify);

/*******************add by zjf End*****************************/

/***************add by pzm, Time: 2013-09-27*****************/

//设置图像的渲染模式，默认为D3D渲染
/*
Param:identify: 句柄;
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool SetRenderMode(LONG identify, int nMode = RENDER_BY_D3D);

//根据帧号定位,定位至nFrameNum最近的关键帧（往前查找）
/*
Param:identify: 句柄；nFrameNum 帧号
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool SetPosByFrameNum(LONG identify,DWORD nFrameNum);

//截取beginTime和endTime之间的录像，保存至filename所指定的路径，beginTime和endTime为秒级时间
/*
Param:identify: 句柄;
beginFrame：截取开始的时间；
endFrame：截取结束的时间；
filename：录像保存的绝对路径
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool GetVideoCut(LONG identify, DWORD beginFrame, DWORD endFrame, LPSTR filename);

//中止文件剪切
/*
Param:identify: 句柄;
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool StopCutVideo(LONG identify);

//设置文件结束回调
/*
Param:identify: 句柄;
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool SetFileEndCallback(LONG identify, void *pUser, pFileOverCB pCallBack);// 通知使用（为了确认视频文件已经播放完毕，因为部分视频文件播放完毕时，当前帧不等于总帧数）

//获取媒体信息
/*
Param:identify: 句柄；mInf用来获取媒体信息的结构指针
OutParam:无
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool GetMediaInfo(LONG identify,MediaInf &mInf);

//获取亮度调节模式参数
/*
Param:nPort				XY_GetPort获取的通道号
OutParam:gray1			    亮度调节参数的灰度值1
		  gray2				亮度调节参数的灰度值2
返回值：成功返回true，否则返回false
*/
NVSSDKENGINE_API bool GetLightParam(LONG nPort,OUT int *gray1,OUT int *gray2);

//视频解码获取配置参数
/*
Param:identify: 句柄； 
nCmd: 智能调节的类型, 1为去雾, 2为反色, 3为锐化, 4为亮度调节
OutParam:
		  当nCmd为1去雾调节时:
		  x1				起始点X坐标
		  y1				起始点Y坐标
		  x2				结束点X坐标
		  y2				结束点Y坐标
		  当nCmd为4亮度调节时:
		  x1 为gray1	亮度调节参数的灰度值1
		  y1 为gray2	亮度调节参数的灰度值2
return: 0成功, <0失败*/
NVSSDKENGINE_API bool GetDecodeParam(LONG identify, int nCmd, int &x1, int &y1, int &x2, int &y2);
/*******************add by pzm End*****************************/
