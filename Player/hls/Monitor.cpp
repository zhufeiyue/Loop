#include "Monitor.h"

struct CDNInfo
{
	std::string cdnsip;
	std::string cdncip;
};

static std::map<std::string, CDNInfo>                       mapCDNInfo;
static std::map<std::string, std::vector<TsDownloadRecord>> mapTsDownloadInfo;

int RecordCDNInfo(
	const std::string& strSession,
	std::string cdnsip,
	std::string cdncip)
{
	mapCDNInfo[strSession].cdnsip = std::move(cdnsip);
	mapCDNInfo[strSession].cdncip = std::move(cdncip);
	return 0;
}

int GetCDNInfo(
	const std::string& strSession,
	std::string& cdnsip ,
	std::string& cdncip)
{
	auto iter = mapCDNInfo.find(strSession);
	if (iter != mapCDNInfo.end())
	{
		cdnsip = iter->second.cdnsip;
		cdncip = iter->second.cdncip;
	}
	else
	{
		cdnsip = "NV";
		cdncip = "NV";
	}

	return 0;
}

int RecordTsDownloadInfo(
	const std::string& strSession,
	TsDownloadRecord& record
)
{
	record.recordTime = std::chrono::steady_clock::now().time_since_epoch().count();
	mapTsDownloadInfo[strSession].push_back(std::move(record));
	return 0;
}

int GetTsDownloadInfo(
	const std::string& strSession,
	std::vector<TsDownloadRecord>& v
)
{
	v = std::move(mapTsDownloadInfo[strSession]);
	return 0;
}

int ClearMonitorInfo(const std::string& strSession)
{
	mapCDNInfo.erase(strSession);
	mapTsDownloadInfo.erase(strSession);

	return 0;
}