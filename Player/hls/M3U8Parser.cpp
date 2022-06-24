#include "M3U8Parser.h"
#include "SimpleHttpClient.h"

#include <QUrl>

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

static bool ParseUrl(const std::string& strUrl,
	std::string& scheme,
	std::string& host,
	std::string& path,
	int& port)
{
	QUrl url(QString::fromStdString(strUrl));
	if (!url.isValid())
	{
		return false;
	}
	
	scheme = url.scheme().toStdString();
	host = url.host().toStdString();
	path = url.path().toStdString();
	port = url.port();

	return true;
}

static int FormatSubAddress(std::vector<Dic>& sub_items, const std::string& m3u8_url)
{
	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	if (!ParseUrl(m3u8_url, scheme, host, path, port))
	{
		qDebug() << __FUNCTION__ << " parse url error: " << m3u8_url.c_str();
		return -1;
	}

	auto paths = split_string(path, '/');
	if (paths.empty())
	{
		qDebug() << __FUNCTION__;
		return -1;
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

		sub_url = scheme + "://" + host + (port > 0 ? ":" + std::to_string(port) : "") + sub_uri;
		iter->find("address")->second = Dic::DicHelper(sub_url);
	}
	return 0;
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

int M3U8Parser::GetVariantInfo(std::vector<Dic>& items)
{
	items.clear();

	Dic dic;
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

int M3U8Parser::GetSegmentInfo(std::vector<Dic>& items)
{
	items.clear();

	Dic dic;
	size_t pos;
	auto segNo = GetSequenceNumber();
	for (size_t i = 0; i < m_vecLines.size(); )
	{
		//#EXTINF:<duration>,[<title>]
		if (strncmp(m_vecLines[i].c_str(), "#EXTINF:", 8) != 0)
		{
			i += 1;
			continue;
		}

		size_t j = i + 1;
		for (; j < m_vecLines.size(); ++j)
		{
			if (!m_vecLines[j].empty() && m_vecLines[j].at(0) != '#')
			{
				auto temp = split_string(m_vecLines[i].c_str() + 8, ',');
				if (!temp.empty())
				{
					dic.insert("duration", atof(temp[0].c_str()));
				}
				if (temp.size() == 2)
				{
					dic.insert("title", temp[1]);
				}
				dic.insert("address", m_vecLines[j]);
				dic.insert("no", segNo++);
				items.push_back(std::move(dic));

				break;
			}
		}

		i = j + 1;
	}
	return 0;
}

int ParseM3U8(std::string strPlaylistUrl, Dic& info, std::vector<Dic>& items)
{
	Dic responData;
	int retry = 0;

	qDebug() << "parse m3u8 " << strPlaylistUrl.c_str();
again:
	SimpleGet(QString::fromStdString(strPlaylistUrl), responData, 1500);

	auto code = responData.get<int>("code");
	auto strMessage = responData.get<std::string>("message");
	if (code != 0)
	{
		qDebug() << strMessage.c_str();
		if (retry++ < 3)
			goto again;

		info.insert("message", strMessage);
		return -1;
	}

	auto httpStatusCode = responData.get<int>("httpCode");
	auto trueUrl = responData.get<std::string>("httpUrl");
	if (httpStatusCode != 200)
	{
		strMessage = "http respon code " + std::to_string(httpStatusCode);
		info.insert("message", strMessage);
		return -1;
	}

	auto iterData = responData.find("httpData");
	if (iterData == responData.end())
	{
		strMessage = "no m3u8 data";
		info.insert("message", strMessage);
		return -1;
	}
	auto data = iterData->second.to<QByteArray>().toStdString();
	auto pParser = std::make_unique<M3U8Parser>(data);
	if (!pParser->IsValid())
	{
		strMessage = "invalid m3u8";
		info.insert("message", strMessage);
		return -1;
	}

	info.insert("cdnsip", responData.get<QString>("cdnsip"));
	info.insert("cdncip", responData.get<QString>("cdncip"));
	info.insert("message", strMessage);
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
		info.insert("address", trueUrl);
		pParser->GetSegmentInfo(items);
	}

	if (0 != FormatSubAddress(items, trueUrl))
	{
		strMessage = "FormatSubAddress error";
		info.insert("message", strMessage);
		return -1;
	}

	return 0;
}