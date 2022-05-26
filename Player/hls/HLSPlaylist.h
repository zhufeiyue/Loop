#pragma once

#include <algorithm>
#include <vector>
#include <mutex>
#include <common/Dic.h>
#include <common/log.h>

class HlsSegment
{
public:
	HlsSegment(Dictionary&);
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

	HlsVariant(Dictionary&);
	~HlsVariant();
	Type GetType();
	int Clear();
	int Update();
	int Seek(int);
	int GetCurrentSegment(std::shared_ptr<HlsSegment>& pSeg);
	int64_t GetTargetDuration() const;

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

	std::chrono::steady_clock::time_point    m_timePointLastAccess;
	std::vector<std::shared_ptr<HlsSegment>> m_segs;
};

class HlsPlaylist
{
public:
	int InitPlaylist(std::string);
	int GetCurrentVariant(std::shared_ptr<HlsVariant>& pVariant);
	int SwitchVariant(Dictionary);

protected:
	int InitDefaultVariant();

private:
	std::vector<std::shared_ptr<HlsVariant>> m_variants;
	std::shared_ptr<HlsVariant>              m_pCurrentVariant;
};

void testHls();