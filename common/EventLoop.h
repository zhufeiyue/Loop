#pragma once

#include "EventQueue.h"

class Eventloop
{
public:
	Eventloop();
	~Eventloop();
	int Run();
	int Exit();

	bool IsRunning() 
	{ return m_bRunning; }
	AsioEventQueue& AsioQueue() 
	{ return m_asioQueue; }

private:
	bool m_bRunning = true;
	AsioEventQueue m_asioQueue;
};

Eventloop& GetLoop();