#include "AVSync.h"
#include <common/EventLoop.h>

int IAVSync::SetMediaInfo(Dictionary dic)
{
	if (dic.contain_key_value("hasVideo", 1))
	{
		m_originVideoTimebase.den = dic.find("videoTimebaseDen")->second.to<int>();
		m_originVideoTimebase.num = dic.find("videoTimebaseNum")->second.to<int>();
	}

	if (dic.contain_key_value("hasAudio", 1))
	{
		m_originAudioTimebase.den = dic.find("audioTimebaseDen")->second.to<int>();
		m_originAudioTimebase.num = dic.find("audioTimebaseNum")->second.to<int>();
	}

	AV_TIME_BASE;
	m_uniformTimebase.den = 1000; // ms
	m_uniformTimebase.num = 1;

	return CodeOK;
}

int IAVSync::SetUpdateInterval(int t)
{
	m_iUpdateInterval = t;
	return CodeOK;
}

int64_t IAVSync::GetCurrentPosition()
{ 
	return m_iCurrentPlayPosition; 
}

int IAVSync::Restet()
{
	return CodeOK;
}

int SyncVideo::Update(AVSyncParam* pParam)
{
	FrameHolderPtr frame;
	int n = 0;

	n = pParam->pDecoder->GetNextFrame(frame, 0);
	if (n == CodeOK)
	{
		auto pts = frame->FrameData()->pts;
		pts = av_rescale_q(pts, m_originVideoTimebase, m_uniformTimebase);
		frame->SetUniformPTS(pts);
		m_iCurrentPlayPosition = pts;

		pParam->pVideoRender->UpdataFrame(std::move(frame));
	}
	else if (n == CodeAgain)
	{
		LOG() << "no cached video frame";
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}


SyncAudio::SyncAudio()
{
	m_timeLastUpdateAudio = std::chrono::steady_clock::now() - std::chrono::seconds(1);
}

int SyncAudio::Update(AVSyncParam* pParam)
{
	if (pParam->now - m_timeLastUpdateAudio < std::chrono::milliseconds(150))
	{
		return CodeOK;
	}
	m_timeLastUpdateAudio = pParam->now;

	FrameHolderPtr frame;
	int n = 0;

again:
	// 得到已解码的音频帧
	if (m_pCachedAudioFrame)
	{
		frame = std::move(m_pCachedAudioFrame);
		n = CodeOK;
	}
	else
	{
		n = pParam->pDecoder->GetNextFrame(frame, 1);
	}

	// 渲染
	if (n == CodeOK)
	{
		// pts转换
		auto pts = frame->FrameData()->pts;
		pts = av_rescale_q(pts, m_originAudioTimebase, m_uniformTimebase);
		frame->SetUniformPTS(pts);
		m_iCurrentPlayPosition = pts;

		n = pParam->pAudioRender->UpdataFrame(frame);
		if (n == CodeAgain)
		{
			goto again;
		}
		else if (n == CodeRejection)
		{
			m_pCachedAudioFrame = std::move(frame);
		}
		else if (n == CodeOK)
		{
		}
		else
		{
			return CodeNo;
		}
	}
	else if (n == CodeAgain)
	{
		LOG() << "no cached audio frame";
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}


SyncAV::SyncAV()
{
	m_timeLastSync = std::chrono::steady_clock::now();
}

int SyncAV::Update(AVSyncParam* pParam)
{
	FrameHolderPtr videoFrame;
	FrameHolderPtr lateFrame;
	int againCount = 0;
	int n = 0;
	bool bNeedSync = false;

	n = SyncAudio::Update(pParam);
	if (n == CodeNo)
	{
		return CodeNo;
	}

	if (pParam->now - m_timeLastSync > std::chrono::milliseconds(500))
	{
		bNeedSync = true;
		m_timeLastSync = pParam->now;
	}

again:
	if (m_pCachedVideoFrame)
	{
		videoFrame = std::move(m_pCachedVideoFrame);
		n = CodeOK;
	}
	else
	{
		n = pParam->pDecoder->GetNextFrame(videoFrame, 0);

		if (n == CodeAgain && lateFrame)
		{
			// 重新使用，已经迟到的frame
			LOG() << "reuse late frame";
			n = CodeOK;
			videoFrame = std::move(lateFrame);
		}
	}
	if (n == CodeOK)
	{
		int64_t videoPts;
		int64_t audioPts;

		if (bNeedSync)
		{
			videoPts = videoFrame->FrameData()->pts;
			videoPts = av_rescale_q(videoPts, m_originVideoTimebase, m_uniformTimebase);
			videoFrame->SetUniformPTS(videoPts);

			pParam->pAudioRender->GetRenderTime(audioPts);

			if (videoPts < audioPts - m_iUpdateInterval*3)
			{
				LOG() << "too late";
				if (againCount < 3)
				{
					againCount += 1;
					lateFrame = std::move(videoFrame);
					goto again;
				}
				else
				{
					LOG() << "render late image";
				}
			}
			else if (videoPts > audioPts + m_iUpdateInterval / 2)
			{
				LOG() << "too early";
				m_pCachedVideoFrame = std::move(videoFrame);
				return CodeOK;
			}
		}

		pParam->pVideoRender->UpdataFrame(std::move(videoFrame));
	}
	else if (n == CodeAgain)
	{
		LOG() << "no cached video frame";
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}