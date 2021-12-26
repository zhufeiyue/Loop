#pragma once

#include <map>

#include <QObject>
#include <QTimer>

class PlayerTimer : public QObject
{
	Q_OBJECT
public:
	explicit PlayerTimer(QObject* parent);
	~PlayerTimer();
	int SetRate(double);
	int GetInterval();

	void start();
	void stop();
	int  interval() const;
	bool isActive() const;

signals:
	void timeout();

private slots:
	void OnTimeout();

private:
	double m_rate = 24;
	int m_iWhichTimer = -1;
	int m_iCountTimer = 0;
	std::pair<QTimer*, int> m_timers[2];
};