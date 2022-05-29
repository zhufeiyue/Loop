#pragma once
#include <functional>
#include <QThread>

class QLoop : public QThread
{
public:
	QLoop(QThread* pThread = nullptr, QObject* parent=nullptr);
	~QLoop();

	int PushEvent(std::function<int()>&&);

private:
	void run() override;

private:
	QObject* m_worker = nullptr;
	std::promise<int> m_promise;
};