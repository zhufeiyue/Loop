#pragma once

#include <algorithm>
#include <vector>
#include <mutex>
#include "Dic.h"

class HlsSegment
{
public:
	HlsSegment(Dic&);
	int64_t GetNo() const { return no; }
	double  GetDuration() const { return duration; }
	std::string GetURL() const { return strAddress; }

	int Prepare();

private:
	std::string strAddress;
	double      duration;
	int64_t     no;
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
	int GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg, bool& isEndSeg);
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
	Type        m_variantType;
	int64_t     m_bandWidth;
	int64_t     m_targetDuration = 10;
	int64_t     m_iCurrentSegIndex = 0;
	int64_t     m_iVariantIndex = -1;

	std::chrono::steady_clock::time_point    m_timePointLastAccess;
	std::vector<std::shared_ptr<HlsSegment>> m_segs;
};

class HlsPlaylist
{
public:
	int InitPlaylist(Dic);
	int SwitchVariant(Dic);
	int GetCurrentVariant(std::shared_ptr<HlsVariant>& pVariant);
	int GetVariantInfoList(std::vector<Dic>&);

protected:
	int InitDefaultVariant(std::string);

private:
	std::vector<std::shared_ptr<HlsVariant>> m_variants;
	std::shared_ptr<HlsVariant>              m_pCurrentVariant;
};
