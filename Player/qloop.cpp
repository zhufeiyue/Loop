#include "qloop.h"
#include <QEvent>
#include <QApplication>

typedef std::function<int()> EventFn;

class EventLambda : public QEvent
{
public:
	EventLambda(EventFn && fn) : QEvent(QEvent::User)
	{
		m_fn = std::move(fn);
	}
	~EventLambda()
	{
	}
	
	int Do()
	{
		if (m_fn)
			return m_fn();
		else
			return -1;
	}

private:
	EventFn m_fn;
};

class EventWorker: public QObject
{
	bool event(QEvent *pEvent)
	{
		if (pEvent->type() == QEvent::User)
		{
			auto pEventLambda = static_cast<EventLambda*>(pEvent);
			if (pEventLambda)
			{
				pEventLambda->Do();
			}
			pEvent->ignore();
			return true;
		}

		return QObject::event(pEvent);
	}
};

QLoop::QLoop(QObject* parent):
	QThread(parent)
{
}

QLoop::~QLoop()
{
}

void QLoop::run()
{
	m_worker = new EventWorker();
	m_promise.set_value(0);

	exec();

	delete m_worker;
	m_worker = nullptr;
}

int QLoop::Run(QThread* pThread)
{
	if (m_worker)
	{
		return -1;
	}

	if (pThread)
	{
		m_worker = new EventWorker();
		m_worker->moveToThread(pThread);
	}
	else
	{
		if (isRunning())
			return 0;

		start(QThread::TimeCriticalPriority);

		auto fu = m_promise.get_future();
		fu.wait();
	}

	return 0;
}

int QLoop::Exit()
{
	if (isRunning())
	{
		quit();
		if (!wait(3000))
		{
			terminate();
		}
	}
	else
	{
		if (m_worker)
		{
			m_worker->deleteLater();
			m_worker = nullptr;
		}
	}

	return 0;
}

int QLoop::PushEvent(EventFn&& fn)
{
	if (m_worker)
	{
		auto pEvent = new EventLambda(std::forward<EventFn>(fn));
		QApplication::postEvent(m_worker, pEvent);

		return 0;
	}
	else
	{
		return -1;
	}
}