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
	else if (type == strAudioType)
	{
	}
	else
	{
		throw 1;
	}
}

int FrameHolder::FrameWidth() const
{
	if (m_pFrame)
		return m_pFrame->width;
	return 0;
}
int FrameHolder::FrameHeight() const
{
	if (m_pFrame)
		return m_pFrame->height;
	return 0;
}
int FrameHolder::FrameFormat()const
{
	if (m_pFrame)
		return m_pFrame->format;
	return -1;
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

FrameHolder* FramePool::Alloc(const std::string& type, int arg1, int arg2, int arg3)
{
	auto pFrame = m_pool.Get<const std::string&, int, int, int>(type, arg1, arg2, arg3, true);

	if (pFrame)
	{
		if (type == strVideoType)
		{
			if (pFrame->FrameWidth() != arg1 || pFrame->FrameHeight() != arg2 || pFrame->FrameFormat() != arg3)
			{
				delete pFrame;
				pFrame = nullptr;
			}
		}
		else if (type == strAudioType)
		{
		}
	}

	return pFrame;
}

FrameHolder* FramePool::Alloc()
{
	auto pFrame = m_pool.Get<>(false);
	return pFrame;
}