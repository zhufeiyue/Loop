#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <map>

int RecordCDNInfo(
	const std::string&,
	std::string,
	std::string);
int GetCDNInfo(
	const std::string&,
	std::string&,
	std::string&
);

struct TsDownloadRecord 
{
	std::string originAddress;
	std::string proxyAddress;
	int         tsSize;
	double      tsDuration;
	double      tsDownloadTime;
	int64_t     recordTime;
};
int RecordTsDownloadInfo(
	const std::string&,
	TsDownloadRecord&
);
int GetTsDownloadInfo(
	const std::string&,
	std::vector<TsDownloadRecord>&
);

struct HttpRequestErrorInfo
{
	std::string strUrl;
	std::string strTime;
	std::string strErrorMsaage;
	std::string strHttpResponData;
	int httpResponCode;
};
int RecordHttpRequestErrorInfo(
	const std::string&,
	HttpRequestErrorInfo&
);
int GetHttpRequestErrorInfo(
	const std::string&,
	std::vector<HttpRequestErrorInfo>&
);

int ClearMonitorInfo(const std::string&);