#include "RenderMiniAudio.h"

void testMiniAudio()
{
	//MiniAudioDevice::EnumDevice();

#ifdef _WIN32

	auto p = new MiniAudioPlayWav("d:/myworld.wav");
	p->SetVolume(1.0f);
	p->Start();

	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

#endif
}

int MiniAudioBase::EnumDevice()
{
	ma_result ret;
	ma_context context;

	ret = ma_context_init(nullptr, 0, nullptr, &context);
	if (MA_SUCCESS != ret)
	{
		LOG() << "ma_context_init " << (int)ret;
		return -1;
	}

	ma_device_info* pPlaybackInfos = nullptr;
	ma_device_info* pCaptureInfos = nullptr;
	ma_uint32 count1(0), count2(0);

	ret = ma_context_get_devices(&context, &pPlaybackInfos, &count1, &pCaptureInfos, &count2);
	if (ret != MA_SUCCESS)
	{
		LOG() << "ma_context_get_devices " << (int)ret;
		ma_context_uninit(&context);
		return -1;
	}

	for (ma_uint32 i = 0; i < count1; ++i)
	{
		auto p = pPlaybackInfos + i;
		p->id;
		p->isDefault;
		LOG() << p->name;
	}

	ma_context_uninit(&context);
	return 0;
}

void DataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	auto p = static_cast<MiniAudioBase*>(pDevice->pUserData);
	if (p)
	{
		p->AudioDataCb(pOutput, pInput, frameCount);
	}
}


MiniAudioBase::MiniAudioBase()
{
}

MiniAudioBase::~MiniAudioBase()
{
	ma_device_uninit(&m_device);
}

int MiniAudioBase::Configure(ma_uint32 sampleRate, ma_uint32 channel, ma_format format)
{
	ma_device_state state;
	state = ma_device_get_state(&m_device);

	ma_device_config config = ma_device_config_init(ma_device_type_playback);
	config.playback.channels = channel;
	config.playback.format = format;
	config.sampleRate = sampleRate;
	config.dataCallback = DataCallback;
	config.pUserData = this;

	ma_result ret;
	ret = ma_device_init(nullptr, &config, &m_device);
	if (ret != MA_SUCCESS)
	{
		LOG() << "ma_device_init " << (int)ret;
		return -1;
	}

	state = ma_device_get_state(&m_device);

	return 0;
}

int MiniAudioBase::Start()
{
	if (ma_device_get_state(&m_device) != ma_device_state_stopped)
	{
		return -1;
	}

	ma_result ret =  ma_device_start(&m_device);
	if (ret != MA_SUCCESS)
	{
		LOG() << "ma_device_start " << ret;
		return -1;
	}

	return 0;
}

int MiniAudioBase::Stop()
{
	if (ma_device_get_state(&m_device) != ma_device_state_started)
	{
		return -1;
	}

	ma_result ret = ma_device_stop(&m_device);
	if (ret != MA_SUCCESS)
	{
		LOG() << "ma_device_stop " << ret;
		return -1;
	}

	return 0;
}

int MiniAudioBase::SetVolume(float v)
{
	auto ret = ma_device_set_master_volume(&m_device, v);
	return 0;
}

int MiniAudioBase::GetVolume(float& v)
{
	auto ret = ma_device_get_master_volume(&m_device, &v);
	return 0;
}

int MiniAudioBase::AudioDataCb(void* pOutput, const void*, ma_uint32 frameCount)
{
	return 0;
}


#ifdef _WIN32
MiniAudioPlayWav::MiniAudioPlayWav(std::string strFile)
{
	m_pWavDemuxer.reset(new WavDemuxerFile(strFile));
	if (m_pWavDemuxer->ReadFormat() != CodeOK)
	{
		return;
	}

	auto& f = m_pWavDemuxer->AudioFormat();
	ma_format format;
	if (f.Format.wBitsPerSample == 16)
	{
		format = ma_format_s16;
	}
	else if (f.Format.wBitsPerSample == 16)
	{
		format = ma_format_u8;
	}
	else
	{
		// todo
		return;
	}

	if (CodeOK != Configure(f.Format.nSamplesPerSec, f.Format.nChannels, format))
	{
		return;
	}
}

int MiniAudioPlayWav::AudioDataCb(void* pOutput, const void*, ma_uint32 frameWant)
{
	uint32_t frameGot = 0;
	if (CodeOK != m_pWavDemuxer->ReadSample((uint8_t*)pOutput, frameWant, frameGot)
		|| frameGot < frameWant)
	{

		auto t = std::thread([this]() 
			{
				// cannt call ma_device_stop in callback
				Stop();
			});
		t.detach(); //detach后不能调用this_thread
	}

	return 0;
}
#endif


int MiniAudioPlay::AudioDataCb(void* pOut, const void*, ma_uint32 frameWant)
{
	if (m_funcFillData)
	{
		m_funcFillData(pOut, frameWant);
	}
	return 0;
}


int AudioDataConvert::Convert(FrameHolderPtr& frame, int& sampleGot)
{
	if (!m_pASwr)
	{
		return CodeNo;
	}

	auto pFFmpegFrame = frame->FrameData();

	int ret = swr_convert(m_pASwr,
		(uint8_t**)m_pFrame->data, m_pFrame->nb_samples,
		(const uint8_t**)(pFFmpegFrame->data), pFFmpegFrame->nb_samples);
	if (ret < 0)
	{
		return ret;
	}

	sampleGot = ret;

	return CodeOK;
}


int RenderMiniAudio::ConfigureRender(RenderInfo info)
{
	if (!info.get<int>("hasAudio", 0))
	{
		return CodeNo;
	}

	m_sampleFormat = (AVSampleFormat)info.get<int32_t>("audioFormat", -1);
	m_sampleRate = info.get<int32_t>("audioRate");
	m_sampleChannel = info.get<int32_t>("audioChannel");
	m_sampleChannelLayout = info.get<int64_t>("audioChannelLayout");

	int ret = ConfigureAudioConvert();
	if (ret != CodeOK)
	{
		return CodeNo;
	}

	m_pMiniAudio.reset(new MiniAudioPlay());
	ret = m_pMiniAudio->Configure(m_sampleRate, 2, ma_format_s16);
	if (ret != CodeOK)
	{
		return CodeNo;
	}

	m_pMiniAudio->SetFuncFillData([this](void* pOut, uint32_t frameWant) 
		{
			RenderData(pOut, frameWant);
		});
	m_pMiniAudio->SetVolume(1.0f);
	m_pMiniAudio->Start();

	return CodeOK;
}

int RenderMiniAudio::ConfigureAudioConvert()
{
	m_pAudioConvert.reset();
	if (m_sampleChannel != 2 || m_sampleFormat != AV_SAMPLE_FMT_S16)
	{
		m_pAudioConvert.reset(new AudioDataConvert());
		int ret = m_pAudioConvert->Configure(
			m_sampleRate, m_sampleChannelLayout, m_sampleFormat,
			m_sampleRate, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16,
			m_sampleRate / 2);
		if (ret != CodeOK)
		{
			m_pAudioConvert.reset();
			return CodeNo;
		}
	}
	return CodeOK;
}

int RenderMiniAudio::RenderData(void* pOut, uint32_t frameWant)
{
	uint32_t frameGot = 0;
	int ret = 0;

again:
	if (!m_pCurrentFrame)
	{
		std::lock_guard<std::mutex> guard(m_audioDataLock);
		if (m_audioDatas.empty())
		{
			return 0;
		}
		m_pCurrentFrame = m_audioDatas.front();
		m_audioDatas.pop();

		auto pFrame = m_pCurrentFrame->FrameData();
		m_audioDataSampleCount -= pFrame->nb_samples;

		if (pFrame->format != m_sampleFormat ||
			pFrame->channel_layout != m_sampleChannelLayout ||
			pFrame->sample_rate != m_sampleRate ||
			m_pCurrentFrame->Speed() != m_sampleSpeed)
		{
			m_sampleFormat = (AVSampleFormat)pFrame->format;
			m_sampleChannelLayout = pFrame->channel_layout;
			m_sampleChannel = pFrame->channels;
			m_sampleRate = pFrame->sample_rate;
			m_sampleSpeed = m_pCurrentFrame->Speed();

			ret = ConfigureAudioConvert();
			if (ret != CodeOK)
			{
				m_sampleFormat = AV_SAMPLE_FMT_NONE;
				return -1;
			}
		}

		if (m_pAudioConvert)
		{
			if (CodeOK != m_pAudioConvert->Convert(m_pCurrentFrame, m_iCurrentFrameSampleCount))
				return -1;
			m_iCurrentFrameOffset = 0;
			m_iCurrentFrameRenderTime = m_pCurrentFrame->UniformPTS();
			m_pCurrentFrameDataPtr = m_pAudioConvert->Frame()->data[0];
		}
		else
		{
			m_iCurrentFrameSampleCount = pFrame->nb_samples;
			m_iCurrentFrameOffset = 0;
			m_iCurrentFrameRenderTime = m_pCurrentFrame->UniformPTS();
			m_pCurrentFrameDataPtr = pFrame->data[0];
		}
	}

	int sampleUseableInCurrentFrame = m_iCurrentFrameSampleCount - m_iCurrentFrameOffset;
	auto bytePerSample = 4;
	auto n = (std::min)(frameWant - frameGot, (uint32_t)sampleUseableInCurrentFrame);

	memcpy((uint8_t*)pOut + frameGot * bytePerSample, m_pCurrentFrameDataPtr + m_iCurrentFrameOffset * bytePerSample, n * bytePerSample);
	frameGot += n;
	m_iCurrentFrameOffset += n;
	if (m_iCurrentFrameOffset >= m_iCurrentFrameSampleCount)
	{
		m_pCurrentFrame.reset();
	}

	if (frameGot < frameWant)
	{
		goto again;
	}

	return 0;
}

int RenderMiniAudio::UpdataFrame(FrameHolderPtr data)
{
	std::lock_guard<std::mutex> guard(m_audioDataLock);
	
	if (data)
	{
		m_audioDataSampleCount += data->FrameData()->nb_samples;
		m_audioDatas.push(std::move(data));
	}

	if (m_audioDataSampleCount < m_sampleRate / 5)
	{
		return CodeAgain;
	}

	return CodeOK;
}

int RenderMiniAudio::Start()
{
	if (m_pMiniAudio)
	{
		m_pMiniAudio->Start();
	}
	return 0;
}

int RenderMiniAudio::Stop()
{
	if (m_pMiniAudio)
	{
		m_pMiniAudio->Stop();
	}
	return 0;
}

int RenderMiniAudio::Pause(bool pause)
{
	m_bPause = pause;
	if (pause)
	{
		Stop();
	}
	else
	{
		Start();
	}
	return 0;
}

int RenderMiniAudio::Reset()
{
	std::lock_guard<std::mutex> guard(m_audioDataLock);

	m_pCurrentFrame.reset();
	m_audioDatas = decltype(m_audioDatas)();
	m_audioDataSampleCount = 0;
	return 0;
}

int RenderMiniAudio::GetRenderTime(int64_t& t)
{
	t = m_iCurrentFrameRenderTime;
	return 0;
}

int RenderMiniAudio::GetVolume(int& v)
{
	float f = 1.0f;
	if (m_pMiniAudio)
	{
		m_pMiniAudio->GetVolume(f);
	}

	v = static_cast<int>(f * 100);

	return 0;
}

int RenderMiniAudio::SetVolume(int v)
{
	if (v < 0)
		v = 0;
	if (v > 100)
		v = 100;
	
	if (m_pMiniAudio)
	{
		m_pMiniAudio->SetVolume(v / 100.f);
	}

	return 0;
}

int RenderMiniAudio::Flush()
{
	LOG() << "unnecessary";
	return 0;
}

int RenderMiniAudio::GetUseableDuration(int32_t& d)
{
	d = static_cast<int32_t>(1000.0 * m_audioDataSampleCount / m_sampleRate);
	return 0;
}

IAudioRender* CreateMiniAudioRender()
{
	return new RenderMiniAudio();
}