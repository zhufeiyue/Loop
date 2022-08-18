#pragma once

#include <string>
#include <mutex>
#include <queue>

#include <al.h>
#include <alc.h>
#include <common/bufpool.h>

#include "IRender.h"
#include "FFmpegDemuxer.h"

#if _WIN32
#include "WavDemuxer.h"
void testPlayWav();
#endif

class OpenALDevice
{
	struct BufInfo
	{
		ALuint id;
		uint32_t size;
		uint32_t sampleCount;
		uint32_t duration;
		int32_t speed;
		int64_t pts;
	};

public:
	OpenALDevice(std::string);
	~OpenALDevice();

	int Configure(int bitPerSample, int samplePerSecond, int channel, int channelMask);
	int Play();
	int Stop();
	int Pause(bool);
	int Reset();
	int SetVolume(float v);
	int GetVolume(float& v);
	int GetPlayPosition(int64_t&);
	int GetUseableDuration(int32_t&);
	int AppendWavData(BufPtr, int64_t pts = 0, int32_t speed = 0);

private:
	int UnqueueBuffer();

private:
	ALCcontext* m_pContext = nullptr;

	ALuint m_source = 0;
	// 记录source应该处于什么状态，而非实际状态
	ALint m_sourceState = 0;

	ALuint m_buffer[6] = { 0 };
	std::queue<ALuint> m_bufferUnQueue;
	std::deque<BufInfo> m_bufferQueue;
	int64_t m_iBufPts         = 0;
	int32_t m_iBufSpeed       = 1;
	size_t  m_iBufSize        = 0;
	size_t  m_iBufSampleCount = 0;
	std::mutex m_lock;

	int m_iChannel = 0;
	int m_iBitPerSampe = 0;
	int m_iBlockAlign = 0;
	int m_iSamplePerSecond = 0;
	ALenum  m_format;
};

class AudioDataConvertBatch : public FFmpegAudioConvert
{
public:
	int Convert(std::vector<FrameHolderPtr>& frames, int& sampleCountOut);
private:
	int Convert(const uint8_t**, int) { return 0; }
};

class RenderOpenAL : public IAudioRender
{
public:
	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Stop() override;
	int Pause(bool) override;
	int Reset() override;
	int GetRenderTime(int64_t&) override;
	int GetVolume(int&) override;
	int SetVolume(int) override;
	int Flush() override;
	// 返回已提交给OpenAL的数据的，可播放时长，单位毫秒
	int GetUseableDuration(int32_t&) override;

protected:
	int AppendOpenALData();

private:
	std::unique_ptr<OpenALDevice>          m_pPlayDevice;
	std::unique_ptr<AudioDataConvertBatch> m_pAudioConvert;

	AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
	int32_t        m_sampleSpeed = 1; //Speed_1X
	int32_t        m_sampleRate;
	int32_t        m_sampleChannel;
	int64_t        m_sampleChannelLayout;

	std::vector<FrameHolderPtr> m_audioFrames;
	int                         m_audioSampleCount = 0;
	BufPtr                      m_pAudioData;
};