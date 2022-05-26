#include "M3U8Parser.h"
#include <common/log.h>
#include <common/ParseUrl.h>

#ifndef _MSC_VER
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

static std::vector<std::string> split_string(const std::string& s, char ch)
{
	std::vector<std::string> parts;

	std::istringstream stream(s);
	std::string strPart;
	while (std::getline(stream, strPart, ch))
	{
		parts.push_back(std::move(strPart));
	}

	return parts;
}

static std::string join_string(const std::vector<std::string>& items, char ch)
{
	std::string res;
	for (size_t i = 0; i < items.size(); ++i)
	{
		res += items[i];
		if (i != items.size() - 1)
		{
			res += ch;
		}
	}

	return res;
}

static void trime_string(std::string& s)
{
	size_t pos1, pos2;
	for (size_t i = 0; i < s.length(); ++i)
	{
		if (!isspace(s[i]))
		{
			pos1 = i;
			break;
		}
	}

	for (size_t i = s.length() - 1; i >= 0; --i)
	{
		if (!isspace(s[i]))
		{
			pos2 = i;
			break;
		}
	}

	if (pos1 >= s.length() || pos2 < 0 || pos2 < pos1)
	{
		s.clear();
		return;
	}

	s = s.substr(pos1, pos2 - pos1 + 1);
}

static bool is_absolute_uri(const std::string& uri)
{
	if (strnicmp(uri.c_str(), "http", 4) == 0)
	{
		return true;
	}

	return false;
}

static int FormatSubAddress(std::vector<Dictionary>& sub_items, std::string m3u8_url)
{
	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	auto last = std::chrono::steady_clock::now();
	if (!ParseUrl(m3u8_url, scheme, host, path, port))
	{
		LOG() << __FUNCTION__ << " parse url error: " << m3u8_url;
		return CodeNo;
	}
	auto now = std::chrono::steady_clock::now();
	LOG() << "ParseUrl use " << std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() << "ms";

	auto paths = split_string(path, '/');
	if (paths.empty())
	{
		LOG() << __FUNCTION__;
		return CodeNo;
	}

	//下面这个循环，计算子地址
	for (auto iter = sub_items.begin(); iter != sub_items.end(); ++iter)
	{
		std::string sub_uri = iter->find("address")->second.to<std::string>();
		if (is_absolute_uri(sub_uri))
		{
			continue;
		}

		std::string sub_url;
		if (sub_uri[0] == '/')
		{
		}
		else
		{
			auto temp = paths;
			temp.back() = sub_uri;
			sub_uri = join_string(temp, '/');
		}

		sub_url = scheme + "://" + host + (port != 0 ? ":" + std::to_string(port) : "") + sub_uri;
		iter->find("address")->second = Dictionary::DictionaryHelper(sub_url);
	}
	return CodeOK;
}

M3U8Parser::M3U8Parser(const std::string& strData)
{
	std::string strLine;
	std::istringstream stream(strData);

	while (std::getline(stream, strLine, '\n'))
	{
		if (!strLine.empty())
		{
			trime_string(strLine);
			m_vecLines.push_back(std::move(strLine));
		}
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

	"EXT-X-BYTERANGE";

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

int64_t M3U8Parser::GetTargetDuration() const
{
	for (auto iter = m_vecLines.begin(); iter != m_vecLines.end(); ++iter)
	{
		if (strncmp(iter->c_str(), "#EXT-X-TARGETDURATION:", 22) == 0)
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
			dic.insert("type", "unknown");

			auto parts = split_string(m_vecLines[i].c_str() + 18, ',');
			for (auto iter = parts.begin(); iter != parts.end(); ++iter)
			{
				if ((pos = iter->find("BANDWIDTH=")) != std::string::npos)
				{
					dic.insert("bandwidth", atoll(iter->c_str()+ pos + 10));
				}
				else if ((pos = iter->find("RESOLUTION=")) != std::string::npos)
				{
					dic.insert("resolution", iter->c_str() + pos + 11);
				}
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
	auto segNo = GetSequenceNumber();
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
			dic.insert("no", segNo++);
			items.push_back(std::move(dic));
		}
	}
	return 0;
}

#include <common/AsioHttp.h>
int ParseM3U8(std::string strPlaylistUrl, Dictionary& info, std::vector<Dictionary>& items)
{
	Dictionary responData;

	LOG() << "parse m3u8 " << strPlaylistUrl;
	SimpleHttpGet(strPlaylistUrl, responData, 5000);

	auto ok = responData.get<int>("ok");
	if (!ok)
	{
		std::string strMessage = responData.get<std::string>("message");
		LOG() << strMessage;
		return CodeNo;
	}

	auto httpStatusCode = responData.get<int>("status");
	auto trueUrl = responData.get<std::string>("url");
	if (httpStatusCode != 200)
	{
		LOG() << httpStatusCode;
		return CodeNo;
	}

	auto iterData = responData.find("data");
	if (iterData == responData.end())
	{
		return CodeNo;
	}
	auto& data = iterData->second.toRef<std::string>();
	auto pParser = std::make_unique<M3U8Parser>(data);
	if (!pParser->IsValid())
	{
		LOG() << "invalid m3u8";
		return CodeNo;
	}

	if (pParser->IsMaster())
	{
		info.insert("master", 1);
		pParser->GetVariantInfo(items);
	}
	else
	{
		info.insert("master", 0);
		info.insert("type", pParser->GetType());
		info.insert("targetDuration", pParser->GetTargetDuration());
		pParser->GetSegmentInfo(items);
	}

	if (CodeOK != FormatSubAddress(items, trueUrl))
	{
		LOG() << "FormatSubAddress error";
		return CodeNo;
	}

	return CodeOK;
}