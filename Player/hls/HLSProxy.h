#pragma once
#include "HLSPlaylist.h"
#include "SimpleHttpServer.h"
#include <QMap>

void testHlsProxy(int argc, char* argv[]);
int GetServer(std::shared_ptr<SimpleHttpServer>&);

enum HlsProxyCode
{
	OK = 0,
	InvalidParam,
	Duplicate,
	StartProxyError,
	SeekError,
	SwitchError,
	MonitorError
};

struct HlsProxyParam
{
	std::string strHlsAddress;
	std::string strDefaultVariant;
	std::string strSessionId;
	std::string strMediaType;
	std::string strCDNSip;
	std::string strCDNCip;
	std::string strProxyAddress;
	std::string strErrorMessage;
	double  variantDuration;
	int64_t variantIndex;
	
	std::vector<Dic> allVariantInfo;
};

class HlsProxy
{
public:
	HlsProxy();
	virtual~HlsProxy();
	virtual int StartProxy(HlsProxyParam&);
	virtual int StopProxy();

private:
	int GetContent(std::string&);
	// hls seek，只对vod有效。根据pos，定位切片，切换到此切片，并返回此切片的起始时间
	int Seek(uint64_t pos, double& newStartPos);

private:
	int HandleMonitor(HttpConnectionPtr conn, QMap<QString, QString>&);
	int HandleSeek(HttpConnectionPtr conn, QMap<QString, QString>&);
	int HandleSwitchVariant(HttpConnectionPtr conn, QMap<QString, QString>&);

private:
	std::unique_ptr<HlsPlaylist> m_pOriginPlaylist;
	std::string                  m_strOriginM3U8Address;

	std::string m_strProxyName;
	std::string m_strLastResponCntent;
	int64_t     m_iProxySegNo = 0;
};

class HlsProxyManager
{
public:
	HlsProxyManager();
	~HlsProxyManager();
	int Start();
	int Stop();

private:
	int HandleStartPlayProxy(HttpConnectionPtr conn, QMap<QString, QString>&);
	int HandleStopPlayProxy(HttpConnectionPtr conn, QMap<QString, QString>&);

private:
	std::map<std::string, HlsProxy*> m_mapProxy;
};