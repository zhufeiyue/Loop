#pragma once

#include <algorithm>
#include <vector>
#include <mutex>
#include "Dic.h"

class HlsSegment
{
public:
	enum class PreloadType
	{
		Unknown,
		Forward, //直接下发ts的网络地址
		Download //下载ts走代理
	};

	HlsSegment(Dic&);
	~HlsSegment();
	int64_t GetNo() const { return no; }
	double  GetDuration() const { return duration; }
	std::string GetURL() const;
	PreloadType GetPreloadType() const { return preloadType; }

	int PreLoad(const std::string&, PreloadType type = PreloadType::Forward);
	int UnLoad();

private:
	std::string strAddress;
	std::string strProxyAddress;
	double      duration;
	int64_t     no;
	PreloadType preloadType = PreloadType::Unknown;
};

class HlsVariant
{
public:
	friend class HlsPlaylist;

	enum class Type
	{
		Live,
		Vod,
		Event,
		Unknown
	};

	HlsVariant(Dic&);
	~HlsVariant();
	Type GetType() const;
	int Clear();
	int Update();
	int Seek(uint64_t, double& newStartPos);
	int SwitchTo(int64_t);
	int GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg, bool& isEndSeg);
	int GetCurrentSegmentNo(int64_t&);
	int GetPreloadInfo(double&, double&);

	int64_t GetBandWidth() const;
	int64_t GetTargetDuration() const;
	int64_t GetVariantIndex() const;
	double  GetDuration() const;
	std::string GetResolution() const;
	std::string GetAddress() const;

	int Prepare();

private:
	int InitPlay();
	int Append(std::vector<std::shared_ptr<HlsSegment>>&);

private:
	std::string m_strAddress;
	std::string m_strResolution;
	std::string m_strSessionId;
	std::string m_strErrorMessage;
	Type        m_variantType;
	int64_t     m_bandWidth;
	int64_t     m_targetDuration = 10;
	int64_t     m_iCurrentSegIndex = 0;
	int64_t     m_iVariantIndex = -1;

	std::chrono::steady_clock::time_point    m_timePointLastAccess;
	std::chrono::steady_clock::time_point    m_timePointLastSeek;
	std::vector<std::shared_ptr<HlsSegment>> m_segs;
};

class HlsPlaylist
{
public:
	int InitPlaylist(Dic);
	int SwitchVariant(Dic);
	int GetCurrentVariant(std::shared_ptr<HlsVariant>& pVariant);
	int GetVariantInfoList(std::vector<Dic>&);

	std::string ErrorMessage() {
		return m_strErrorMessage;
	}

protected:
	int InitDefaultVariant(std::string);

private:
	std::vector<std::shared_ptr<HlsVariant>> m_variants;
	std::shared_ptr<HlsVariant>              m_pCurrentVariant;
	std::string                              m_strErrorMessage;
};
