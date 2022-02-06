#include <common/AsioSocket.h>
#include <common/ParseUrl.h>
#include "HLSPlaylist.h"

void testHls()
{
	Eventloop loop;
	auto thread = std::thread([&loop]() {
		loop.Run();
	});

	auto pHlsPlaylist = new HlsPlaylist();
	pHlsPlaylist->SetM3U8Address("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865");
	//pHlsPlaylist->SetM3U8Address("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048");

	thread.join();
}

static std::vector<std::string> split_string(const std::string& s, char ch)
{
	std::istringstream stream(s);
	std::string strPart;
	std::vector<std::string> parts;

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

static bool is_absolute_uri(const std::string& uri)
{
	if (strnicmp(uri.c_str(), "http", 4) == 0)
	{
		return true;
	}

	return false;
}

HlsPlaylist::HlsPlaylist()
{
}

HlsPlaylist::~HlsPlaylist()
{
}

int HlsPlaylist::SetM3U8Address(std::string strAddress)
{
	m_strM3U8Address = strAddress;
	return GetM3U8(strAddress);
}

int HlsPlaylist::GetM3U8(std::string strAddress)
{
	auto pHttp = std::make_shared<HttpClient>(GetLoop());
	pHttp->Get(strAddress, 
		[this](std::string data, Dictionary info) { this->OnGetM3U8(std::move(data), std::move(info)); },
		[this](Dictionary error) { this->OnGetM3U8Error(std::move(error)); });

	return CodeOK;
}

void HlsPlaylist::OnGetM3U8(std::string data, Dictionary returnInfo)
{
	auto result = returnInfo.find("result")->second.to<int>();
	auto url = returnInfo.find("url")->second.to<std::string>();
	if (result != 200)
	{
		return;
	}

	m_pParser.reset(new M3U8Parser(data));
	if (!m_pParser->IsValid())
	{
		LOG() << "invalid m3u8 " << url;
		return;
	}

	std::vector<Dictionary> infos;
	if (m_pParser->IsMain())
	{
		m_pParser->GetSubM3U8Info(infos);
	}
	else
	{
		m_pParser->GetSegmentInfo(infos);
	}

	std::string host;
	std::string scheme;
	std::string path;
	int port(0);
	if (!ParseUrl(url, scheme, host, path, port))
	{
		LOG() << __FUNCTION__ << " parse error: " << url;
		return;
	}
	auto paths = split_string(path, '/');

	for (auto iter = infos.begin(); iter != infos.end(); ++iter)
	{
		std::string sub_url = iter->find("address")->second.to<std::string>();
		if (is_absolute_uri(sub_url))
		{
			continue;
		}

		auto main_path = paths;
		auto sub_path = split_string(sub_url, '/');

		for (auto i1 = main_path.rbegin(), i2 = sub_path.rbegin(); 
			i1 != main_path.rend() && i2 != sub_path.rend(); ++i1, ++i2)
		{
			if (*i1 != *i2)
			{
				*i1 = *i2;
			}
			else
			{
				break;
			}
		}

		auto subPath = join_string(main_path, '/');
		LOG() << subPath;
	}
}

void HlsPlaylist::OnGetM3U8Error(Dictionary errInfo)
{
	auto message = errInfo.find("message")->second.to<std::string>();
	LOG() << message;
}