#pragma once
#include "NVSSDKDef.h"

class IDVRHandler
{
public:
	virtual bool connectDVR(char* sIP,int nPort,char* sUser,char* sPwd,int nChannel)=0;

	virtual void disconnectDVR()=0;

	virtual void freeLoginInfo()=0;
	// -由于增加接口需要设置DVR上通道的名称和检测这个DVR是否能够登陆上，加以下两个接口（郑峰）-
	//检测DVR
	virtual LONG checkDVR(char* sIP, int nPort, char* sUser, char* sPwd,CLInfo pInfo[],int nMaxCount)=0;
	//设置信息
	virtual void setDVR(char* sIP, int nPort, char* sUser, char* sPwd,CLInfo pInfo[],int nInfoCount)=0;
	// -----------------------------------------------------------------------------------------
	
	virtual void releaseHandler()=0;

	virtual bool captureStream(LPCAPTURESTREAMCALLBACK lpCallBack, void* pUser, int nGetVideoType)=0;

	virtual bool stopCaptureStream()=0;	

	virtual int getRecordFileEx(int nType,char* startTime,char* endTime,RECORDFILE pRecordFile[],int nMaxFileCount)=0;
	virtual RECORDFILE* getRecordFile(int nType,char* startTime,char* endTime,int* nFileCount)=0;

	virtual LONG downloadRecordFile(char* fileName,char* startTime, char* endTime,int nFileSize,char* saveFile,LPDOWNLOADRECORDFILECALLBACK lpCallBack,void* pUser)=0;
	
	virtual bool stopDownloadRecordFile(LONG ldownFileHandle)=0;

	virtual LONG replayRecordFile(int hWnd,char* fileName,char* startTime, char* endTime,int nFileSize)=0;

	virtual bool controlReplayRecordFile(LONG lReplayHandle,int nCmd,int nInValue,int* outValue)=0;

	virtual bool stopReplayRecordFile(LONG lReplayHandle)=0;

	virtual bool startTalk()=0;

	virtual bool stopTalk()=0;

	virtual bool controlPTZ(int nCmd, bool bEnable)=0;
    
	virtual bool presetPTZ(int nCmd, int nIndex)=0;

	virtual bool showSettingPane()=0;

	virtual bool initialPlayer(int nPort,int nDecodeType,BYTE* buff, int nSize, int nStreamType)=0;
    
	virtual bool startPlayer(HWND hWnd)=0;

	virtual bool startPlayer2(HWND hWnd, LPDRAWCBCALLLBACK lpDrawCBFun, void *pUser)=0;

	//2007-04-23
	virtual bool startPlayer4Standard(HWND hWnd,LPDecCBFun lpDecCBFun, int nCameraID)=0;
	
	virtual bool addData(BYTE* buff, int nSize)=0;
	
	virtual bool stopPlayer()=0;

	virtual bool locPlay_OpenFile(int nPort,int nDecodeType,int hWnd,char* fileName)=0;

	virtual bool locPlay_Control(int nCmd,int nInValue,int* outValue)=0;

	virtual bool locPlay_Stop()=0;

	virtual bool alarm_startCapture(int nPort,LPALARMCALLBACK lpCallBack)=0;

	virtual bool alarm_stop()=0;

	virtual int log_QueryEx(LOGQUERYTYPE nQueryType, LOGERRORTYPE nErrorType,char* startTime,char* endTime,LOGINFO pLogInfo[],int nMaxLogCount)=0;

	virtual LOGINFO* log_Query(LOGQUERYTYPE nQueryType, LOGERRORTYPE nErrorType,char* startTime,char* endTime,int* nLogCount)=0;
	
	virtual bool setVideoEffect(DWORD dwBrightValue,DWORD dwContrastValue, DWORD dwSaturationValue,DWORD dwHueValue)=0;
	virtual bool getVideoEffect(DWORD *pBrightValue,DWORD *pContrastValue, DWORD *pSaturationValue,DWORD *pHueValue)=0;
	virtual bool setDVRConfig(DWORD dwCommand,LONG lChannel,LPVOID lpInBuffer,DWORD dwInBufferSize)=0;
	virtual bool getDVRConfig(DWORD dwCommand,LONG lChannel,LPVOID lpOutBuffer,DWORD dwOutBufferSize,LPDWORD lpBytesReturned)=0;
	virtual bool convertToBmpFile(char * pBuf,LONG nSize,LONG nWidth,LONG nHeight,LONG nType,char *sFileName)=0;
	virtual bool oneByOneBack(LONG nPort)=0;
	virtual bool oneByOne(LONG nPort)=0;
	virtual bool setPlayedTimeEx(LONG nPort,DWORD nTime)=0;
	virtual bool setFileRefCallBack(LONG nPort,void (__stdcall *pFileRefDone) (DWORD nPort,DWORD nUser),DWORD nUser)=0;
	virtual bool setDisplayCallBack(LONG nPort, void (__stdcall *pFileRefDone)(long nPort,char * pBuf,long nSize,long nWidth,long nHeight,long nStamp,long nType,long nReserved))=0;
	virtual bool playBackSaveData(LONG lReplayHandle,char *sFileName)=0;
	virtual bool stopPlayBackSave(LONG lReplayHandle)=0;
	virtual int getDownloadPos(LONG lFileHandle)=0;
	virtual bool getDVRWorkState(char dvrWorkState[],int arrayLen)=0;
	virtual bool getDVRIPByResolveSvr(char *sServerIP,int wServerPort,char *sDVRName,int wDVRNameLen,char *sDVRSerialNumber,int wDVRSerialLen,char* sGetIP)=0;
	virtual bool setConnectTime(DWORD dwWaitTime,DWORD dwTryTimes)=0;
	virtual bool startListen (char *sLocalIP,int wLocalPort)=0;
	virtual bool stopListen()=0;
	virtual bool serialSend(char* pBuf,DWORD nLength,char* pBuf2,DWORD nLen2)=0;
	virtual bool serialSendWithCallBack(char* pBuf,DWORD nLength,int nCameraID)=0;
	virtual bool buildSerialSend(LONG lSerialPort,LPSERIALDATACALLBACK pSerialCallBack,bool bOpen)=0;
	virtual void makeKey()=0;
	//启动PC 端声音捕获
	virtual bool clientAudioStart()=0;
	//停止PC 端声音捕获
	virtual bool clientAudioStop()=0;
	//添加一台DVR 到可以接收PC 机声音的组里
	virtual bool addDVR()=0;
	//删除已经加到组的DVR
	virtual bool delDVR()=0;
	virtual bool setVoiceComClientVolume(WORD wVolume)=0;
	virtual bool controlPTZSpeed(int nCmd, bool bEnable,int nSpeed)=0;
	
	//2007-05-23增加 获取通道解码器地址
	virtual int getDecoderAddress(LONG lChannelIndex)=0;
	//2007-05-25 增加
	virtual bool capturePicture(char* sFilePath)=0;
	//2008-03-10 增加
	virtual void reSetDDrawDevice(HWND hWnd)=0;

	//2008-03-10 增加
	virtual bool controlCameraSound(bool bOpenSound)=0;
	//2009-07-20 增加
	virtual bool controlPTZSpeedWithChannel(int nCmd, bool bEnable, int nSpeed, int nChannel) = 0;
	//2009-07-20 增加
	virtual bool presetPTZWithChannel(int nCmd, int nIndex, int nChannel)=0;
    //2009-07-30 增加
	virtual bool getPlayBackOsdTime(LONG  lReplayHandle, char *pOsdTime)=0;
	//2009-0730 增加
	virtual void setCurrentChannel(int nChannel)=0;
	//2009-0730 增加
	virtual DWORD getLastError()=0;
	//2009-07-30 增加
	virtual bool playBackCaptureFile(LONG lReplayHandle, char *pFileName)=0;
	//设置远程回放的抓图模式
	virtual bool setCapturePictureMode(DWORD dwCaptureMode)=0;

	
	//2009-08-17 增加(用于支持华三)
	virtual LONG getCameraInfo(int *nSize)=0;

	virtual int setPlayWindow(HWND palyWindow[], int nMaxCount)=0;

	virtual bool startPlayerByCamera(HWND hwndPlay, int nCameraID)=0;
	virtual bool  stopPlayerByCamera(HWND hwndPlay, int nCameraID)=0;
	virtual bool queryPorperySupport(XYPROPROPERTY propery) = 0;
	virtual void setCurrentCameraCode(char* sCameraCode)=0;
	/*文件流媒体播放控制*/
	virtual LONG openVideoFile(char *sFileName)=0;
	virtual int readVideoFile(char *pBuff,int nLen, int *pOutStreamType)=0;
	virtual bool closeVideoFile()=0;
	virtual bool controlPlay(int nCmd,int param, int *outValue)=0;
	virtual LONG captureFileStream(LPCAPTURESTREAMCALLBACK lpCallBack, void* pUser, int nStreamType, RECORDFILE FileInfo, char* sType)=0;
	virtual void ClearPlayBuffer() = 0;
	virtual void pusePlayDecode(bool bPuse) = 0;
	virtual bool startRecord(char* saveFile) = 0;
	virtual bool stopRecord() = 0;
};
