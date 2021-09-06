#include "Player.h"
#include "DecodeFile.h"
#include "RenderGraphicsView.h"

static Nursery<IDecoder::MediaInfo>& GetNursery()
{
	static Nursery<IDecoder::MediaInfo> ins;
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
			auto key = GetNursery().Put(value);

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

	auto value = GetNursery().Get(key);
	if (!value)
	{
		return;
	}

	auto iterResult = value->find("result");
	if (iterResult == value->end() || iterResult->second.to<int>() != CodeOK)
	{
		LOG() << "init decode failed";
		return;
	}

	value->erase("result");
	value->erase("message");
	value->insert("type", "init");
	if (CodeOK != m_pVideoRender->ConfigureRender(*value)) 
	{
		LOG() << "ConfigureRender failed";
		return;
	}

	int timerInterval = 40;
	auto iterVideoRate = value->find("videorate");
	if (iterVideoRate != value->end())
	{
		double videoRate = iterVideoRate->second.to<double>();
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
	int n = m_pDecoder->GetNextFrame(frame, 0);
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

int Player::InitVideoRender(QWidget* pWidget)
{
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
	return CodeOK;
}
