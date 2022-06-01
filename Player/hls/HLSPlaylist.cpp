#include "HLSPlaylist.h"
#include "M3U8Parser.h"

#include <QDebug>

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

HlsSegment::HlsSegment(Dic& dic)
{
	this->no = dic.get<int64_t>("no");
	this->strAddress = dic.get<std::string>("address");
	this->duration = dic.get<double>("duration");
}

int HlsSegment::Prepare()
{
	return 0;
}


HlsVariant::HlsVariant(Dic& dic)
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

HlsVariant::Type HlsVariant::GetType() const
{
	return m_variantType;
}

int HlsVariant::Append(std::vector<std::shared_ptr<HlsSegment>>& segs)
{
	if (segs.empty())
	{
		return -1;
	}

	// 认为segs是有序的，以no排序

	if (m_segs.empty())
	{
		m_segs = std::move(segs);
		m_iCurrentSegIndex = 0;
		return 0;
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
		qDebug() << "seg no mismatch!!!";
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
			return -1;
		}
		else
		{
			return -1;
		}
	}


	return 0;
}

int HlsVariant::Clear()
{
	m_segs.clear();
	m_iCurrentSegIndex = 0;
	return 0;
}

int HlsVariant::Update()
{
	Dic info;
	std::vector<Dic> items;
	int ret;

	ret = ParseM3U8(m_strAddress, info, items);
	if (ret != 0)
	{
		return ret;
	}

	auto isMaster = info.get<int>("master");
	if (isMaster)
	{
		qDebug() << "shouldn't be master m3u8";
		return -1;
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

	return 0;
}

int HlsVariant::Seek(uint64_t pos, double& newStartPos)
{
	// pos 单位毫秒
	auto duration = GetDuration();

	auto seekPos = static_cast<double>(pos) / 1000;
	if (pos < 0 || seekPos >= duration)
	{
		return -1;
	}

	double startTime = 0;
	for (size_t i = 0; i < m_segs.size(); ++i)
	{
		if (seekPos >= startTime && seekPos < startTime + m_segs[i]->GetDuration())
		{
			m_iCurrentSegIndex = (int64_t)i;
			newStartPos = startTime;
			return 0;
		}

		startTime += m_segs[i]->GetDuration();
	}

	return -1;
}

int HlsVariant::InitPlay()
{
	int ret = -1;

	if (GetType() == Type::Unknown || m_segs.empty())
	{
		ret = Update();
		if (ret != 0)
		{
			return ret;
		}
	}

	auto type = GetType();
	if (type == Type::Unknown || m_segs.empty())
	{
		qDebug() << "cann't init play for this variant. type: "
			<< HlsVariantType2String(type).c_str()
			<< "; segment number: " << m_segs.size();
		return -1;
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
		qDebug() << "fatal error " __FUNCTION__;
		return -1;
	}

	m_segs[m_iCurrentSegIndex]->Prepare();

	return 0;
}

int HlsVariant::GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg, bool& isEndSeg)
{
	if (m_variantType == Type::Live)
	{
		auto now = std::chrono::steady_clock::now();
		auto interval = now - m_timePointLastAccess;
		qDebug() << "m3u8 request interval " << std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
		m_timePointLastAccess = now;
		if (interval > std::chrono::seconds(60))
		{
			Clear();
			if (0 != Update())
			{
				qDebug() << "update error";
				return -1;
			}
		}
	}

	if (m_iCurrentSegIndex < 0 || m_iCurrentSegIndex >= (int64_t)m_segs.size())
	{
		return -1;
	}


	pSeg = m_segs[m_iCurrentSegIndex];
	m_iCurrentSegIndex += 1;
	
	
	isEndSeg = false;
	if (GetType() == HlsVariant::Type::Vod)
	{
		if (m_iCurrentSegIndex >= (int64_t)m_segs.size())
		{
			isEndSeg = true;
		}
	}

	return 0;
}

int64_t HlsVariant::GetTargetDuration() const
{
	return m_targetDuration;
}

double  HlsVariant::GetDuration() const
{
	if (GetType() != HlsVariant::Type::Vod)
	{
		return 0;
	}

	double duration = 0;
	for (auto iter = m_segs.begin(); iter != m_segs.end(); ++iter)
	{
		duration += (*iter)->GetDuration();
	}

	return duration;
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

	return 0;
}


int HlsPlaylist::GetCurrentVariant(std::shared_ptr<HlsVariant>& pVariant)
{
	if (!m_pCurrentVariant)
	{
		return -1;
	}

	pVariant = m_pCurrentVariant;

	return 0;
}

int HlsPlaylist::SwitchVariant(Dic)
{
	return 0;
}

int HlsPlaylist::InitPlaylist(std::string strPlaylistUrl)
{
	Dic info;
	std::vector<Dic> items;

	auto ret = ParseM3U8(strPlaylistUrl, info, items);
	if (ret != 0)
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
		qDebug() << "no variant";
		return -1;
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