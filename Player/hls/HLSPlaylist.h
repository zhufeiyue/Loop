#pragma once

#include <common/Dic.h>
#include <common/log.h>
#include "M3U8Parser.h"

class HttpClient;
class HlsPlaylist
{
public:
	HlsPlaylist();
	~HlsPlaylist();
	int SetM3U8Address(std::string);

protected:
	int GetM3U8(std::string);
	void OnGetM3U8(std::string, Dictionary);
	void OnGetM3U8Error(Dictionary);

protected:
	std::string m_strM3U8Address;
	std::shared_ptr<HttpClient> m_pHttpClient;
	std::unique_ptr<M3U8Parser> m_pParser;
};

void testHls();