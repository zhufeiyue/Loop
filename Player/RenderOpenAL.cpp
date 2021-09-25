#include "RenderOpenAL.h"

OpenALDevice::OpenALDevice(std::string strDeviceName)
{
	m_pDevice = alcOpenDevice(strDeviceName.empty() ? NULL : strDeviceName.c_str());
	if (!m_pDevice)
	{
		LOG() << "alcOpenDevice " << alGetError();
		return;
	}

	m_pContext = alcCreateContext(m_pDevice, NULL);
	if (!m_pContext)
	{
		LOG() << "alcCreateContext " << alcGetError(m_pDevice);
		return;
	}

	if (!alcMakeContextCurrent(m_pContext))
	{
		LOG() << "alcMakeContextCurrent " << alcGetError(m_pDevice);
		alcDestroyContext(m_pContext);
		m_pContext = NULL;
		return;
	}

	alGenSources(1, &m_source);
	m_sourceState = AL_INITIAL;

	int bufferCount = sizeof(m_buffer) / sizeof(m_buffer[0]);
	alGenBuffers(bufferCount, m_buffer);
	for (int i = 0; i < bufferCount; ++i)
	{
		m_bufferUnQueue.push(m_buffer[i]);
	}
}

OpenALDevice::~OpenALDevice()
{
	if (m_pContext)
	{
		alSourceStop(m_source);
		alDeleteSources(1, &m_source);
		alDeleteBuffers(sizeof(m_buffer) / sizeof(m_buffer[0]), m_buffer);

		alcMakeContextCurrent(NULL);
		alcDestroyContext(m_pContext);
		m_pContext = NULL;
	}

	if (m_pDevice)
	{
		alcCloseDevice(m_pDevice);
		m_pDevice = NULL;
	}
}

int OpenALDevice::Configure(int bitPerSample, int samplePerSecond, int channel, int channelMask)
{
	m_iChannel = channel;
	m_iBitPerSampe = bitPerSample;
	m_iSamplePerSecond = samplePerSecond;
	m_iBlockAlign = channel * bitPerSample / 8;

	if (channel == 1)
	{
		switch (bitPerSample)
		{
		case 4:
			m_format = alGetEnumValue("AL_FORMAT_MONO_IMA4");
			break;
		case 8:
			m_format = alGetEnumValue("AL_FORMAT_MONO8");
			break;
		case 16:
			m_format = alGetEnumValue("AL_FORMAT_MONO16");
			break;
		default:
			return CodeNo;
			break;
		}
	}
	else if (channel == 2)
	{
		switch (bitPerSample)
		{
		case 4:
			m_format = alGetEnumValue("AL_FORMAT_STEREO_IMA4");
			break;
		case 8:
			m_format = alGetEnumValue("AL_FORMAT_STEREO8");
			break;
		case 16:
			m_format = alGetEnumValue("AL_FORMAT_STEREO16"); // AL_FORMAT_REAR16 todo
			break;
		default:
			return CodeNo;
			break;
		}
	}
	else if (channel == 4 && bitPerSample == 16)
	{
		m_format = alGetEnumValue("AL_FORMAT_QUAD16");
	}
	else if (channel == 6 && bitPerSample == 16)
	{
		m_format = alGetEnumValue("AL_FORMAT_51CHN16");
	}
	else if (channel == 7 && bitPerSample == 16)
	{
		m_format = alGetEnumValue("AL_FORMAT_61CHN16");
	}
	else if (channel == 8 && bitPerSample == 16)
	{
		m_format = alGetEnumValue("AL_FORMAT_71CHN16");
	}
	else
	{
		LOG() <<  __FUNCTION__ <<  " invalid audio param";
		return CodeNo;
	}

	return CodeOK;
}

int OpenALDevice::Play()
{
	if (m_source != 0)
	{
		alSourcePlay(m_source);
		alGetSourcei(m_source, AL_SOURCE_STATE, &m_sourceState);
		if (m_sourceState != AL_PLAYING)
		{
			LOG() << __FUNCTION__ << ' ' << alGetError();
			return CodeNo;
		}
	}

	return CodeOK;
}

int OpenALDevice::Pause()
{
	if (m_source != 0)
	{
		alSourcePause(m_source);
		alGetSourcei(m_source, AL_SOURCE_STATE, &m_sourceState);
	}

	return CodeOK;
}

int OpenALDevice::Stop()
{
	if (m_source != 0)
	{
		alSourceStop(m_source);
		alGetSourcei(m_source, AL_SOURCE_STATE, &m_sourceState);
	}

	return CodeOK;
}

int OpenALDevice::AppendWavData(BufPtr data)
{
	if (!m_pContext)
	{
		return CodeNo;
	}

	if (!data || (data->PlayloadSize() % m_iBlockAlign) != 0)
	{
		return CodeNo;
	}

	std::lock_guard<std::mutex> guard(m_lock);
	ALint iBuffersProcessed = 0;
	ALuint uiBuffer = 0;

	iBuffersProcessed = 0;
	alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
	if (iBuffersProcessed)
	{
		ALuint temp[16] = { 0 };
		alSourceUnqueueBuffers(m_source, iBuffersProcessed, temp);
		for (auto i = 0; i < iBuffersProcessed; ++i)
		{
			m_bufferUnQueue.push(temp[i]);
		}
	}

	if (m_bufferUnQueue.empty())
	{
		return CodeAgain;
	}

	bool bAutoRePlay = false;
	// 判断是否因buffer用尽，进入的暂停状态
	if (m_sourceState == AL_PLAYING) // 本应处于播放状态
	{
		ALint iState;
		auto bufferCount = sizeof(m_buffer) / sizeof(m_buffer[0]); // 总的buffer的数量
		if (m_bufferUnQueue.size() >= bufferCount-1)
		{
			LOG() << "check source state";
			alGetSourcei(m_source, AL_SOURCE_STATE, &iState);
			if (iState == AL_STOPPED)
			{
				bAutoRePlay = true;
			}
		}
	}

	// 获得一个未使用的buffer，绑定数据后，加入source中
	uiBuffer = m_bufferUnQueue.front();
	m_bufferUnQueue.pop();
	alBufferData(uiBuffer, m_format, data->DataConst(), data->PlayloadSize(), m_iSamplePerSecond);
	alSourceQueueBuffers(m_source, 1, &uiBuffer);

	if (bAutoRePlay) 
	{
		Play();
	}

	return CodeOK;
}


int RenderOpenAL::ConfigureRender(RenderInfo info)
{
	int res = CodeNo;
	auto iter = info.find("hasAudio");
	if (iter == info.end() || !iter->second.to<int>())
	{
		return CodeNo;
	}

	iter = info.find("audioformat");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto audioFormat = (AVSampleFormat)iter->second.to<int32_t>();

	iter = info.find("audiorate");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto audioRate = iter->second.to<int32_t>();

	iter = info.find("audiochannel");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto channel = iter->second.to<int>();

	iter = info.find("audiochannelLayout");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto channelLayout = iter->second.to<int64_t>();

	if (audioFormat != AV_SAMPLE_FMT_S16 || channel != 2)
	{
		m_pAudioConvert.reset(new FFmpegAudioConvert());
		res = m_pAudioConvert->Configure(
			audioRate, channelLayout, audioFormat, 
			audioRate, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16, 
			audioRate);
		if (res != CodeOK)
		{
			return CodeNo;
		}
	}

	m_pPlayDevice.reset(new OpenALDevice(""));
	if (CodeOK != m_pPlayDevice->Configure(16, audioRate, 2, 0))
	{
		return CodeNo;
	}

	return CodeOK;
}

int RenderOpenAL::UpdataFrame(FrameHolderPtr data)
{
	return CodeOK;
}


void testPlayWav()
{
	static BufPool<48000, 96000> bufPool;

	auto pWavFile = new WavDemuxerFile("d:/myworld.wav");
	pWavFile->ReadFormat();
	auto format = pWavFile->AudioFormat();

	auto pOpenALDevice = new OpenALDevice("");
	pOpenALDevice->Configure( format.Format.wBitsPerSample, 
		format.Format.nSamplesPerSec, 
		format.Format.nChannels,
		format.dwChannelMask);

	BufPtr wavBuf;
	std::thread thread;

	auto funcAppendStream = [=, &wavBuf](uint32_t frameCount) {
		uint32_t frameGot = 0;
		uint32_t wavDataSize = format.Format.nBlockAlign * frameCount;
		int result;

		if (!wavBuf)
		{
			wavBuf = bufPool.AllocAutoFreeBuf(wavDataSize);
			if (!wavBuf)
			{
				LOG() << "alloc buf failed";
				return CodeNo;
			}

			result = pWavFile->ReadSample(wavBuf->Data(), frameCount, frameGot);
			if (result != CodeOK || frameGot == 0)
			{
				return CodeNo;
			}

			wavBuf->PlayloadSize() = frameGot * format.Format.nBlockAlign;
		}

		result = pOpenALDevice->AppendWavData(wavBuf);
		if (result == CodeOK)
		{
			wavBuf.reset();
		}
		else if (result == CodeAgain)
		{
		}
		else
		{
			wavBuf.reset();
			return CodeNo;
		}

		return CodeOK;
	};

	if (CodeOK != funcAppendStream(format.Format.nSamplesPerSec / 2))
	{
		goto end;
	}
	pOpenALDevice->Play();

	thread = std::thread([funcAppendStream, format]() 
		{
			while (true)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(200));

				auto res = funcAppendStream(format.Format.nSamplesPerSec / 4);
				if (res != CodeOK)
				{
					break;
				}
			}
		});
	thread.join();

end:
	pOpenALDevice->Stop();
	delete pOpenALDevice;
	delete pWavFile;
}