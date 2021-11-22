#pragma
#include "IDecoder.h"
#include "IRender.h"
#include "FFmpegDemuxer.h"

struct AVSyncParam
{
	IDecoder* pDecoder = nullptr;
	IRender* pVideoRender = nullptr;
	IRender* pAudioRender = nullptr;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
};

class IAVSync
{
public:
	virtual ~IAVSync() {}
	virtual int Update(AVSyncParam*) = 0;

	virtual int SetMediaInfo(Dictionary dic);
	virtual int SetUpdateInterval(int);

protected:
	AVRational m_originVideoTimebase;
	AVRational m_originAudioTimebase;
	AVRational m_uniformTimebase;
	int m_iUpdateInterval = 0; //ms
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
	std::chrono::steady_clock::time_point m_timeLastUpdateAudio;
	FrameHolderPtr                        m_pCachedAudioFrame;
};

class SyncAV : public SyncAudio
{
public:
	SyncAV();
	int Update(AVSyncParam*) override;

private:
	std::chrono::steady_clock::time_point m_timeLastSync;
	FrameHolderPtr                        m_pCachedVideoFrame;
};