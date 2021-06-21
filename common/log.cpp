#include "log.h"

#ifdef _MSC_VER
#include <Windows.h>
#endif

static std::ofstream gLogFile;
int InitLogger(const char* logFileNamePrefix)
{
	if (!logFileNamePrefix)
	{
		return CodeNo;
	}
	if (gLogFile.is_open())
	{
		return CodeNo;
	}

	time_t now;
	time(&now);
	struct tm tmnow;
#ifdef _MSC_VER
	localtime_s(&tmnow, &now);
#else
	localtime_r(&now, &tmnow);
#endif

	std::stringstream ss;
	ss << logFileNamePrefix << '_'
		<< std::setw(2) << std::setfill('0') << (tmnow.tm_mon + 1) << '_'
		<< std::setw(2) << std::setfill('0') << tmnow.tm_mday 
		<< ".log";

	gLogFile.open(ss.str(), std::ofstream::out | std::ofstream::app);
	if (!gLogFile.is_open())
	{
		return CodeNo;
	}

	std::clog.rdbuf(gLogFile.rdbuf());

	return CodeOK;
}

//static thread_local std::stringstream gSS;
LogPrintHelper::LogPrintHelper()
{
}

LogPrintHelper::~LogPrintHelper()
{
	m_ss << std::endl;
	auto logString = m_ss.str();
	//gSS.str("");

	std::clog << logString;

#ifdef _MSC_VER
	OutputDebugStringA(logString.c_str());
#endif
}

std::stringstream& LogPrintHelper::Log()
{
	auto now = std::chrono::system_clock::now();
	auto now_t = std::chrono::system_clock::to_time_t(now);
	auto dur = now - std::chrono::system_clock::from_time_t(now_t);
	auto mill = std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();

	struct tm now_tm;
#ifdef _MSC_VER
	localtime_s(&now_tm, &now_t);
#else
	localtime_r(&now_t, &now_tm);
#endif

	m_ss << std::setw(2) << std::setfill('0') <<(now_tm.tm_mon + 1) << '-' 
		<< std::setw(2) << std::setfill('0') << now_tm.tm_mday << ' '
		<< std::setw(2) << std::setfill('0') << now_tm.tm_hour << ':'
		<< std::setw(2) << std::setfill('0') << now_tm.tm_min << ":"
		<< std::setw(2) << std::setfill('0') << now_tm.tm_sec 
		<< '.' << mill << ' ';

	return m_ss;
}