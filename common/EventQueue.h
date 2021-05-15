#pragma once

#ifdef _MSC_VER
#include <sdkddkver.h>
#endif
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>
#include <functional>
#include <memory>
#include <thread>
#include "log.h"

typedef std::function<int()> Fn;

class AsioEventQueue
{
public:
	AsioEventQueue();
	~AsioEventQueue();

	boost::asio::io_service& Context()
	{ return m_context; }

	int PushEvent(Fn&&);
	int PopEvent();
	int ScheduleTimer(Fn&&, int mill, bool repeat);

private:
	boost::asio::io_service m_context;
};