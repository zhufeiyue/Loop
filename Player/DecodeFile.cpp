#include "DecodeFile.h"
#include <common/Log.h>
#include <common/EventLoop.h>
#include "FFmpegDemuxer.h"

DecodeFile::DecodeFile()
{
	m_pEventLoop.reset(new Eventloop());
	m_threadWork = std::thread([this]() 
		{
			m_pEventLoop->Run();
		});
}

DecodeFile::~DecodeFile()
{
	if (m_pEventLoop)
	{
		m_pEventLoop->Exit();
	}

	if (m_threadWork.joinable())
	{
		m_threadWork.join();
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

			m_pDecoder.reset(new FFmpegHWDecode(strMediaPath));
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
				mediaInfo.insert("width", m_pDecoder->GetFrameSize().first);
				mediaInfo.insert("height", m_pDecoder->GetFrameSize().second);
				mediaInfo.insert("videorate", m_pDecoder->GetFrameRate());
				AVPixelFormat format = m_pDecoder->GetFrameFormat();
				if (m_pDecoder->IsSupportHW())
				{
					// todo 硬解出来的图像都是NV12???
					format = AV_PIX_FMT_NV12;
				}
				mediaInfo.insert("videoformat", (int)format);
			}

			if (m_pDecoder->ContainAudio())
			{
				mediaInfo.insert("audioformat", (int)m_pDecoder->GetSampleFormat());
				mediaInfo.insert("audiorate", m_pDecoder->GetSampleRate());
			}
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

int DecodeFile::Seek(int64_t)
{
	return CodeOK;
}

int DecodeFile::GetNextFrame(FrameHolderPtr& frameInfo, int type)
{
	auto pFrame = m_cachedVideoFrame.Alloc();
	if (!pFrame || m_iCachedFrameCount < 2)
	{
		if (m_pEventLoop && m_pEventLoop->IsRunning())
		{
			m_pEventLoop->AsioQueue().PushEvent([this]() 
				{
					while (m_iCachedFrameCount < 5)
					{
						if (DecodeVideoFrame() != CodeOK)
						{
							break;
						}
						else
						{
							m_iCachedFrameCount += 1;
						}
					};
					return CodeOK;
				});
		}
	}
	
	if(pFrame)
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

	auto pFrame = m_blankVideoFrame.Alloc("video", pDecodedImage->width, pDecodedImage->height, pDecodedImage->format);
	if (!pFrame)
	{
		return CodeNo;
	}

	int n = av_frame_copy(*pFrame, pDecodedImage);
	if (n < 0)
	{
		m_blankVideoFrame.Free(pFrame);

		char buf[64] = { 0 };
		av_make_error_string(buf, sizeof(buf), n);
		LOG() << "av_frame_copy " << buf << " " << n;
		return CodeNo;
	}

	m_cachedVideoFrame.Free(pFrame);

	return CodeOK;
}