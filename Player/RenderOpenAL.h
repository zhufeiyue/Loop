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
	int AppendWavData(BufPtr);

private:
	ALCdevice* m_pDevice = NULL;
	ALCcontext* m_pContext = NULL;

	ALuint m_source = 0;
	ALint m_sourceState = 0;

	ALuint m_buffer[4] = { 0 };
	std::queue<ALuint> m_bufferUnQueue;
	std::mutex m_lock;

	int m_iChannel = 0;
	int m_iBitPerSampe = 0;
	int m_iBlockAlign = 0;
	int m_iSamplePerSecond = 0;
	ALenum  m_format;
};


class RenderOpenAL : public IRender
{
public:
	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

private:
	std::unique_ptr<OpenALDevice> m_pPlayDevice;
	std::unique_ptr<FFmpegAudioConvert> m_pAudioConvert;
};