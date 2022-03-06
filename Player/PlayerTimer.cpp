#include "PlayerTimer.h"
#include <common/Log.h>

PlayerTimer::PlayerTimer(QObject* parent):
	QObject(parent)
{
}

PlayerTimer::~PlayerTimer()
{
}

int PlayerTimer::SetRate(double rate)
{
	//if (rate < 1 || rate > 100)
	//{
	//	return CodeNo;
	//}
	if (rate < 1)
		rate = 1;
	if (rate > 60)
		rate = 60;

	double dInterval = 1000.0 / rate;

	int intervalFloor = std::floor(dInterval);
	int intervalCeil = std::ceil(dInterval);

	auto abs_floor_2_real = std::abs(dInterval - intervalFloor);
	auto abs_ceil_2_real = std::abs(dInterval - intervalCeil);

	auto timer_floor = new QTimer(this);
	timer_floor->setInterval(intervalFloor);
	timer_floor->setTimerType(Qt::PreciseTimer);
	QObject::connect(timer_floor, SIGNAL(timeout()), this, SIGNAL(timeout()), Qt::DirectConnection);

	auto timer_ceil = new QTimer(this);
	timer_ceil->setInterval(intervalCeil);
	timer_ceil->setTimerType(Qt::PreciseTimer);
	QObject::connect(timer_ceil, SIGNAL(timeout()), this, SIGNAL(timeout()), Qt::DirectConnection);

	auto bIsRunning = isActive();
	if (m_timers[0].first)
	{
		m_timers[0].first->deleteLater();
	}
	if (m_timers[1].first)
	{
		m_timers[1].first->deleteLater();
	}

	const int kPeriod = static_cast<int>(3 * rate);
	int count = std::round(kPeriod * abs_floor_2_real);
	m_timers[0] = std::make_pair(timer_floor, kPeriod - count);
	m_timers[1] = std::make_pair(timer_ceil, count);
	m_rate = rate;
	m_iWhichTimer = abs_floor_2_real < abs_ceil_2_real ? 0 : 1;
	m_iCountTimer = 0;

	if (intervalFloor == intervalCeil)
	{
	}
	else
	{
		QObject::connect(this, SIGNAL(timeout()), this, SLOT(OnTimeout()));
	}

	if (bIsRunning)
	{
		start();
	}

	return CodeOK;
}

void PlayerTimer::OnTimeout()
{
	m_iCountTimer += 1;
	if (m_iCountTimer >= m_timers[m_iWhichTimer].second)
	{
		m_iCountTimer = 0;
		m_timers[m_iWhichTimer].first->stop();
		if (m_iWhichTimer == 0)
			m_iWhichTimer = 1;
		else if (m_iWhichTimer == 1)
			m_iWhichTimer = 0;
		else
			throw 1;
		m_timers[m_iWhichTimer].first->start();
	}
}

int PlayerTimer::GetInterval()
{
	return 0;
}

void PlayerTimer::start()
{
	if (m_iWhichTimer < 0)
	{
		return;
	}
	if (!m_timers[m_iWhichTimer].first)
	{
		return;
	}

	m_timers[m_iWhichTimer].first->start();
}

void PlayerTimer::stop()
{
	if (m_iWhichTimer < 0)
	{
		return;
	}
	if (!m_timers[m_iWhichTimer].first)
	{
		return;
	}

	m_timers[m_iWhichTimer].first->stop();
}

bool PlayerTimer::isActive() const
{
	if (m_iWhichTimer < 0)
	{
		return false;
	}
	if (!m_timers[m_iWhichTimer].first)
	{
		return false;
	}

	return m_timers[m_iWhichTimer].first->isActive();
}

int PlayerTimer::interval() const
{
	if (m_iWhichTimer < 0)
	{
		return 0;
	}
	if (!m_timers[m_iWhichTimer].first)
	{
		return 0;
	}
	return m_timers[m_iWhichTimer].first->interval();
}