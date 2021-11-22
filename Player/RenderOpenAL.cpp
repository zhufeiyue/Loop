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
		if (m_sourceState == AL_INITIAL)
		{
			// 缺少queued buffer，调用alSourcePlay后，source state仍然处于initial状态
			m_sourceState = AL_PLAYING;
		}
		else if (m_sourceState != AL_PLAYING)
		{
			LOG() << __FUNCTION__ << ' ' << alGetError();
			return CodeNo;
		}

		//alSourcef(m_source, AL_PITCH, 1.5f);
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

		std::lock_guard<std::mutex> guard(m_lock);
		UnqueueBuffer();
	}

	m_iBufPts = 0;
	m_iBufSize = 0;

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

int OpenALDevice::GetPlayPts(int64_t& pts)
{
	UnqueueBuffer();

	int64_t bufStartPts = m_iBufPts;
	ALint sampleOffsetOfCurrentBuf = m_iBufSampleCount;
	if (m_bufferQueue.empty())
	{
	}
	else
	{
		bufStartPts = std::get<1>(m_bufferQueue.front());

		alGetSourcei(m_source, AL_SAMPLE_OFFSET, &sampleOffsetOfCurrentBuf);
		if (alGetError() != AL_NO_ERROR)
		{
			return CodeNo;
		}
	}

	// ms
	pts = bufStartPts + 1000 * sampleOffsetOfCurrentBuf / m_iSamplePerSecond;

	//LOG() << pts;

	return CodeOK;
}

int OpenALDevice::AppendWavData(BufPtr data, int64_t pts)
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
		if (m_bufferUnQueue.size() >= bufferCount-1) // 处于unqueue状态的buffer太多，播放可能已经停止
		{
			LOG() << "check source state";
			alGetSourcei(m_source, AL_SOURCE_STATE, &iState);
			if (iState == AL_STOPPED || iState == AL_INITIAL)
			{
				bAutoRePlay = true;
			}
		}
	}

	// 获得一个未使用(unqueue)的buffer，绑定数据后，加入source中
	uiBuffer = m_bufferUnQueue.front();
	m_bufferUnQueue.pop();
	alBufferData(uiBuffer, m_format, data->DataConst(), data->PlayloadSize(), m_iSamplePerSecond);
	alSourceQueueBuffers(m_source, 1, &uiBuffer);

	// buffer id\buffer pts\buffer size
	m_bufferQueue.push_back(std::make_tuple(uiBuffer, pts, data->PlayloadSize()));

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
		for (auto i = 0; i < iBuffersProcessed; ++i)
		{
			m_bufferUnQueue.push(temp[i]);

			auto iter = m_bufferQueue.begin();
			auto bok = false;
			for (; iter != m_bufferQueue.end(); ++iter)
			{
				if (std::get<0>(*iter) == temp[i])
				{
					m_iBufPts = std::get<1>(*iter);
					m_iBufSize = std::get<2>(*iter);
					m_iBufSampleCount = m_iBufSize / m_iBlockAlign;

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


int AudioDataCacheConvert::Convert(const uint8_t** ppInData, int incount, int64_t inPts, int64_t& outPts, bool& bReject)
{
	/*
	* 利用swr_convert的性质，转换、缓存，不超过一定数量的音频帧
	* 
	* 通过outPts获得，一段数据的起始pts
	* 通过bReject获得，当前输入数据是否被处理
	* 返回值，负数-发生错误，0-需要更多的输入，正数-得到输出帧数
	*/

	bReject = true;

	if (!m_pASwr)
	{
		return CodeNo;
	}

	int outcount = swr_get_out_samples(m_pASwr, incount);
	int n = 0;

	if (outcount > m_pFrame->nb_samples)
	{
		n = swr_convert(m_pASwr, m_pFrame->data, m_pFrame->nb_samples, nullptr, 0);
		outPts = m_iPts;
		m_iPts = -1;
	}
	else
	{
		if (m_iPts == -1)
		{
			m_iPts = inPts;
		}
		n = swr_convert(m_pASwr, m_pFrame->data, 0, ppInData, incount);
		bReject = false;
	}

	return n;
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
	auto audioFormat = (AVSampleFormat)iter->second.to<int32_t>();

	iter = info.find("audioRate");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto audioRate = iter->second.to<int32_t>();

	iter = info.find("audioChannel");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto channel = iter->second.to<int>();

	iter = info.find("audioChannelLayout");
	if (iter == info.end())
	{
		return CodeNo;
	}
	auto channelLayout = iter->second.to<int64_t>();

	int sampleCountAppend = audioRate / 4;
	//if (audioFormat != AV_SAMPLE_FMT_S16 || channel != 2)
	{
		m_pAudioConvert.reset(new AudioDataCacheConvert());
		res = m_pAudioConvert->Configure(
			audioRate, channelLayout, audioFormat,
			audioRate, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16,
			sampleCountAppend);
		if (res != CodeOK)
		{
			return CodeNo;
		}
	}

	m_pAudioData.reset(new Buf(sampleCountAppend * 4));

	m_pPlayDevice.reset(new OpenALDevice(""));
	if (CodeOK != m_pPlayDevice->Configure(16, audioRate, 2, 0))
	{
		return CodeNo;
	}
	m_pPlayDevice->Play();
	m_pPlayDevice->SetVolume(1.0f);

	return CodeOK;
}

int RenderOpenAL::UpdataFrame(FrameHolderPtr inData)
{
	if (m_bAudioDataValid)
	{
		if (CodeOK != AppendOpenALData())
		{
			return CodeNo;
		}

		return CodeRejection;
	}

	bool bReject = false;
	int n = 0;
	int64_t outPts = 0;

	n = m_pAudioConvert->Convert(
		(const uint8_t**)inData->FrameData()->data,
		inData->FrameData()->nb_samples,
		inData->UniformPTS(), outPts, bReject);

	if (n < 0)
	{
		return CodeNo;
	}

	if (n == 0)
	{
		return CodeAgain;
	}
	else
	{
		auto audioSize = n * 4;
		auto pOutFrame = m_pAudioConvert->Frame();

		memcpy(m_pAudioData->Data(), pOutFrame->data[0], audioSize);
		m_pAudioData->PlayloadSize() = audioSize;
		m_iAudioDataPts = outPts;

		if (CodeOK != AppendOpenALData())
		{
			return CodeNo;
		}

		return CodeRejection;
	}
}

int RenderOpenAL::Start()
{
	return CodeOK;
}

int RenderOpenAL::Pause()
{
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

int RenderOpenAL::Seek(int64_t)
{
	return CodeOK;
}

int RenderOpenAL::GetRenderTime(int64_t& pts)
{
	if (!m_pPlayDevice)
	{
		return CodeNo;
	}

	pts = 0;
	m_pPlayDevice->GetPlayPts(pts);

	return CodeOK;
}

int RenderOpenAL::AppendOpenALData()
{
	int n = m_pPlayDevice->AppendWavData(m_pAudioData, m_iAudioDataPts);

	m_bAudioDataValid = false;
	if (n == CodeOK)
	{
	}
	else if (n == CodeRejection)
	{
		m_bAudioDataValid = true;
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}


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