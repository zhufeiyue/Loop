#pragma once
#include "HLSPlaylist.h"
#include "SimpleHttpServer.h"
#include <QMap>

void testHlsProxy(int argc, char* argv[]);

struct HlsProxyParam
{
	std::string strHlsAddress;
	std::string strDefaultVariant;
	std::string strProxyAddress;
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
	// hls seek��ֻ��vod��Ч������pos����λ��Ƭ���л�������Ƭ�������ش���Ƭ����ʼʱ��
	int Seek(uint64_t pos, double& newStartPos);

private:
	int HandleSeek(HttpConnection& conn, QMap<QString, QString>&);
	int HandleSwitchVariant(HttpConnection& conn, QMap<QString, QString>&);

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
	int HandleStartPlayProxy(HttpConnection& conn, QMap<QString, QString>&);
	int HandleStopPlayProxy(HttpConnection& conn, QMap<QString, QString>&);

private:
	std::map<std::string, HlsProxy*> m_mapProxy;
};