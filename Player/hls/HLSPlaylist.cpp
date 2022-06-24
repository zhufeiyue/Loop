#include "HLSPlaylist.h"
#include "M3U8Parser.h"
#include "Monitor.h"

#include <QDebug>
#include <QTime>

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

#define SupportDown
#ifdef SupportDown
int StartTsProxy(const std::string&, const std::string&, double, std::string&);
int StopTsProxy(const std::string&);
#else
static int StartTsProxy(
	const std::string& strOrigin, 
	const std::string& strSessionId, 
	double tsDuration,
	std::string& strProxy)
{
	strProxy = strOrigin;
	return 0;
}
static int StopTsProxy(const std::string& strProxy)
{
	return 0;
}
#endif

HlsSegment::HlsSegment(Dic& dic)
{
	this->no = dic.get<int64_t>("no");
	this->strAddress = dic.get<std::string>("address");
	this->duration = dic.get<double>("duration");
}

HlsSegment::~HlsSegment()
{
	UnLoad();
}

std::string HlsSegment::GetURL() const
{
	if (preloadType == PreloadType::Download)
	{
		if (!strProxyAddress.empty())
		{
			return strProxyAddress;
		}
	}
	return strAddress;
}

int HlsSegment::PreLoad(const std::string& strSessionId, HlsSegment::PreloadType type)
{
	if (preloadType != PreloadType::Unknown)
	{
		qDebug() << "preload type is sure";
		return -1;
	}

	preloadType = type;
	if (preloadType == PreloadType::Forward)
	{
	}
	else if (preloadType == PreloadType::Download)
	{
		if (GetDuration() < 3)
		{
			return 0;
		}

		if (0 != StartTsProxy(strAddress, strSessionId, GetDuration(), strProxyAddress))
		{
			strProxyAddress.clear();
		}
	}
	else if (preloadType == PreloadType::Unknown)
	{
	}

	return 0;
}

int HlsSegment::UnLoad()
{
	if (preloadType == PreloadType::Download)
	{
		if (!strProxyAddress.empty())
		{
			StopTsProxy(strProxyAddress);
			strProxyAddress.clear();
		}
	}

	preloadType = PreloadType::Unknown;

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
	m_timePointLastSeek = m_timePointLastAccess;
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
		m_strErrorMessage = info.get<std::string>("message");
		return ret;
	}

	RecordCDNInfo(m_strSessionId, info.get<std::string>("cdnsip"), info.get<std::string>("cdncip"));

	auto isMaster = info.get<int>("master");
	if (isMaster)
	{
		m_strErrorMessage = "shouldn't be master m3u8";
		qDebug() << m_strErrorMessage.c_str();
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
	if (GetType() != Type::Vod)
	{
		return -1;
	}

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
			m_timePointLastSeek = std::chrono::steady_clock::now();
			m_iCurrentSegIndex = (int64_t)i;
			newStartPos = startTime;
			return 0;
		}

		startTime += m_segs[i]->GetDuration();
	}

	return -1;
}

int HlsVariant::SwitchTo(int64_t segNo)
{
	Clear();
	if (0 != InitPlay())
	{
		return -1;
	}

	for (size_t i = 0; i < m_segs.size(); ++i)
	{
		if (m_segs[i]->GetNo() == segNo)
		{
			m_iCurrentSegIndex = (int64_t)i;
			return 0;
		}
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
		m_strErrorMessage = "m3u8 type is unknown";
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

	m_segs[m_iCurrentSegIndex]->PreLoad(m_strSessionId);
	m_timePointLastAccess = std::chrono::steady_clock::now();
	return 0;
}

int HlsVariant::GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg, bool& isEndSeg)
{
	auto now = std::chrono::steady_clock::now();
	auto interval = now - m_timePointLastAccess;
	qDebug() << "m3u8 request interval " << std::chrono::duration_cast<std::chrono::milliseconds>(interval).count();
	m_timePointLastAccess = now;

	if (GetType() == Type::Live)
	{
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

int HlsVariant::GetCurrentSegmentNo(int64_t& segNo)
{
	if (m_iCurrentSegIndex < 0 || m_iCurrentSegIndex >= (int64_t)m_segs.size())
	{
		return -1;
	}

	segNo = m_segs[m_iCurrentSegIndex]->GetNo();

	return 0;
}

int HlsVariant::GetPreloadInfo(double& startTime, double& duration)
{
	if (m_iCurrentSegIndex < 0 || m_iCurrentSegIndex >= (int64_t)m_segs.size())
	{
		return -1;
	}

	duration = m_segs[m_iCurrentSegIndex]->GetDuration();
	
	double s = 0;
	for (int64_t i = 0; i < m_iCurrentSegIndex; ++i)
	{
		s += m_segs[i]->GetDuration();
	}
	startTime = s;

	return 0;
}

int64_t HlsVariant::GetBandWidth() const
{
	return m_bandWidth;
}

int64_t HlsVariant::GetTargetDuration() const
{
	return m_targetDuration;
}

int64_t HlsVariant::GetVariantIndex() const
{
	return m_iVariantIndex;
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

std::string HlsVariant::GetResolution() const
{
	return m_strResolution;
}

std::string HlsVariant::GetAddress() const
{
	return m_strAddress;
}

int HlsVariant::Prepare()
{
	int64_t index = m_iCurrentSegIndex - 3;
	for (int64_t i = 0; i < m_iCurrentSegIndex - 5; ++i)
	{
		m_segs[i]->UnLoad();
	}

	if (m_variantType == Type::Live)
	{
		if (m_segs.size() > 20)
		{
			m_segs.erase(m_segs.begin());
			m_iCurrentSegIndex -= 1;
		}

		if (m_iCurrentSegIndex >= (int64_t)m_segs.size() - 2)
		{
			Update();
		}
	}
	else if (m_variantType == Type::Vod)
	{
	}

	// 准备下一个segment
	index = m_iCurrentSegIndex;
	if (index >= 0 && index < (int64_t)m_segs.size())
	{
		HlsSegment::PreloadType preloadType = HlsSegment::PreloadType::Download;
		if (GetType() == Type::Vod)
		{
			auto now = std::chrono::steady_clock::now();
			if (now - m_timePointLastSeek <= std::chrono::seconds(10))
			{
				preloadType = HlsSegment::PreloadType::Forward;
			}
		}

		m_segs[index]->PreLoad(m_strSessionId, preloadType);
	}
	else
	{
		qDebug() << "dont know what to prepare";
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

int HlsPlaylist::GetVariantInfoList(std::vector<Dic>& lists)
{
	lists.clear();

	for (size_t i = 0; i < m_variants.size(); ++i)
	{
		Dic dic;
		dic.insert("bandwidth", m_variants[i]->GetBandWidth());
		dic.insert("resolution", m_variants[i]->GetResolution());
		dic.insert("index", m_variants[i]->GetVariantIndex());

		lists.emplace_back(std::move(dic));
	}

	return 0;
}

int HlsPlaylist::SwitchVariant(Dic dic)
{
	auto iter = dic.find("newVariantIndex");
	int newVariantIndex = iter->second.to<int>(-1);

	if (newVariantIndex < 0 || newVariantIndex >= (int)m_variants.size())
	{
		return -1;;
	}

	if (!m_pCurrentVariant)
	{
		return -1;
	}

	if (newVariantIndex == m_pCurrentVariant->GetVariantIndex())
	{
		return 0;
	}

	int64_t segNo = 0;
	if (0 != m_pCurrentVariant->GetCurrentSegmentNo(segNo))
	{
		return -1;
	}

	auto newVariant = m_variants[newVariantIndex];
	if (0 != newVariant->SwitchTo(segNo))
	{
		return -1;
	}

	m_pCurrentVariant = newVariant;
	return 0;
}

int HlsPlaylist::InitPlaylist(Dic dic)
{
	int ret;
	Dic info;
	std::string strHlsAddress;
	std::string strDefaultVariant;
	std::string strSessionId;
	std::vector<Dic> items;

	strHlsAddress = dic.get<std::string>("address");
	strDefaultVariant = dic.get<std::string>("defaultVariant");
	strSessionId = dic.get<std::string>("sessionId");

	ret = ParseM3U8(strHlsAddress, info, items);
	if (ret != 0)
	{
		std::string strMessage = info.get<std::string>("message");
		qDebug() << strMessage.c_str();

		m_strErrorMessage = std::move(strMessage);
		return ret;
	}

	RecordCDNInfo(strSessionId, info.get<std::string>("cdnsip"), info.get<std::string>("cdncip"));

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

	for (size_t i = 0; i < m_variants.size(); ++i)
	{
		m_variants[i]->m_strSessionId = strSessionId;
	}

	return InitDefaultVariant(std::move(strDefaultVariant));
}

int HlsPlaylist::InitDefaultVariant(std::string strDefaultVariant)
{
	// by bandwidth
	int bitrate = atoi(strDefaultVariant.c_str());
	if (bitrate == 0)
	{
		if (!m_variants.empty())
			bitrate = (int)m_variants.back()->GetBandWidth();
	}

	std::sort(m_variants.begin(), m_variants.end(),
		[](std::shared_ptr<HlsVariant>& l, std::shared_ptr<HlsVariant>& r)
		{
			return l->GetBandWidth() < r->GetBandWidth();
		});

	auto iter = std::min_element(m_variants.begin(), m_variants.end(),
		[bitrate](std::shared_ptr<HlsVariant>& l, std::shared_ptr<HlsVariant>& r)
		{
			auto n1 = std::abs(l->GetBandWidth() - bitrate);
			auto n2 = std::abs(r->GetBandWidth() - bitrate);

			if (n1 < n2)
			{
				return true;
			}
			else if (n1 == n2)
			{
				return l->GetBandWidth() < r->GetBandWidth();
			}
			else
			{
				return false;
			}
		});

	if (iter != m_variants.end())
	{
		m_pCurrentVariant = *iter;
	}

	for (size_t i = 0; i < m_variants.size(); ++i)
	{
		m_variants[i]->m_iVariantIndex = (int64_t)i;
	}

	int ret = -1;
	if (m_pCurrentVariant)
	{
		ret = m_pCurrentVariant->InitPlay();
		if (ret != 0)
		{
			m_strErrorMessage = m_pCurrentVariant->m_strErrorMessage;
		}
	}
	return ret;
}
