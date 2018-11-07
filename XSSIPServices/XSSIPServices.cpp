// XSSIPServices.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ParamRead.h"
#include "SipServer.h"

extern CSIPServer g_sip;

#ifdef _DEBUG

int _tmain(int argc, _TCHAR* argv[])
{
    if(!g_sip.StartRun())
    {
        printf("\n\n按回车键结束!\n");
    }
    //_CrtDumpMemoryLeaks();
    getchar();
    return 0;
}

#else

#include "ntservice.h"
#include <iostream>
#define SERVICE_NAME "XY_SIPServer"
//本地函数定义
static BOOL WINAPI		HandlerRoutine( DWORD dwCtrlType );
static LONG __stdcall	GlobalCrashHandlerExceptionFilter(EXCEPTION_POINTERS* pExPtrs);

//服务局柄
static CNTService NTService(SERVICE_NAME);
using namespace std;

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
//int main(int argc, char* argv[])
{
	int iCode = 0;

	///////////////////////////////////
	LPTOP_LEVEL_EXCEPTION_FILTER oldExceptionFilter; ///旧的异常处理函数
	oldExceptionFilter = SetUnhandledExceptionFilter( GlobalCrashHandlerExceptionFilter);
	///设置控制处理函数
	SetConsoleCtrlHandler( 
		(PHANDLER_ROUTINE)HandlerRoutine,	// handler function
		true								// add or remove handler
		);
	//////////////////////////////////////
	try
	{
		// Create the service object

		// 解析标准的命令行参数 (如安装、卸载、版本等)
		NTService.ParseStandardArgs(argc, argv);
		// When we get here, the service has been stopped
		iCode = NTService.m_Status.dwWin32ExitCode;
	}
	catch(...)
	{
		OutputDebugString("执行服务主程序异常出错! \n");		
		NTService.OnStop(); //停止服务
	}

	//删除控制处理函数
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE)HandlerRoutine,	// handler function
		false								// add or remove handler
		);
	SetUnhandledExceptionFilter( oldExceptionFilter);

	return iCode;

}

BOOL WINAPI HandlerRoutine( DWORD dwCtrlType )
{
	switch( dwCtrlType){
  case CTRL_C_EVENT:		//  0
	  break;
  case CTRL_BREAK_EVENT:	//  1
	  break;
  case CTRL_CLOSE_EVENT:	//  2
	  break;
  case CTRL_SHUTDOWN_EVENT: //	6  
	  break;
  default:
	  return false;
	}

	NTService.OnStop();		//停止

	ExitProcess(0);			//停止程序的运行

	return true;
}

LONG __stdcall GlobalCrashHandlerExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
	LONG lRet = EXCEPTION_CONTINUE_SEARCH;
	__try
	{
		OutputDebugString("error on global error???");
	}
	__except ( EXCEPTION_EXECUTE_HANDLER ){
		lRet = EXCEPTION_CONTINUE_SEARCH;
	}
	return (lRet);
}



#endif
