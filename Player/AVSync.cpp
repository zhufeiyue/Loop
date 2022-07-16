#include "AVSync.h"
#include <common/EventLoop.h>

double GetSpeedByEnumValue(int iSpeed)
{
	PlaySpeed speed = (PlaySpeed)iSpeed;
	switch (speed)
	{
	case PlaySpeed::Speed_1X:
		return 1.0;
	case PlaySpeed::Speed_0_5X:
		return 0.5;
	case PlaySpeed::Speed_1_5X:
		return 1.5;
	case PlaySpeed::Speed_2X:
		return 2.0;
	default:
		return 1.0;
	}
}

int IAVSync::SetMediaInfo(Dictionary dic)
{
	if (dic.get<int>("hasVideo"))
	{
		m_originVideoTimebase.den = dic.find("videoTimebaseDen")->second.to<int>();
		m_originVideoTimebase.num = dic.find("videoTimebaseNum")->second.to<int>();
	}

	if (dic.get<int>("hasAudio"))
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

int IAVSync::SetPlaySpeed(PlaySpeed speed)
{
	if (speed == m_playSpeed)
	{
		return CodeNo;
	}
	m_playSpeed = speed;

	return CodeOK;
}

int64_t IAVSync::GetCurrentPosition()
{ 
	return m_iCurrentPlayPosition; 
}

int IAVSync::Reset()
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

int SyncAudio::ProcessVolumeFilter(FilterAudio* pFilter, FrameHolderPtr& frame)
{
	if (pFilter)
	{
		return pFilter->Process(frame->FrameData());
	}
	return CodeOK;
}

int SyncAudio::ProcessSpeedFilter(FilterAudio* pFilter, FrameHolderPtr& frame)
{
	if (pFilter)
	{
		return pFilter->Process(frame->FrameData());
	}
	return CodeOK;
}

int SyncAudio::Update(AVSyncParam* pParam)
{
	if (pParam->now - m_timeLastUpdateAudio < std::chrono::milliseconds(m_iAudioUpdateInterval))
	{
		return CodeOK;
	}

	int32_t durationWaitPlay = 0;
	pParam->pAudioRender->GetUseableDuration(durationWaitPlay);
	if (durationWaitPlay > 300) // 300ms
	{
		m_timeLastUpdateAudio = pParam->now;
		return CodeOK;
	}

	FrameHolderPtr frame;
	int n = 0;

again:
	n = pParam->pDecoder->GetNextFrame(frame, 1);
	if (n == CodeOK)
	{
		frame->SetSpeed((int32_t)m_playSpeed);
		if (pParam->pFilterSpeed)
		{
			n = ProcessSpeedFilter(pParam->pFilterSpeed, frame);
			if (n == CodeAgain)
				goto again;
		}
	}


	if (n == CodeOK)
	{
		// pts rescale
		auto pts = frame->FrameData()->pts;
		pts = av_rescale_q(pts, m_originAudioTimebase, m_uniformTimebase);
		frame->SetUniformPTS(pts);
		m_iCurrentPlayPosition = pts;

		// render audio
		n = pParam->pAudioRender->UpdataFrame(frame);
		if (n == CodeAgain)
		{
			goto again;
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
	else if (n == CodeEnd)
	{
		//LOG() << "end of audio";
		return pParam->pAudioRender->Flush();
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}

int SyncAudio::Reset()
{
	m_iAudioUpdateInterval = 150;
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

	if (pParam->now - m_timeLastSync > std::chrono::milliseconds(m_iSyncInterval))
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
		if (n == CodeAgain)
		{
			if (lateFrame)
			{
				// 重新使用，已经迟到的frame
				LOG() << "reuse late frame";
				n = CodeOK;
				videoFrame = std::move(lateFrame);
			}
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
				if (againCount < 1)
				{
					againCount += 1;
					lateFrame = std::move(videoFrame);
					goto again;
				}
				else
				{
					LOG() << "render late image";
				}

				m_iSyncInterval -= 100;
				if (m_iSyncInterval < 100)
					m_iSyncInterval = 100;
			}
			else if (videoPts > audioPts + m_iUpdateInterval)
			{
				LOG() << "too early";
				m_pCachedVideoFrame = std::move(videoFrame);

				m_iSyncInterval -= 100;
				if (m_iSyncInterval < 200)
					m_iSyncInterval = 200;
				return CodeOK;
			}
			else
			{
				m_iSyncInterval = 500;
			}
		}

		pParam->pVideoRender->UpdataFrame(std::move(videoFrame));
	}
	else if (n == CodeAgain)
	{
		LOG() << "no cached video frame";
	}
	else if (n == CodeEnd)
	{
		//LOG() << "end of video";
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}