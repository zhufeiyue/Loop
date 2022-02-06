#include "M3U8Parser.h"

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

bool M3U8Parser::IsMain() const
{
	for (auto iter = m_vecLines.begin(); iter != m_vecLines.end(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-STREAM-INF", 17) == 0)
		{
			return true;
		}
	}
	return false;
}

bool M3U8Parser::IsVod() const
{
	for (auto iter = m_vecLines.rbegin(); iter != m_vecLines.rend(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-ENDLIST", 14) == 0)
		{
			return true;
		}
	}
	return false;
}

bool M3U8Parser::IsLive() const
{
	return !IsVod();
}

int M3U8Parser::GetSubM3U8Info(std::vector<Dictionary>& items)
{
	items.clear();

	Dictionary dic;
	size_t pos;
	for (size_t i = 0; i < m_vecLines.size(); ++i)
	{
		if (strncmp(m_vecLines[i].c_str(), "#EXT-X-STREAM-INF", 17) == 0)
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
	return 0;
}