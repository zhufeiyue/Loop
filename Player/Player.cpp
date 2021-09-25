#include "Player.h"
#include "DecodeFile.h"
#include "RenderGraphicsView.h"
#include "RenderOpenAL.h"

template<typename T>
static Nursery<T>& GetNursery()
{
	static Nursery<T> ins;
	return ins;
}

Player::Player(QObject* pParent) :QObject(pParent)
{
}

Player::~Player()
{
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
	if (!m_pDecoder)
	{
		return CodeNo;
	}

	m_pDecoder->DestroyDecoder([=](IDecoder::MediaInfo mediaInfo) 
		{
			return CodeOK;
		});

	return CodeOK;
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

	iter = value->find("hasVideo");
	if (iter != value->end() && 
		iter->second.to<int>() &&
		CodeOK != m_pVideoRender->ConfigureRender(*value))
	{
		LOG() << "video ConfigureRender failed";
		return;
	}

	iter = value->find("hasAudio");
	if (iter != value->end() &&
		iter->second.to<int>() &&
		CodeOK != m_pAudioRender->ConfigureRender(*value))
	{
		LOG() << "audio ConfigureRender failed";
		return;
	}

	int timerInterval = 40;
	iter = value->find("videorate");
	if (iter != value->end())
	{
		double videoRate = iter->second.to<double>();
		if (videoRate > 0) 
		{
			timerInterval = static_cast<int> (std::round(1000.0 / videoRate));
		}
	}

	if (!m_pTimer)
	{
		m_pTimer = new QTimer();
		m_pTimer->setTimerType(Qt::PreciseTimer);
		QObject::connect(m_pTimer, SIGNAL(timeout()), this, SLOT(OnTimeout()));
	}
	if (m_pTimer->isActive())
	{
		m_pTimer->stop();
	}
	m_pTimer->setInterval(timerInterval);
	m_pTimer->start();
}

void Player::OnTimeout()
{
	if (!m_pDecoder || !m_pVideoRender)
	{
		return;
	}

	FrameHolderPtr frame;
	int n = 0;
	n = m_pDecoder->GetNextFrame(frame, 0);
	if (n == CodeOK)
	{
		if (m_pVideoRender)
		{
			m_pVideoRender->UpdataFrame(std::move(frame));
		}
	}
	else if (n == CodeAgain)
	{
		LOG() << "no cached frame";
	}
	else
	{
		LOG() << "play stop";
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

	auto pView = pWidget->findChild<QGraphicsView*>();
	if (!pView)
	{
		return CodeNo;
	}
	m_pVideoRender.reset(new VideoRenderGraphicsView(pView));

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