#pragma once
#include "HLSPlaylist.h"

void testHlsProxy(int argc, char* argv[]);

class HlsProxy
{
public:
	HlsProxy();
	~HlsProxy();
	int StartProxy(std::string strOriginAddress, std::string& strProxyAddress);
	int StopProxy();

private:
	int GetContent(std::string&);

private:
	std::unique_ptr<HlsPlaylist> m_pOriginPlaylist;
	std::string                  m_strOriginM3U8Address;

	std::mutex  m_lock;
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
	std::map<std::string, HlsProxy*> m_mapProxy;
};