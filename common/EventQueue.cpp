#include "EventQueue.h"

AsioEventQueue::AsioEventQueue():
	m_context(1)
{
}

AsioEventQueue::~AsioEventQueue()
{
}

int AsioEventQueue::PushEvent(Fn&& fn)
{
	boost::asio::post(m_context, std::forward<Fn>(fn));

	return CodeOK;
}

int AsioEventQueue::PopEvent()
{
	boost::system::error_code ec;

	auto count = m_context.poll(ec);
	if (ec)
	{
		LOG() << __FUNCTION__ << "asio poll error:" << ec.message();
		return CodeNo;
	}

	return count;
}

static void handleTimer(const boost::system::error_code& err,
	boost::asio::steady_timer* pTimer,
	Fn&& fn,
	int mill, 
	bool repeat)
{
	int isContinue = fn();
	if (repeat && isContinue == CodeOK)
	{
		pTimer->expires_at(pTimer->expiry() + boost::asio::chrono::milliseconds(mill));
		pTimer->async_wait(
			[temp = std::move(fn), mill, repeat, pTimer](const boost::system::error_code& err) mutable
		{
			handleTimer(err, pTimer, std::forward<Fn>(temp), mill, repeat);
		});
	}
	else
	{
		delete pTimer;
	}
}

int AsioEventQueue::ScheduleTimer(Fn&& fn, int mill, bool repeat)
{
	if (!fn)
	{
		return CodeNo;
	}

	boost::asio::steady_timer* pTimer = new boost::asio::steady_timer(
		m_context, 
		boost::asio::chrono::milliseconds(mill));

	pTimer->async_wait(
		[temp = std::move(fn), mill, repeat, pTimer](const boost::system::error_code& err) mutable
	{
		handleTimer(err, pTimer, std::forward<Fn>(temp), mill, repeat);
	});

	return CodeOK;
}
