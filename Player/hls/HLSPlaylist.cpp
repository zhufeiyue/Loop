#include <common/AsioSocket.h>
#include <common/AsioHttp.h>
#include <common/ParseUrl.h>
#include "HLSPlaylist.h"
#include "M3U8Parser.h"

void testHls()
{
	Eventloop loop;
	auto thread = std::thread([&loop]() {
		loop.Run();
		});

	auto pHlsPlaylist = new HlsPlaylist();
	pHlsPlaylist->SetM3U8Address("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865");
	//pHlsPlaylist->SetM3U8Address("http://112.74.200.9:88/tv000000/m3u8.php?/migu/637444830");
	//pHlsPlaylist->SetM3U8Address("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048");
	//pHlsPlaylist->SetM3U8Address("https://imgcdn.start.qq.com/cdn/win.client/installer/START-installer-v0.11.0.7841.exe");
	thread.join();
}

std::vector<std::string> split_string(const std::string& s, char ch)
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
#if  WIN32
	if (strnicmp(uri.c_str(), "http", 4) == 0)
#else
	if(strncasecmp(uri.c_str(), "http", 4) == 0)
#endif
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
	return GetM3U8(strAddress);
}

int HlsPlaylist::GetM3U8(std::string strAddress)
{
	auto pHttpClient = std::make_shared<AsioHttpClient>();
	pHttpClient->Get(strAddress,
		[this](std::string_view data, Dictionary info) { this->OnGetM3U8(std::string(data), std::move(info)); },
		[this](Dictionary error) { this->OnGetM3U8Error(std::move(error)); });

	return CodeOK;
}

void HlsPlaylist::OnGetM3U8(std::string data, Dictionary returnInfo)
{
	auto result = returnInfo.find("result")->second.to<int>();
	auto url = returnInfo.find("url")->second.to<std::string>();
	auto isMaster = false;
	if (result != 200)
	{
		return;
	}

	auto pParser = std::make_shared<M3U8Parser>(data);
	if (!pParser->IsValid())
	{
		LOG() << "invalid m3u8 " << url;
		return;
	}

	std::vector<Dictionary> infos;
	if (pParser->IsMaster())
	{
		pParser->GetVariantInfo(infos);
		isMaster = true;
	}
	else
	{
		pParser->GetSegmentInfo(infos);
		LOG() << pParser->GetType();
		LOG() << "sequence number: " << pParser->GetSequenceNumber();
	}

	if (CodeOK != FormatSubAddress(infos, url))
	{
		LOG() << "FormatSubAddress error";
		return;
	}

	if (isMaster)
	{
		auto sub_url = infos[0].find("address")->second.to<std::string>();
		GetM3U8(sub_url);
	}
}

void HlsPlaylist::OnGetM3U8Error(Dictionary errInfo)
{
	auto message = errInfo.find("message")->second.to<std::string>();
	LOG() << message;
}

int HlsPlaylist::FormatSubAddress(std::vector<Dictionary>& sub_items, std::string m3u8_url)
{
	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	if (!ParseUrl(m3u8_url, scheme, host, path, port))
	{
		LOG() << __FUNCTION__ << " parse url error: " << m3u8_url;
		return CodeNo;
	}
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

		sub_url = scheme + "://" +  host + (port!=0 ? ":"+std::to_string(port) : "") + sub_uri;
		iter->find("address")->second = Dictionary::DictionaryHelper(sub_url);
	}
	return CodeOK;
}