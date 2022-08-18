#include "RenderOpenAL.h"

double GetSpeedByEnumValue(int);

static ALCdevice* pgDevice = nullptr;

OpenALDevice::OpenALDevice(std::string strDeviceName)
{
	const char* defaultName = nullptr;
	defaultName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
	if (defaultName)
		LOG() << "default openal device name: " << defaultName;

	if(!pgDevice)
		pgDevice = alcOpenDevice(strDeviceName.empty() ? defaultName : strDeviceName.c_str());
	if (!pgDevice)
	{
		LOG() << "alcOpenDevice " << alGetError();
		return;
	}

	m_pContext = alcCreateContext(pgDevice, NULL);
	if (!m_pContext)
	{
		LOG() << "alcCreateContext " << alcGetError(pgDevice);
		return;
	}

	if (!alcMakeContextCurrent(m_pContext))
	{
		LOG() << "alcMakeContextCurrent " << alcGetError(pgDevice);
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
	ALenum err = AL_NO_ERROR;

	if (m_pContext)
	{
		if (m_sourceState == AL_PLAYING)
		{
			alSourcei(m_source, AL_BUFFER, 0);
			alSourceStop(m_source);
		}

		alDeleteSources(1, &m_source);
		alDeleteBuffers(sizeof(m_buffer) / sizeof(m_buffer[0]), m_buffer);

		if (!alcMakeContextCurrent(NULL))
		{
			LOG() << "alcMakeContextCurrent(NULL) failed";
		}
		alcDestroyContext(m_pContext);
		err = alcGetError(pgDevice);
		if (err != AL_NO_ERROR)
		{
			LOG() << "alcDestroyContext " << alcGetString(pgDevice, err);
		}

		m_pContext = NULL;
	}
}

int OpenALDevice::Configure(int bitPerSample, int samplePerSecond, int channel, int channelMask)
{
	m_iChannel = channel;
	m_iBitPerSampe = bitPerSample;
	m_iSamplePerSecond = samplePerSecond;
	m_iBlockAlign = channel * bitPerSample / 8;

	if (m_source == 0)
	{
		return CodeNo;
	}

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

		// 缺少queued buffer，调用alSourcePlay后，source state仍然处于initial或stop状态
		m_sourceState = AL_PLAYING;

		auto err = alGetError();
		if (err != alGetError())
		{
			LOG() << __FUNCTION__ << ' ' << alGetError();
			return CodeNo;
		}
	}

	return CodeOK;
}

int OpenALDevice::Pause(bool bPause)
{
	if (m_source != 0)
	{
		if (bPause)
		{
			alSourcePause(m_source);
			m_sourceState = AL_PAUSED;
		}
		else
		{
			alSourcePlay(m_source);
			m_sourceState = AL_PLAYING;
		}
	}

	return CodeOK;
}

int OpenALDevice::Stop()
{
	std::lock_guard<std::mutex> guard(m_lock);

	if (m_source != 0)
	{
		alSourceStop(m_source);
		alSourcei(m_source, AL_BUFFER, 0);
		alGetSourcei(m_source, AL_SOURCE_STATE, &m_sourceState);

		for (auto iter = m_bufferQueue.begin(); iter != m_bufferQueue.end(); ++iter)
		{
			m_bufferUnQueue.push(iter->id);
		}
		m_bufferQueue.clear();
	}

	m_iBufPts = 0;
	m_iBufSize = 0;

	return CodeOK;
}

int OpenALDevice::Reset()
{
	std::lock_guard<std::mutex> guard(m_lock);

	alSourceStop(m_source);
	alSourcei(m_source, AL_BUFFER, 0);
	for (auto iter = m_bufferQueue.begin(); iter != m_bufferQueue.end(); ++iter)
	{
		m_bufferUnQueue.push(iter->id);
	}
	m_bufferQueue.clear();
	if (m_sourceState == AL_PLAYING)
		alSourcePlay(m_source);

	return CodeOK;
}

int OpenALDevice::SetVolume(float v)
{
	if (v < 0.0f)
	{
		v = 0.0f;
	}
	if (v > 1.0f)
	{
		v = 1.0f;
	}

	//alListenerf(AL_GAIN, v);
	alSourcef(m_source, AL_GAIN, v);
	auto err = alGetError();
	if (err != AL_NO_ERROR)
	{
		return CodeNo;
	}

	return CodeOK;
}

int OpenALDevice::GetVolume(float& v)
{
	//alGetListenerf(AL_GAIN, &v);
	alGetSourcef(m_source, AL_GAIN, &v);
	auto err = alGetError();
	if (err != AL_NO_ERROR)
	{
		return CodeNo;
	}

	return CodeOK;
}

int OpenALDevice::GetPlayPosition(int64_t& pts)
{
	std::lock_guard<std::mutex> guard(m_lock);
	UnqueueBuffer();

	int64_t bufStartPts = m_iBufPts;
	int32_t bufSpeed = m_iBufSpeed;
	ALint sampleOffsetOfCurrentBuf = m_iBufSampleCount;
	if (m_bufferQueue.empty())
	{
	}
	else
	{
		bufStartPts = m_bufferQueue.front().pts;
		bufSpeed = m_bufferQueue.front().speed;
		alGetSourcei(m_source, AL_SAMPLE_OFFSET, &sampleOffsetOfCurrentBuf);
		if (alGetError() != AL_NO_ERROR)
		{
			return CodeNo;
		}
	}

	// ms
	pts = bufStartPts + 
		std::round(GetSpeedByEnumValue(bufSpeed) * 1000 * sampleOffsetOfCurrentBuf / m_iSamplePerSecond);

	return CodeOK;
}

int OpenALDevice::GetUseableDuration(int32_t& duration)
{
	std::lock_guard<std::mutex> guard(m_lock);

	UnqueueBuffer();

	uint32_t n = 0;
	for (auto iter = m_bufferQueue.begin(); iter != m_bufferQueue.end(); ++iter)
	{
		n += iter->duration;
	}

	duration = static_cast<int32_t>(n);
	return CodeOK;
}

int OpenALDevice::AppendWavData(BufPtr data, int64_t pts, int32_t speed)
{
	std::lock_guard<std::mutex> guard(m_lock);
	ALuint uiBuffer = 0;

	if (!m_pContext)
	{
		return CodeNo;
	}
	if (!data || (data->PlayloadSize() % m_iBlockAlign) != 0)
	{
		return CodeNo;
	}

	UnqueueBuffer();
	if (m_bufferUnQueue.empty())
	{
		return CodeRejection;
	}

	bool bAutoRePlay = false;
	// 判断是否因queued buffer用尽，进入的暂停状态;如果是，自动开始播放
	if (m_sourceState == AL_PLAYING) // 本应处于播放状态
	{
		ALint iState;
		auto bufferCount = sizeof(m_buffer) / sizeof(m_buffer[0]); // 总的buffer的数量
		if (m_bufferUnQueue.size() >= bufferCount) // 处于unqueue状态的buffer太多，播放可能已经停止
		{
			LOG() << "check source state";
			alGetSourcei(m_source, AL_SOURCE_STATE, &iState);
			if (iState == AL_STOPPED || iState == AL_INITIAL)
			{
				LOG() << "restart play";
				bAutoRePlay = true;
			}
		}
	}

	// 获得一个未使用(unqueue)的buffer，绑定数据后，加入source中
	uiBuffer = m_bufferUnQueue.front();
	m_bufferUnQueue.pop();
	alBufferData(uiBuffer, m_format, data->DataConst(), data->PlayloadSize(), m_iSamplePerSecond);
	alSourceQueueBuffers(m_source, 1, &uiBuffer);

	BufInfo bufInfo;
	bufInfo.id = uiBuffer;
	bufInfo.pts = pts;
	bufInfo.size = data->PlayloadSize();
	bufInfo.speed = speed;
	bufInfo.sampleCount = bufInfo.size / m_iBlockAlign;
	bufInfo.duration = 1000 * bufInfo.sampleCount / m_iSamplePerSecond;
	m_bufferQueue.push_back(bufInfo);

	if (bAutoRePlay) 
	{
		alSourcePlay(m_source);
	}

	return CodeOK;
}

int OpenALDevice::UnqueueBuffer()
{
	ALint iBuffersProcessed = 0;

	alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);
	if (iBuffersProcessed > 0)
	{
		ALuint temp[16] = { 0 };
		alSourceUnqueueBuffers(m_source, iBuffersProcessed, temp);
		for (ALint i = 0; i < iBuffersProcessed; ++i)
		{
			m_bufferUnQueue.push(temp[i]);

			auto iter = m_bufferQueue.begin();
			auto bok = false;
			for (; iter != m_bufferQueue.end(); ++iter)
			{
				if (iter->id == temp[i])
				{
					m_iBufPts = iter->pts;
					m_iBufSize = iter->size;
					m_iBufSpeed = iter->speed;
					m_iBufSampleCount = iter->sampleCount;

					m_bufferQueue.erase(iter);
					bok = true;
					break;
				}
			}

			if (!bok)
			{
				LOG() << __FUNCTION__ << " " << __LINE__ << " fatal error happened";
			}
		}
	}

	return CodeOK;
}


int AudioDataConvertBatch::Convert(std::vector<FrameHolderPtr>& frames, int& outSampleCount)
{
	int32_t sampleCount = 0;
	int32_t ret;
	int64_t outPts;
	AVFrame* pFrame;

	outSampleCount = 0;
	if (!m_pFrame)
	{
		return CodeNo;
	}

	if (frames.empty())
	{
		return CodeOK;
	}

	for (size_t i = 0; i < frames.size(); ++i)
	{
		sampleCount += frames[i]->FrameData()->nb_samples;
	}

	if (sampleCount > this->m_pFrame->nb_samples)
	{
		auto pBiggerFrame = av_frame_alloc();
		pBiggerFrame->format = m_pFrame->format;
		pBiggerFrame->channel_layout = m_pFrame->channel_layout;
		pBiggerFrame->sample_rate = m_pFrame->sample_rate;
		pBiggerFrame->nb_samples = sampleCount;

		ret = av_frame_get_buffer(pBiggerFrame, 0);
		if (0 != ret)
		{
			PrintFFmpegError(ret, "av_frame_get_buffer");
			return CodeNo;
		}

		av_frame_free(&m_pFrame);
		m_pFrame = pBiggerFrame;
	}

	for (size_t i = 0; i < frames.size(); ++i)
	{
		pFrame = frames[i]->FrameData();
		ret = swr_convert(m_pASwr, (uint8_t**)m_pFrame->data, 0, (const uint8_t**)pFrame->data, pFrame->nb_samples);
		if (ret < 0)
		{
			PrintFFmpegError(ret, "swr_convert");
			return CodeNo;
		}
	}

	ret = swr_convert(m_pASwr, m_pFrame->data, m_pFrame->nb_samples, nullptr, 0);
	if (ret < 0)
	{
		PrintFFmpegError(ret, "swr_convert");
		return CodeNo;
	}

	outSampleCount = ret;

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

	iter = info.find("audioFormat");
	if (iter == info.end())
	{
		return CodeNo;
	}
	m_sampleFormat = (AVSampleFormat)iter->second.to<int32_t>();

	iter = info.find("audioRate");
	if (iter == info.end())
	{
		return CodeNo;
	}
	m_sampleRate = iter->second.to<int32_t>();

	iter = info.find("audioChannel");
	if (iter == info.end())
	{
		return CodeNo;
	}
	m_sampleChannel = iter->second.to<int>();

	iter = info.find("audioChannelLayout");
	if (iter == info.end())
	{
		return CodeNo;
	}
	m_sampleChannelLayout = iter->second.to<int64_t>();

	//if (audioFormat != AV_SAMPLE_FMT_S16 || channel != 2)
	{
		m_pAudioConvert.reset(new AudioDataConvertBatch());
		res = m_pAudioConvert->Configure(
			m_sampleRate, m_sampleChannelLayout, m_sampleFormat,
			m_sampleRate, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16,
			m_sampleRate / 2);
		if (res != CodeOK)
		{
			return CodeNo;
		}
	}

	m_pAudioData.reset(new Buf(m_sampleRate / 2 * 4));

	m_pPlayDevice.reset(new OpenALDevice(""));
	if (CodeOK != m_pPlayDevice->Configure(16, m_sampleRate, 2, 0))
	{
		return CodeNo;
	}
	m_pPlayDevice->Play();
	m_pPlayDevice->SetVolume(1.0f);

	return CodeOK;
}

int RenderOpenAL::Flush()
{
	if (!m_pAudioConvert)
	{
		LOG() << __FUNCTION__ << __LINE__;
		return CodeNo;
	}

	if (m_audioFrames.empty())
	{
		return CodeOK;
	}

	int32_t sampleGot = 0;
	auto ret = m_pAudioConvert->Convert(m_audioFrames, sampleGot);
	if (ret != CodeOK)
	{
		return ret;
	}

	int32_t firstPts = m_audioFrames[0]->UniformPTS();
	int32_t firstSpeed = m_audioFrames[0]->Speed();

	m_audioFrames.clear();
	m_audioSampleCount = 0;

	{
		auto audioSize = sampleGot * 4;
		auto pOutFrame = m_pAudioConvert->Frame();

		memcpy(m_pAudioData->Data(), pOutFrame->data[0], audioSize);
		m_pAudioData->PlayloadSize() = audioSize;


		ret = m_pPlayDevice->AppendWavData(m_pAudioData, firstPts, firstSpeed);
		if (ret == CodeOK)
		{
		}
		else if (ret == CodeRejection)
		{
			LOG() << "AppendWavData rejection";
		}
		else
		{
			return CodeNo;
		}
		
		return CodeOK;
	}
}

int RenderOpenAL::GetUseableDuration(int32_t& duration)
{
	if (!m_pPlayDevice)
	{
		return CodeNo;
	}

	duration = 0;
	return m_pPlayDevice->GetUseableDuration(duration);
}

int RenderOpenAL::UpdataFrame(FrameHolderPtr inData)
{
	auto pFrame = inData->FrameData();
	int ret = -1;

	if (pFrame->format != m_sampleFormat ||
		pFrame->channel_layout != m_sampleChannelLayout ||
		pFrame->sample_rate != m_sampleRate ||
		inData->Speed() != m_sampleSpeed)
	{
		ret = Flush();
		if (ret != CodeOK)
		{
			return ret;
		}

		m_sampleFormat = (AVSampleFormat)pFrame->format;
		m_sampleChannelLayout = pFrame->channel_layout;
		m_sampleChannel = pFrame->channels;
		m_sampleRate = pFrame->sample_rate;
		m_sampleSpeed = inData->Speed();

		m_pAudioData.reset(new Buf(m_sampleRate * 4));

		m_pAudioConvert.reset(new AudioDataConvertBatch());
		ret = m_pAudioConvert->Configure(
			m_sampleRate, m_sampleChannelLayout, m_sampleFormat,
			m_sampleRate, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16,
			m_sampleRate);
		if (ret != CodeOK)
		{
			return ret;
		}
	}

	m_audioFrames.push_back(std::move(inData));
	m_audioSampleCount += pFrame->nb_samples;
	
	if (m_audioSampleCount >= m_sampleRate / 5)
	{
		return Flush();
	}
	else
	{
		return CodeAgain;
	}
}

int RenderOpenAL::Start()
{
	return CodeOK;
}

int RenderOpenAL::Pause(bool b)
{
	if (m_pPlayDevice)
	{
		m_pPlayDevice->Pause(b);
	}
	return CodeOK;
}

int RenderOpenAL::Stop()
{
	if (m_pPlayDevice)
	{
		m_pPlayDevice->Stop();
	}
	return CodeOK;
}

int RenderOpenAL::Reset()
{
	if (m_pPlayDevice)
	{
		m_pPlayDevice->Reset();
	}

	m_audioFrames.clear();
	m_audioSampleCount = 0;

	return CodeOK;
}

int RenderOpenAL::GetRenderTime(int64_t& pts)
{
	if (!m_pPlayDevice)
	{
		return CodeNo;
	}

	pts = 0;
	m_pPlayDevice->GetPlayPosition(pts);

	return CodeOK;
}

int RenderOpenAL::GetVolume(int& iVolume)
{
	iVolume = 0;
	if (!m_pPlayDevice)
	{
		return CodeNo;
	}

	float fVolume = 1.0f;
	int result = m_pPlayDevice->GetVolume(fVolume);
	if (result != CodeOK)
	{
		return CodeNo;
	}

	iVolume = std::round(fVolume * 100);
	return CodeOK;
}

int RenderOpenAL::SetVolume(int iVolume)
{
	if (!m_pPlayDevice)
	{
		return CodeNo;
	}

	if (iVolume < 0)
		iVolume = 0;
	else if (iVolume > 100)
		iVolume = 100;

	float fVolume = iVolume / 100.0f;
	return m_pPlayDevice->SetVolume(fVolume);
}

int RenderOpenAL::AppendOpenALData()
{
	return CodeOK;
}

#if _WIN32
void testPlayWav()
{
	static BufPool<48000, 96000> bufPool;

	auto pWavFile = new WavDemuxerFile("d:/myworld.wav");
	pWavFile->ReadFormat();
	auto& format = pWavFile->AudioFormat();

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
		else if (result == CodeRejection)
		{
		}
		else
		{
			wavBuf.reset();
			return CodeNo;
		}

		//alSpeedOfSound

		return CodeOK;
	};

	pOpenALDevice->Play();
	pOpenALDevice->SetVolume(1.0f);
	
	if (CodeOK != funcAppendStream(format.Format.nSamplesPerSec / 2))
	{
		goto end;
	}
	thread = std::thread([pOpenALDevice, funcAppendStream, format]()
		{
			int64_t pos = 0;
			while (true)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
#endif

IAudioRender* CreateOpenALRender()
{
	return new RenderOpenAL();
}