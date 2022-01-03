#pragma
#include "IDecoder.h"
#include "IRender.h"
#include "FFmpegDemuxer.h"
#include "FFmpegFilter.h"

enum class PlaySpeed
{
	Speed_0_5X,
	Speed_1X,
	Speed_1_5X,
	Speed_2X
};

double GetSpeedByEnumValue(PlaySpeed);

struct AVSyncParam
{
	IDecoder* pDecoder = nullptr;
	IRender* pVideoRender = nullptr;
	IRender* pAudioRender = nullptr;
	FilterAudio* pFilterVolume = nullptr;
	FilterAudio* pFilterSpeed = nullptr;
	PlaySpeed    playSpeed = PlaySpeed::Speed_1X;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
};

class IAVSync
{
public:
	virtual ~IAVSync() {}
	virtual int Update(AVSyncParam*) = 0;

	virtual int SetMediaInfo(Dictionary dic);
	virtual int SetUpdateInterval(int);
	virtual int Restet();
	virtual int64_t GetCurrentPosition();
protected:
	AVRational m_originVideoTimebase;
	AVRational m_originAudioTimebase;
	AVRational m_uniformTimebase;
	int        m_iUpdateInterval = 0; //ms
	int64_t    m_iCurrentPlayPosition = 0;
};

class SyncVideo : public IAVSync
{
public:
	int Update(AVSyncParam*) override;
};

class SyncAudio : public IAVSync
{
public:
	SyncAudio();
	int Update(AVSyncParam*) override;

private:
	int ProcessVolumeFilter(FilterAudio*, FrameHolderPtr&);
	int ProcessSpeedFilter(FilterAudio*, FrameHolderPtr&);

private:
	std::chrono::steady_clock::time_point m_timeLastUpdateAudio;
	FrameHolderPtr                        m_pCachedAudioFrame;
	int                                   m_iAudioUpdateInterval = 150;
};

class SyncAV : public SyncAudio
{
public:
	SyncAV();
	int Update(AVSyncParam*) override;

private:
	std::chrono::steady_clock::time_point m_timeLastSync;
	FrameHolderPtr                        m_pCachedVideoFrame;
	int                                   m_iSyncInterval = 500;
};