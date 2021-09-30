#include "IDecoder.h"

extern "C"
{
#include <libavutil/frame.h>
}

static std::string strVideoType = "video";
static std::string strAudioType = "audio";

FrameHolder::~FrameHolder()
{
	if (m_pFrame)
	{
		av_frame_free(&m_pFrame);
	}
}

FrameHolder::FrameHolder()
{
	throw std::logic_error("fatal error");
}

FrameHolder::FrameHolder(const std::string& type, int arg1, int arg2, int arg3)
{
	if (type == strVideoType)
	{
		int width = arg1;
		int height = arg2;
		int format = arg3;

		m_pFrame = av_frame_alloc();
		m_pFrame->width = width;
		m_pFrame->height = height;
		m_pFrame->format = AVPixelFormat(format);

		if (av_frame_get_buffer(m_pFrame, 0) < 0)
		{
			av_frame_free(&m_pFrame);
			throw std::logic_error("av_frame_get_buffer error");
		}
	}
	else
	{
		throw std::logic_error("fatal error");
	}
}

FrameHolder::FrameHolder(const std::string& type, int arg1, int arg2, int arg3, int arg4)
{
	if (type == strAudioType)
	{
		int channelLayout = arg1;
		int rate = arg2;
		int sampleCount = arg3;
		int format = arg4;

		m_pFrame = av_frame_alloc();
		m_pFrame->format = (AVSampleFormat)format;
		m_pFrame->channel_layout = channelLayout;
		m_pFrame->sample_rate = rate;
		m_pFrame->nb_samples = sampleCount;

		if (av_frame_get_buffer(m_pFrame, 0) < 0)
		{
			av_frame_free(&m_pFrame);
			throw std::logic_error("av_frame_get_buffer error");
		}
	}
	else
	{
		throw std::logic_error("fatal error");
	}
}

bool FramePool::Free(FrameHolder* pFrame)
{
	if (pFrame)
	{
		if (!m_pool.Put(pFrame))
		{
			delete pFrame;
		}
	}
	return true;
}

FrameHolder* FramePool::Alloc(const std::string& type, int arg1, int arg2, int arg3, int arg4)
{
	FrameHolder* pFrame = nullptr;

	if (type == strVideoType)
	{
		pFrame = m_pool.Get<const std::string&, int, int, int>(type, arg1, arg2, arg3, true);
		if (!pFrame)
		{
			return nullptr;
		}

		auto ffmpegFrame = pFrame->FrameData();
		if (ffmpegFrame->width != arg1 || ffmpegFrame->height != arg2 || ffmpegFrame->format != arg3)
		{
			delete pFrame;
			pFrame = nullptr;
		}
	}
	else if (type == strAudioType)
	{
		pFrame = m_pool.Get<const std::string&, int, int, int, int>(type, arg1, arg2, arg3, arg4, true);
		if (!pFrame)
		{
			return nullptr;
		}

		if (pFrame->FrameData()->channel_layout != arg1 
			|| pFrame->FrameData()->sample_rate != arg2
			|| pFrame->FrameData()->format != arg4)
		{
			delete pFrame;
			pFrame = nullptr;
		}
	}

	return pFrame;
}

FrameHolder* FramePool::Alloc()
{
	auto pFrame = m_pool.Get<>(false);
	return pFrame;
}