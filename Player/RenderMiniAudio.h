#pragma once
#ifdef __APPLE__
	#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#include "miniaudio.h"
#include <string>
#include <memory>
#include <thread>

#include <common/log.h>
#include <common/bufpool.h>
#include "IRender.h"
#include "FFmpegDemuxer.h"

class MiniAudioBase
{
public:
	static int EnumDevice();

	friend void DataCallback(ma_device*, void*, const void*, ma_uint32);

	MiniAudioBase();
	virtual ~MiniAudioBase();
	virtual int Configure(ma_uint32 sampleRate, ma_uint32 channel, ma_format format);

	int Start();
	int Stop();
	int SetVolume(float);
	int GetVolume(float&);

protected:
	virtual int AudioDataCb(void*, const void*, ma_uint32);

protected:
	ma_device m_device;
};

#ifdef _WIN32
#include "WavDemuxer.h"
class MiniAudioPlayWav : public MiniAudioBase
{
public:
	MiniAudioPlayWav(std::string);

protected:
	int AudioDataCb(void*, const void*, ma_uint32) override;

protected:
	std::unique_ptr<WavDemuxerFile> m_pWavDemuxer;
};
#endif

class MiniAudioPlay : public MiniAudioBase
{
public:
	typedef std::function<void(void*, uint32_t)> FuncFillAudioData;

	void SetFuncFillData(FuncFillAudioData func)
	{
		m_funcFillData = std::move(func);
	}

protected:
	int AudioDataCb(void*, const void*, ma_uint32) override;

protected:
	FuncFillAudioData m_funcFillData;
};

class AudioDataConvert : public FFmpegAudioConvert
{
public:
	int Convert(FrameHolderPtr& frame, int& sampleCountOut);
private:
	int Convert(const uint8_t**, int) { return 0; }
};

class RenderMiniAudio : public IAudioRender
{
public:
	int ConfigureAudioConvert();
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
	int GetUseableDuration(int32_t&) override;

protected:
	int RenderData(void*, uint32_t);

protected:
	ma_format                         m_miniAudioFormat;
	uint32_t                          m_miniAudioChannel;
	uint32_t                          m_miniAudioBytePerSample;
	std::unique_ptr<MiniAudioPlay>    m_pMiniAudio;
	std::unique_ptr<AudioDataConvert> m_pAudioConvert;

	AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
	int32_t        m_sampleSpeed = 1; //Speed_1X
	int32_t        m_sampleRate;
	int32_t        m_sampleChannel;
	int64_t        m_sampleChannelLayout;

private:
	std::mutex                 m_audioDataLock;
	std::queue<FrameHolderPtr> m_audioDatas;
protected:
	std::atomic<int32_t>       m_audioDataSampleCount = 0;
	int32_t        m_iCurrentFrameSampleCount = 0;
	int32_t        m_iCurrentFrameOffset = 0;
	int64_t        m_iCurrentFrameRenderTime = 0;
	FrameHolderPtr m_pCurrentFrame;
	const uint8_t* m_pCurrentFrameDataPtr = nullptr;
	bool           m_bPause = false;
};

class RenderMiniAudio1 : public RenderMiniAudio
{
public:
	struct AudioRenderData
	{
		int32_t sampleCount;
		int64_t sampleRenderTime;
		BufPtr buf;
	};

	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;
	int Reset() override;

protected:
	int RenderData(void*, uint32_t);

protected:
	BufPool<1024 * 4, 1024 * 8> m_audioDataBufPool;
	ObjectPool<AudioRenderData> m_dataWaitRender;
	ObjectPool<AudioRenderData> m_dataWaitFill;
	std::unique_ptr<AudioRenderData> m_pCurrentRenderData;
};