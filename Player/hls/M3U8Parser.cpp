#include "M3U8Parser.h"

#ifndef _MSC_VER
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif
std::vector<std::string> split_string(const std::string&, char);

M3U8Parser::M3U8Parser(const std::string& strData)
{
	std::string strLine;
	std::istringstream stream(strData);

	while (std::getline(stream, strLine, '\n'))
	{
		if (!strLine.empty())
			m_vecLines.push_back(std::move(strLine));
	}
}

M3U8Parser::~M3U8Parser()
{
}

bool M3U8Parser::IsValid() const
{
	if (m_vecLines.size() < 3)
	{
		return false;
	}

	if (strncmp(m_vecLines[0].c_str(), "#EXTM3U", 7) != 0)
	{
		return false;
	}

	return true;
}

bool M3U8Parser::IsMaster() const
{
	for (auto iter = m_vecLines.begin(); iter != m_vecLines.end(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-STREAM-INF:", 18) == 0)
		{
			return true;
		}
	}
	return false;
}

std::string M3U8Parser::GetType() const
{
	for (auto iter = m_vecLines.begin(); iter != m_vecLines.end(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-PLAYLIST-TYPE:", 21) == 0)
		{
			// VOD playlist不可被修改，是点播
			// EVENT playlist只能从尾部添加新segment。这与LIVE还有有区别，LIVE维护一个segment区间，可以从头部删除、尾部添加
			if (strnicmp(iter->c_str() + 21, "VOD", 3) == 0)
				return "VOD";
			else if (strnicmp(iter->c_str() + 21, "EVENT", 5) == 0)
				return "EVENT";
		}
	}

	for (auto iter = m_vecLines.rbegin(); iter != m_vecLines.rend(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-ENDLIST", 14) == 0)
			return "VOD";
	}

	return "LIVE";
}

int64_t M3U8Parser::GetSequenceNumber()const
{
	for (auto iter = m_vecLines.begin(); iter != m_vecLines.end(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-MEDIA-SEQUENCE:", 22) == 0)
			return atoll(iter->c_str() + 22);
	}

	return 0;
}

int M3U8Parser::GetVariantInfo(std::vector<Dictionary>& items)
{
	items.clear();

	Dictionary dic;
	size_t pos;
	for (size_t i = 0; i < m_vecLines.size(); ++i)
	{
		if (strncmp(m_vecLines[i].c_str(), "#EXT-X-STREAM-INF:", 18) == 0)
		{
			if (i + 1 >= m_vecLines.size())
			{
				return -1;
			}

			dic.insert("address", m_vecLines[i + 1]);
			if ((pos = m_vecLines[i].find("BANDWIDTH=")) != std::string::npos)
			{
				dic.insert("BANDWIDTH", atoi(m_vecLines[i].c_str()+ pos + 10));
			}

			items.push_back(std::move(dic));
		}
	}

	return 0;
}

int M3U8Parser::GetSegmentInfo(std::vector<Dictionary>& items)
{
	items.clear();

	Dictionary dic;
	size_t pos;
	for (size_t i = 0; i < m_vecLines.size(); ++i)
	{
		//#EXTINF:<duration>,[<title>]
		if (strncmp(m_vecLines[i].c_str(), "#EXTINF:", 8) == 0)
		{
			if (i + 1 >= m_vecLines.size())
			{
				return -1;
			}


			auto temp = split_string(m_vecLines[i].c_str() + 8, ',');
			if (!temp.empty())
			{
				dic.insert("duration", atof(temp[0].c_str()));
			}
			if (temp.size() == 2)
			{
				dic.insert("title", temp[1]);
			}
			dic.insert("address", m_vecLines[i + 1]);
			items.push_back(std::move(dic));
		}
	}
	return 0;
}