#include "EventLoop.h"

Eventloop::Eventloop()
{
}

Eventloop::~Eventloop()
{
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
		else
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