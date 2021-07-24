#include "EventLoop.h"

static Eventloop* gLoop = nullptr;

Eventloop& GetLoop()
{
	if (gLoop == nullptr)
	{
		throw std::logic_error("there is no running loop");
	}

	return *gLoop;
}

Eventloop::Eventloop()
{
	if (!gLoop)
	{
		LOG() << "set global loop";
		gLoop = this;
	}
}

Eventloop::~Eventloop()
{
	if (gLoop == this)
	{
		gLoop = nullptr;
	}
}

int Eventloop::Run()
{
	LOG() << "event loop start";

	int n = 0;

	while (IsRunning())
	{
		n = m_asioQueue.PopEvent();
		if (n == CodeNo)
		{
			m_bRunning = false;
		}
		else if(n == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	LOG() << "event loop end";
	return CodeOK;
}

int Eventloop::Exit()
{
	m_bRunning = false;
	return CodeOK;
}