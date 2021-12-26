#include "Player.h"
#include "DecodeFile.h"
#include "RenderGraphicsView.h"
#include "RenderOpenGLWidget.h"
#include "RenderOpenAL.h"

template<typename T>
static Nursery<T>& GetNursery()
{
	static Nursery<T> ins;
	return ins;
}

Player::Player(QObject* pParent) :QObject(pParent)
{
	QThread::currentThread()->setPriority(QThread::HighestPriority);
}

Player::~Player()
{
	QThread::currentThread()->setPriority(QThread::NormalPriority);
}

int Player::StartPlay(std::string strMediaPath)
{
	if (m_pDecoder)
	{
		return CodeNo;
	}

	auto sig = QMetaMethod::fromSignal(&Player::sigDecoderInited);
	if (isSignalConnected(sig))
	{
		return CodeNo;
	}
	QObject::connect(this, SIGNAL(sigDecoderInited(quint64)),
		this, SLOT(OnDecoderInited(quint64)), Qt::QueuedConnection);

	m_pDecoder.reset(new DecodeFile);
	m_pDecoder->InitDecoder(strMediaPath, [=](IDecoder::MediaInfo mediaInfo)
		{
			auto value = std::shared_ptr<IDecoder::MediaInfo>(new IDecoder::MediaInfo(std::move(mediaInfo)));
			auto key = GetNursery<IDecoder::MediaInfo>().Put(value);

			emit this->sigDecoderInited(key);
			return CodeOK;
		});

	return CodeOK;
}

int Player::StopPlay()
{
	if (m_pTimer)
	{
		if (m_pTimer->isActive())
		{
			m_pTimer->blockSignals(true);
			m_pTimer->stop();
		}
		m_pTimer->deleteLater();
		m_pTimer = nullptr;
	}

	if (m_pAudioRender)
	{
		m_pAudioRender->Stop();
	}
	if (m_pVideoRender)
	{
		m_pVideoRender->Stop();
	}

	if (m_pDecoder)
	{
		std::promise<int> promise;
		std::future<int> fu = promise.get_future();
		m_pDecoder->DestroyDecoder([&promise](IDecoder::MediaInfo mediaInfo)
		{
			promise.set_value(0);
			return CodeOK;
		});

		fu.get();
	}


	return CodeOK;
}

int Player::Seek(int64_t pos)
{
	// pos ms
	auto sig = QMetaMethod::fromSignal(&Player::sigDecoderSeek);
	if (!isSignalConnected(sig))
	{
		QObject::connect(this, SIGNAL(sigDecoderSeek(quint64)), this, SLOT(OnDecoderSeek(quint64)), Qt::QueuedConnection);
	}

	if (!m_bInited)
	{
		return CodeOK;
	}

	if (m_bSeeking)
	{
		return CodeNo;
	}

	int64_t curPos = 0;
	GetCurrentPos(curPos);

	m_bSeeking = true;

	if (m_pTimer)
	{
		m_pTimer->stop();
	}

	if (m_pAudioRender)
	{
		m_pAudioRender->Reset();
	}
	if (m_pVideoRender)
	{
		m_pVideoRender->Reset();
	}

	if (m_pAVSync)
	{
		m_pAVSync->Restet();
	}

	if (m_pDecoder)
	{
		m_pDecoder->Seek(pos, curPos, [this](Dictionary dic) 
		{
			this->sigDecoderSeek(0);
			return CodeOK;
		});
	}

	return CodeOK;
}

int Player::Pause(bool bPause)
{
	if (!m_bInited)
	{
		return CodeNo;
	}

	if (m_bSeeking)
	{
		return CodeNo;
	}

	if (m_pTimer)
	{
		if (bPause)
			m_pTimer->stop();
		else
			m_pTimer->start();
	}

	if (m_pAudioRender)
	{
		m_pAudioRender->Pause(bPause);
	}
	if (m_pVideoRender)
	{
		m_pVideoRender->Pause(bPause);
	}

	m_bPlaying = m_pTimer->isActive();

	return CodeOK;
}

int Player::SetSpeed(int playSpeed)
{
	if (!m_pAVSync)
	{
		return CodeNo;
	}

	return CodeOK;
}

int Player::GetDuration(int64_t& duration) const
{
	auto iter = m_mediaInfo.find("duration");
	if (iter != m_mediaInfo.end())
	{
		duration = iter->second.to<int64_t>(0); // second
		return CodeOK;
	}
	else
	{
		duration = 0;
		return CodeNo;
	}
}

int Player::GetCurrentPos(int64_t& playPosition) const
{
	if (m_pAVSync)
	{
		playPosition = m_pAVSync->GetCurrentPosition();
		return CodeOK;
	}
	else
	{
		playPosition = 0;
		return CodeNo;
	}
}

bool Player::IsSupportSeek() const
{
	return true;
}

bool Player::IsPlaying() const
{
	return m_bPlaying;
}

void Player::OnDecoderInited(quint64 key)
{
	QObject::disconnect(this, SIGNAL(sigDecoderInited(quint64)),
		this, SLOT(OnDecoderInited(quint64)));

	auto value = GetNursery<IDecoder::MediaInfo>().Get(key);
	if (!value)
	{
		return;
	}

	auto iter = value->find("result");
	if (iter == value->end() || iter->second.to<int>() != CodeOK)
	{
		LOG() << "init decode failed";
		return;
	}

	value->erase("result");
	value->erase("message");
	value->insert("type", "init");
	m_mediaInfo = *value;

	iter = value->find("hasVideo");
	if (iter != value->end())
	{
		m_bHasVideo = iter->second.to<int>();
		if (m_bHasVideo && CodeOK != m_pVideoRender->ConfigureRender(*value))
		{
			LOG() << "video ConfigureRender failed";
			m_bHasVideo = false;
			m_pDecoder->EnableVideo(false);
		}
	}

	iter = value->find("hasAudio");
	if (iter != value->end())
	{
		m_bHasAudio = iter->second.to<int>();
		if (m_bHasAudio && CodeOK != m_pAudioRender->ConfigureRender(*value))
		{
			LOG() << "audio ConfigureRender failed";
			m_bHasAudio = false;
			m_pDecoder->EnableAudio(false);
		}
	}

	// start play timer
	double videoRate = 25;
	iter = value->find("videoRate");
	if (iter != value->end())
	{
		videoRate = iter->second.to<double>();
	}

	if (!m_pTimer)
	{
		m_pTimer = new PlayerTimer(this);
		QObject::connect(m_pTimer, SIGNAL(timeout()), this, SLOT(OnTimeout()));
	}
	if (m_pTimer->isActive())
	{
		m_pTimer->stop();
	}
	m_pTimer->SetRate(videoRate);

	// create av sync
	if (m_bHasVideo && !m_bHasAudio)
	{
		m_pAVSync.reset(new SyncVideo());
	}
	else if (!m_bHasVideo && m_bHasAudio)
	{
		m_pAVSync.reset(new SyncAudio());
	}
	else if (m_bHasVideo && m_bHasAudio)
	{
		//m_pAVSync.reset(new SyncVideo());
		//m_pAVSync.reset(new SyncAudio());
		m_pAVSync.reset(new SyncAV());
	}
	else
	{
		return;
	}
	m_pAVSync->SetUpdateInterval(m_pTimer->interval());
	m_pAVSync->SetMediaInfo(*value);

	m_syncParam.pDecoder = m_pDecoder.get();
	m_syncParam.pVideoRender = m_pVideoRender.get();
	m_syncParam.pAudioRender = m_pAudioRender.get();

	m_bInited = true;
	m_bPlaying = true;
	m_pTimer->start();
}

void Player::OnDecoderSeek(quint64)
{
	if (m_bPlaying)
	{
		// seek前，处于播放状态。直接开始播放，启动定时器即可
		if (m_pTimer)
			m_pTimer->start();
		m_bSeeking = false;
	}
	else
	{
		// seek前，处于暂停状态。执行两次Update后，大概率可以，至少更新显示一帧视频，而音频依然处于暂停状态
		QTimer::singleShot(0, [this]() {
			OnTimeout(); 
		});
		QTimer::singleShot(50, [this]() {
			OnTimeout(); 
			m_bSeeking = false; 
		});
	}
}

void Player::OnTimeout()
{
	m_syncParam.now = std::chrono::steady_clock::now();

	if (CodeOK != m_pAVSync->Update(&m_syncParam))
	{
		m_pTimer->stop();
	}
}

int Player::InitVideoRender(void* pData)
{
	auto pWidget = (QWidget*)pData;
	if (!pWidget)
	{
		return CodeNo;
	}

	if (0)
	{
		auto pView = pWidget->findChild<QGraphicsView*>();
		if (!pView)
		{
			return CodeNo;
		}
		m_pVideoRender.reset(new VideoRenderGraphicsView(pView));
	}
	else
	{
		auto pGLVidget = pWidget->findChild<VideoGLWidget*>();
		if (!pGLVidget)
		{
			return CodeNo;
		}
		m_pVideoRender.reset(new VideoRenderOpenGLWidget(pGLVidget));
	}


	return CodeOK;
}

int Player::DestroyVideoRender()
{
	if (m_pVideoRender)
	{
		m_pVideoRender.reset();
	}
	return CodeOK;
}

int Player::InitAudioRender(void*)
{
	m_pAudioRender.reset(new RenderOpenAL());
	return CodeOK;
}

int Player::DestroyAudioRender()
{
	if (m_pAudioRender)
	{
		m_pAudioRender.reset();
	}
	return CodeOK;
}