#pragma once
#include <functional>
#include <QThread>

class QLoop : public QThread
{
public:
	QLoop(QObject*);
	~QLoop();

	int Run(QThread* pThread = nullptr);
	int Exit();
	int PushEvent(std::function<int()>&&);

private:
	void run() override;

private:
	QObject* m_worker = nullptr;
	std::promise<int> m_promise;
};