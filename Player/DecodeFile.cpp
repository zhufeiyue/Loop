#include "DecodeFile.h"
#include <common/Log.h>
#include <common/EventLoop.h>
#include "FFmpegDemuxer.h"


DecodeFile::DecodeFile()
{
	m_pEventLoop.reset(new Eventloop());

	for (int i = 0; i < 2; ++i)
	{
		m_threadWork[i] = std::thread([this]()
		{
			m_pEventLoop->Run();
		});
	}
}

DecodeFile::~DecodeFile()
{
	m_bVideoDecodeError = true;
	m_bAudioDecodeError = true;

	if (m_pEventLoop)
	{
		m_pEventLoop->Exit();
	}

	for (int i = 0; i < 2; ++i)
	{
		if (m_threadWork[i].joinable())
		{
			m_threadWork[i].join();
		}
	}

	if (m_pDecoder)
	{
		m_pDecoder.reset(nullptr);
	}
}

int DecodeFile::InitDecoder(std::string strMediaPath, IDecoder::Callback initCallback)
{
	if (!m_pEventLoop && !m_pEventLoop->IsRunning())
	{
		return CodeNo;
	}

	m_pEventLoop->AsioQueue().PushEvent([=]()
		{
			MediaInfo mediaInfo;
			std::string strMsg = "fail";
			int32_t result = CodeNo;

			if (m_pDecoder)
			{
				strMsg = "Decoder already exists";
				LOG() << strMsg;
				goto End;
			}

			FileProvider* pFileProvider = nullptr;
			//pFileProvider = new FileProvider(strMediaPath);

			m_pDecoder.reset(new FFmpegHWDecode(strMediaPath, pFileProvider));
			//m_pDecoder.reset(new FFmpegDecode(strMediaPath));

			if (!m_pDecoder->ContainVideo() && !m_pDecoder->ContainAudio())
			{
				strMsg = "no media found for " + strMediaPath;
				LOG() << strMsg;
				goto End;
			}

			if (CodeOK != m_pDecoder->CreateDecoder())
			{
				strMsg = "create decoder failed";
				LOG() << strMsg;
				goto End;
			}

			if (m_pDecoder->ContainVideo())
			{
				m_iMaxCacheVideoFrameCount = 10;
				auto videoTimebase = m_pDecoder->GetVideoTimebase(0);

				mediaInfo.insert("hasVideo", true);
				mediaInfo.insert("videoTimebaseNum", videoTimebase.num);
				mediaInfo.insert("videoTimebaseDen", videoTimebase.den);
				mediaInfo.insert("width", m_pDecoder->GetFrameSize().first);
				mediaInfo.insert("height", m_pDecoder->GetFrameSize().second);
				mediaInfo.insert("videoRate", m_pDecoder->GetFrameRate());
				AVPixelFormat format = m_pDecoder->GetFrameFormat();
				if (m_pDecoder->IsSupportHW())
				{
					// todo 硬解出来的图像都是NV12???
					format = AV_PIX_FMT_NV12;
				}
				mediaInfo.insert("videoFormat", (int)format);
			}

			if (m_pDecoder->ContainAudio())
			{
				m_iAuioRate = m_pDecoder->GetSampleRate();
				m_iMaxCacheAudioFrameCount = m_iAuioRate;
				auto audioTimebase = m_pDecoder->GetAudioTimebase(0);

				mediaInfo.insert("hasAudio", true);
				mediaInfo.insert("audioTimebaseNum", audioTimebase.num);
				mediaInfo.insert("audioTimebaseDen", audioTimebase.den);
				mediaInfo.insert("audioFormat", (int)m_pDecoder->GetSampleFormat());
				mediaInfo.insert("audioRate", m_iAuioRate);
				mediaInfo.insert("audioChannel", m_pDecoder->GetSampleChannel());
				mediaInfo.insert("audioChannelLayout", m_pDecoder->GetChannelLayout());
			}

			mediaInfo.insert("duration", m_pDecoder->GetDuration());

			result = CodeOK;
			strMsg = "OK";

		End:
			mediaInfo.insert("result", result);
			mediaInfo.insert("message", strMsg);

			if (result != CodeOK)
			{
				m_pDecoder.reset(nullptr);
			}

			if (initCallback)
			{
				initCallback(mediaInfo);
			}

			return result;
		});

	return CodeOK;
}

int DecodeFile::DestroyDecoder(IDecoder::Callback destroyCallback)
{
	if (!m_pEventLoop && !m_pEventLoop->IsRunning())
	{
		return CodeNo;
	}

	m_pEventLoop->AsioQueue().PushEvent([=]() 
		{
			m_iMaxCacheVideoFrameCount = 0;
			m_iMaxCacheAudioFrameCount = 0;
			while (m_bAudioDecoding || m_bVideoDecoding)
			{
				LOG() << "waiting decode end(for stop)";
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			m_pDecoder.reset(nullptr);

			MediaInfo mediaInfo;
			mediaInfo.insert("result", (int)CodeOK);
			mediaInfo.insert("message", "OK");
			if (destroyCallback)
			{
				destroyCallback(mediaInfo);
			}

			return CodeOK;
		});

	return CodeOK;
}

bool DecodeFile::EnableVideo(bool bEnable)
{
	if (m_pDecoder)
	{
		return m_pDecoder->EnableVideo(bEnable);
	}
	return false;
}

bool DecodeFile::EnableAudio(bool bEnable)
{
	if (m_pDecoder)
	{
		return m_pDecoder->EnableAudio(bEnable);
	}
	return false;
}

int DecodeFile::Seek(int64_t pos, int64_t currPos, Callback seekCb)
{
	if (!m_pEventLoop || !m_pEventLoop->IsRunning())
	{
		return CodeNo;
	}
	if (m_bSeeking)
	{
		return CodeNo;
	}

	auto oldMaxVideoCount = m_iMaxCacheVideoFrameCount;
	auto oldMaxAudioCount = m_iMaxCacheAudioFrameCount;

	m_iMaxCacheAudioFrameCount = 0;
	m_iMaxCacheVideoFrameCount = 0;
	m_bSeeking = true;

	m_pEventLoop->AsioQueue().PushEvent([=]() 
	{
		while (m_bVideoDecoding || m_bAudioDecoding)
		{
			LOG() << "waiting decode end(for seek)";
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (m_pDecoder)
		{
			if (CodeOK != m_pDecoder->Seek(pos, currPos))
			{
				return CodeNo;
			}
		}

		FrameHolder* pFrameHolder = nullptr;
		while (true)
		{
			pFrameHolder = m_cachedVideoFrame.Alloc();
			if (pFrameHolder)
			{
				//m_iCachedFrameCount -= 1;
				m_blankVideoFrame.Free(pFrameHolder);
			}
			else
			{
				break;
			}
		}
		m_iCachedFrameCount = 0;

		while (true)
		{
			pFrameHolder = m_cacheAudioFrame.Alloc();
			if (pFrameHolder)
			{
				//m_iCachedSampleCount -= pFrameHolder->FrameData()->nb_samples;
				m_blankAudioFrame.Free(pFrameHolder);
			}
			else
			{
				break;
			}
		}
		m_iCachedSampleCount = 0;

		m_bSeeking = false;
		m_iMaxCacheAudioFrameCount = oldMaxAudioCount;
		m_iMaxCacheVideoFrameCount = oldMaxVideoCount;

		if (seekCb)
		{
			Dictionary dic;
			dic.insert("result", 1);

			seekCb(dic);
		}

		return CodeOK;
	});

	return CodeOK;
}

int DecodeFile::GetNextFrame(FrameHolderPtr& frameInfo, int type)
{
	if (m_bSeeking)
	{
		return CodeAgain;
	}

	if (type == 0)
	{
		return GetNextVideoFrmae(frameInfo);
	}
	else if (type == 1)
	{
		return GetNextAudioFrame(frameInfo);
	}

	return CodeNo;
}

int DecodeFile::GetNextVideoFrmae(FrameHolderPtr& frameInfo)
{
	auto pFrame = m_cachedVideoFrame.Alloc();

	if (m_bVideoDecodeError)
	{
		if (pFrame)
		{
			goto GotData;
		}
		if (m_pDecoder->VideoDecodeError() == AVERROR_EOF && !m_pDecoder->IsEOF())
		{
			return CodeEnd;
		}
		return CodeNo;
	}

	if (!pFrame || m_iCachedFrameCount < m_iMaxCacheVideoFrameCount / 2)
	{
		if (m_pEventLoop && m_pEventLoop->IsRunning() && !m_bVideoDecoding)
		{
			m_bVideoDecoding = true;
			m_pEventLoop->AsioQueue().PushEvent([this]()
				{
					while (m_iCachedFrameCount < m_iMaxCacheVideoFrameCount &&
						m_bVideoDecoding && !m_bVideoDecodeError)
					{
						if (DecodeVideoFrame() != CodeOK)
						{
							// todo 获得出错原因，end of stream ???
							m_bVideoDecodeError = true;
						}
						else
						{
							m_iCachedFrameCount += 1;
						}
					}

					m_bVideoDecoding = false;
					return CodeOK;
				});
		}
	}

GotData:
	if (pFrame)
	{
		m_iCachedFrameCount -= 1;
		frameInfo = FrameHolderPtr(pFrame, [this](FrameHolder* pFrameToFree)
			{
				m_blankVideoFrame.Free(pFrameToFree);
			});
		return CodeOK;
	}
	else
	{
		return CodeAgain;
	}
}

int DecodeFile::DecodeVideoFrame()
{
	if (!m_pDecoder)
	{
		return CodeNo;
	}

	auto res = m_pDecoder->DecodeVideoFrame();
	if (res != CodeOK)
	{
		LOG() << "DecodeVideoFrame " << res;
		return CodeNo;
	}

	auto pDecodedImage = m_pDecoder->GetVFrame();
	if (!pDecodedImage)
	{
		return CodeNo;
	}

	auto pFrame = m_blankVideoFrame.Alloc("video", 
		pDecodedImage->width, 
		pDecodedImage->height, 
		pDecodedImage->format, 0);
	if (!pFrame)
	{
		return CodeNo;
	}

	int n = av_frame_copy(pFrame->FrameData(), pDecodedImage);
	if (n < 0)
	{
		m_blankVideoFrame.Free(pFrame);

		PrintFFmpegError(n, __FUNCTION__ " av_frame_copy");
		return CodeNo;
	}
	n = av_frame_copy_props(pFrame->FrameData(), pDecodedImage);

	if (!m_cachedVideoFrame.Free(pFrame))
	{
		LOG() << __FUNCTION__ << " " << __LINE__ << " " << "cache decoded frame error";
		return CodeNo;
	}

	return CodeOK;
}

int DecodeFile::GetNextAudioFrame(FrameHolderPtr& frameInfo)
{
	auto pFrame = m_cacheAudioFrame.Alloc();

	if (m_bAudioDecodeError)
	{
		if (pFrame)
		{
			goto GotData;
		}

		if (m_pDecoder->AudioDecodeError() == AVERROR_EOF && !m_pDecoder->IsEOF())
		{
			return CodeEnd;
		}
		return CodeNo;
	}

	if (!pFrame || m_iCachedSampleCount < m_iMaxCacheAudioFrameCount / 2)
	{
		if (m_pEventLoop && m_pEventLoop->IsRunning() && !m_bAudioDecoding)
		{
			m_bAudioDecoding = true;
			m_pEventLoop->AsioQueue().PushEvent([this]()
				{
					while (m_iCachedSampleCount < m_iMaxCacheAudioFrameCount && 
						m_bAudioDecoding && !m_bAudioDecodeError)
					{
						int sampleCount = 0;
						if (DecodeAudioFrame(sampleCount) != CodeOK)
						{
							m_bAudioDecodeError = true;
						}
						else
						{
							m_iCachedSampleCount += sampleCount;
						}
					}

					m_bAudioDecoding = false;
					return CodeOK;
				});
		}
	}

GotData:
	if (pFrame)
	{
		m_iCachedSampleCount -= pFrame->FrameData()->nb_samples;

		frameInfo = FrameHolderPtr(pFrame, [this](FrameHolder* pFrametoFree) 
			{
				m_blankAudioFrame.Free(pFrametoFree);
			});
		return CodeOK;
	}
	else
	{
		return CodeAgain;
	}
}

int DecodeFile::DecodeAudioFrame(int& sampleCountGot)
{
	if (!m_pDecoder)
	{
		return CodeNo;
	}

	auto res = m_pDecoder->DecodeAudioFrame();
	if (res != CodeOK)
	{
		LOG() << "DecodeAudioFrame " << res;
		return CodeNo;
	}

	auto pDecodeSample = m_pDecoder->GetAFrame();
	sampleCountGot = pDecodeSample->nb_samples;
	if (pDecodeSample->channel_layout == 0)
		pDecodeSample->channel_layout = av_get_default_channel_layout(pDecodeSample->channels);

	AV_CH_LAYOUT_STEREO;
	auto pFrame = m_blankAudioFrame.Alloc("audio",
		pDecodeSample->format,
		pDecodeSample->sample_rate,
		pDecodeSample->nb_samples,
		pDecodeSample->channel_layout);
	if (!pFrame)
	{
		return CodeNo;
	}

	int n = av_frame_copy(pFrame->FrameData(), pDecodeSample);
	if (n < 0)
	{
		m_blankAudioFrame.Free(pFrame);

		PrintFFmpegError(n, __FUNCTION__ " av_frame_copy");
		return CodeNo;
	}
	n = av_frame_copy_props(pFrame->FrameData(), pDecodeSample);

	//LOG() << "cache audio sample";
	if (!m_cacheAudioFrame.Free(pFrame))
	{
		LOG() << __FUNCTION__ << " " << __LINE__ << " " << "cache decoded sample error";
		return CodeNo;
	}

	return CodeOK;
}