#pragma once

#include <string>
#include <mutex>
#include <queue>

#include <al.h>
#include <alc.h>
#include <common/bufpool.h>

#include "IRender.h"
#include "WavDemuxer.h"
#include "FFmpegDemuxer.h"

void testPlayWav();

class OpenALDevice
{
public:
	OpenALDevice(std::string);
	~OpenALDevice();

	int Configure(int bitPerSample, int samplePerSecond, int channel, int channelMask);
	int Play();
	int Pause();
	int Stop();
	int SetVolume(float v);
	int GetVolume(float& v);
	int GetPlayPts(int64_t&);
	int AppendWavData(BufPtr, int64_t pts = 0);

private:
	int UnqueueBuffer();

private:
	ALCdevice* m_pDevice = NULL;
	ALCcontext* m_pContext = NULL;

	ALuint m_source = 0;
	ALint m_sourceState = 0;

	ALuint m_buffer[4] = { 0 };
	std::queue<ALuint> m_bufferUnQueue;
	std::deque<std::tuple<ALuint, int64_t, uint32_t>> m_bufferQueue;
	int64_t m_iBufPts         = 0;
	size_t  m_iBufSize        = 0;
	size_t  m_iBufSampleCount = 0;
	std::mutex m_lock;

	int m_iChannel = 0;
	int m_iBitPerSampe = 0;
	int m_iBlockAlign = 0;
	int m_iSamplePerSecond = 0;
	ALenum  m_format;
};

class AudioDataCacheConvert : public FFmpegAudioConvert
{
public:
	int Convert(const uint8_t** ppInData, int incount, int64_t inPts, int64_t& outPts, bool&bReject);

protected:
	int64_t m_iPts = -1;
};

class RenderOpenAL : public IRender
{
public:
	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Pause() override;
	int Stop() override;
	int Seek(int64_t) override;
	int GetRenderTime(int64_t&) override;

protected:
	int AppendOpenALData();

private:
	std::unique_ptr<OpenALDevice> m_pPlayDevice;
	std::unique_ptr<AudioDataCacheConvert> m_pAudioConvert;

	BufPtr m_pAudioData;
	int64_t m_iAudioDataPts = 0;
	bool m_bAudioDataValid = false;
};