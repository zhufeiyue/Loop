#include "HLSPlaylist.h"
#include "M3U8Parser.h"

static HlsVariant::Type String2HlsVariantType(const std::string& strType)
{
	auto type = HlsVariant::Type::Unknown;
	if (strType == "LIVE")
		type = HlsVariant::Type::Live;
	else if (strType == "VOD")
		type = HlsVariant::Type::Vod;
	else if (strType == "EVENT")
		type = HlsVariant::Type::Event;

	return type;
}
static std::string HlsVariantType2String(HlsVariant::Type type)
{
	switch (type)
	{
	case HlsVariant::Type::Live:
		return "LIVE";
	case HlsVariant::Type::Vod:
		return "VOD";
	case HlsVariant::Type::Event:
		return "EVENT";
	}

	return "unknown";
}

HlsSegment::HlsSegment(Dictionary& dic)
{
	this->no = dic.get<int64_t>("no");
	this->strAddress = dic.get<std::string>("address");
	this->duration = dic.get<double>("duration");
}

int HlsSegment::Prepare()
{
	return CodeOK;
}


HlsVariant::HlsVariant(Dictionary& dic)
{
	m_variantType = String2HlsVariantType(dic.get<std::string>("type"));
	m_targetDuration = dic.get<int64_t>("targetDuration");
	m_bandWidth = dic.get<int64_t>("bandwidth");
	m_strAddress = dic.get<std::string>("address");
	m_strResolution = dic.get<std::string>("resolution");
	m_timePointLastAccess = std::chrono::steady_clock::now();
}

HlsVariant::~HlsVariant()
{
}

HlsVariant::Type HlsVariant::GetType()
{
	return m_variantType;
}

int HlsVariant::Append(std::vector<std::shared_ptr<HlsSegment>>& segs)
{
	if (segs.empty())
	{
		return CodeNo;
	}

	// 认为segs是有序的，以no排序

	if (m_segs.empty())
	{
		m_segs = std::move(segs);
		m_iCurrentSegIndex = 0;
		return CodeOK;
	}

	auto oldSegNo = m_segs.back()->GetNo();
	auto iter = std::find_if(segs.begin(), segs.end(), [oldSegNo](std::shared_ptr<HlsSegment>& p)
		{
			if (p)
				return p->GetNo() == oldSegNo;
			else
				return false;
		});

	if (iter != segs.end())
	{
		iter = iter + 1;
		for (; iter != segs.end(); ++iter)
		{
			m_segs.push_back(std::move(*iter));
		}
	}
	else
	{
		LOG() << "seg no mismatch!!!";
		auto firstNewSegNo = segs.front()->GetNo();
		auto lastNewSegNo = segs.back()->GetNo();
		if (firstNewSegNo > oldSegNo)
		{
			for (iter = segs.begin(); iter != segs.end(); ++iter)
			{
				m_segs.push_back(std::move(*iter));
			}
		}
		else if (lastNewSegNo < oldSegNo)
		{
			return CodeNo;
		}
		else
		{
			return CodeNo;
		}
	}


	return CodeOK;
}

int HlsVariant::Clear()
{
	m_segs.clear();
	m_iCurrentSegIndex = 0;
	return CodeOK;
}

int HlsVariant::Update()
{
	Dictionary info;
	std::vector<Dictionary> items;
	int ret;

#ifdef _DEBUG
	auto last = std::chrono::steady_clock::now();
	ret = ParseM3U8(m_strAddress, info, items);
	auto now = std::chrono::steady_clock::now();
	LOG() << "ParseM3U8 use " << std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() << "ms";
#else
	ret = ParseM3U8(m_strAddress, info, items);
#endif
	if (ret != CodeOK)
	{
		return ret;
	}

	auto isMaster = info.get<int>("master");
	if (isMaster)
	{
		LOG() << "shouldn't be master m3u8";
		return CodeNo;
	}

	auto targetDuration = info.get<int64_t>("targetDuration");
	if (targetDuration > m_targetDuration)
	{
		m_targetDuration = targetDuration;
	}

	m_variantType = String2HlsVariantType(info.get<std::string>("type"));

	std::vector<std::shared_ptr<HlsSegment>> segs;
	for (auto iter = items.begin(); iter != items.end(); ++iter)
	{
		segs.push_back(std::make_shared<HlsSegment>(*iter));
	}
	Append(segs);

	return CodeOK;
}

int HlsVariant::Seek(int)
{
	return CodeOK;
}

int HlsVariant::InitPlay()
{
	int ret = CodeNo;

	if (GetType() == Type::Unknown || m_segs.empty())
	{
		ret = Update();
		if (ret != CodeOK)
		{
			return ret;
		}
	}

	auto type = GetType();
	if (type == Type::Unknown || m_segs.empty())
	{
		LOG() << "cann't init play for this variant. type: "
			<< HlsVariantType2String(type)
			<< "; segment number: " << m_segs.size();
		return CodeNo;
	}

	if (type == Type::Vod)
	{
		// 点播从头开始播
		m_iCurrentSegIndex = 0;
	}
	else if(type == Type::Live || type == Type::Event)
	{
		m_iCurrentSegIndex = (int64_t)m_segs.size() - 3; // 从倒数第3个开播
		if (m_iCurrentSegIndex < 0)
		{
			m_iCurrentSegIndex = 0;
		}
	}
	else
	{
		LOG() << "fatal error " __FUNCTION__;
		return CodeNo;
	}

	m_segs[m_iCurrentSegIndex]->Prepare();

	return CodeOK;
}

int HlsVariant::GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg)
{
	if (m_variantType == Type::Live)
	{
		auto now = std::chrono::steady_clock::now();
		auto interval = now - m_timePointLastAccess;
		LOG() << "m3u8 request interval " << std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
		m_timePointLastAccess = now;
		if (interval > std::chrono::seconds(60))
		{
			Clear();
			if (CodeOK != Update())
			{
				LOG() << "update error";
				return CodeNo;
			}
		}
	}

	if (m_iCurrentSegIndex < 0 || m_iCurrentSegIndex >= (int64_t)m_segs.size())
	{
		return CodeNo;
	}

	pSeg = m_segs[m_iCurrentSegIndex];
	m_iCurrentSegIndex += 1;

	return CodeOK;
}

int64_t HlsVariant::GetTargetDuration() const
{
	return m_targetDuration;
}

int HlsVariant::Prepare()
{
	if (m_variantType == Type::Live)
	{
		if (m_iCurrentSegIndex >= (int64_t)m_segs.size() - 1)
		{
			Update();
		}
	}

	return CodeOK;
}


int HlsPlaylist::GetCurrentVariant(std::shared_ptr<HlsVariant>& pVariant)
{
	if (!m_pCurrentVariant)
	{
		return CodeNo;
	}

	pVariant = m_pCurrentVariant;

	return CodeOK;
}

int HlsPlaylist::SwitchVariant(Dictionary)
{
	return 0;
}

int HlsPlaylist::InitPlaylist(std::string strPlaylistUrl)
{
	Dictionary info;
	std::vector<Dictionary> items;

	auto ret = ParseM3U8(strPlaylistUrl, info, items);
	if (ret != CodeOK)
	{
		return ret;
	}

	auto isMaster = info.get<int>("master");
	if (isMaster)
	{
		for (auto iter = items.begin(); iter != items.end(); ++iter)
		{
			m_variants.push_back(std::make_shared<HlsVariant>(*iter));
		}
	}
	else
	{
		auto pVariant = std::make_shared<HlsVariant>(info);

		std::vector<std::shared_ptr<HlsSegment>> segs;
		for (auto iter = items.begin(); iter != items.end(); ++iter)
		{
			segs.push_back(std::make_shared<HlsSegment>(*iter));
		}
		pVariant->Append(segs);

		m_variants.push_back(std::move(pVariant));
	}

	return InitDefaultVariant();
}

int HlsPlaylist::InitDefaultVariant()
{
	if (m_variants.empty())
	{
		LOG() << "no variant";
		return CodeNo;
	}

	// todo 下面直接使用第一个variant作为默认
	m_pCurrentVariant = m_variants.front();

	return m_pCurrentVariant->InitPlay();
}

void testHls()
{
	auto pHlsPlaylist = new HlsPlaylist();
	//pHlsPlaylist->InitPlaylist("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865");
	//pHlsPlaylist->InitPlaylist("http://112.74.200.9:88/tv000000/m3u8.php?/migu/637444830");
	//pHlsPlaylist->InitPlaylist("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048");
	pHlsPlaylist->InitPlaylist("http://183.207.249.9/PLTV/3/224/3221225548/index.m3u8");

	std::this_thread::sleep_for(std::chrono::seconds(120));
}