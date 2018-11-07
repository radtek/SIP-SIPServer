#pragma once


#include <log4cxx/log4cxx.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/xml/domconfigurator.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/logstring.h>
#include <log4cxx/helpers/exception.h>
#include <log4cxx/helpers/loglog.h>

using namespace std;
using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace log4cxx::xml;

enum LoggerLevel{TRACELEVEL, DEBUGLEVEL, INFOLEVEL, WARNLEVEL, ERRORLEVEL, FATALLEVEL};

class CLogRecorder
{
public:
	CLogRecorder(void);
	~CLogRecorder(void);
public:
	bool InitLogger(const char * pszLogConfigPath, const char * pszLoggerName, const char * pszModuleName);
	void WriteLog(const char * pszFunction, const char * pszLogContent, LoggerLevel nLevel=INFOLEVEL);
	void WriteDebugLog(const char * pszFunction, const char * pszLogContent);
	void WriteInfoLog(const char * pszFunction, const char * pszLogContent);
	void WriteWarnLog(const char * pszFunction, const char * pszLogContent);
	void WriteErrorLog(const char * pszFunction, const char * pszLogContent);
	void WriteFatalLog(const char * pszFunction, const char * pszLogContent);

	void WriteLogEx(LoggerLevel nLevel, const char * pszFunction, const char * pszFormat, ...);
	void WriteDebugLogEx(const char * pszFunction,const char * pszFormat, ...);
	void WriteInfoLogEx(const char * pszFunction, const char * pszFormat, ...);
	void WriteWarnLogEx(const char * pszFunction, const char * pszFormat, ...);
	void WriteErrorLogEx(const char * pszFunction, const char * pszForamt, ...);
	void WriteFatalLogEx(const char * pszFunction, const char * pszFormat, ...);
private:
	char m_szModuleName[64];	// Module Name
	LoggerPtr m_logger;
	
};
