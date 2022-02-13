#pragma once

#include <common/Dic.h>
#include <common/log.h>

class HlsPlaylist
{
public:
	struct Variant
	{

	};

	struct Segment
	{

	};
public:
	HlsPlaylist();
	~HlsPlaylist();
	int SetM3U8Address(std::string);

protected:
	int GetM3U8(std::string);
	void OnGetM3U8(std::string, Dictionary);
	void OnGetM3U8Error(Dictionary);

	int FormatSubAddress(std::vector<Dictionary>&, std::string);

protected:

};

void testHls();