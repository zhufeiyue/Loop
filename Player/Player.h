#pragma once
#include <mutex>
#include "IRender.h"
#include "IDecoder.h"
#include <QObject>
#include <QMetaMethod>
#include <QWidget>
#include <QTimer>

template<typename T>
class Nursery
{
	void* operator new(size_t) {}
public:
	uint64_t Put(std::shared_ptr<T> pChild)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		uint64_t key = 0;

		if (!pChild)
		{
			return 0;
		}

#ifdef _WIN32
	#ifdef _WIN64
		key = reinterpret_cast<uint64_t>(pChild.get());
	#else
		key = (int64_t)reinterpret_cast<uint32_t>(pChild.get());
	#endif
#else
	#error "not windows plaform"
#endif
		auto iter = m_items.find(key);
		if (iter != m_items.end())
		{
			return 0;
		}
		assert(key != 0);
		m_items.insert(std::make_pair(key, std::move(pChild)));

		return key;
	}

	std::shared_ptr<T> Get(uint64_t key)
	{
		std::lock_guard<std::mutex> guard(m_lock);

		auto iter = m_items.find(key);
		if (iter == m_items.end())
		{
			return nullptr;
		}

		auto value = std::move(iter->second);
		m_items.erase(iter);
		return value;
	}

private:
	std::mutex m_lock;
	std::map<uint64_t, std::shared_ptr<T>> m_items;
};

class Player : public QObject
{
	Q_OBJECT
public:
	Player(QObject* pParent = nullptr);
	virtual ~Player();
	virtual int StartPlay(std::string);
	virtual int StopPlay();

	virtual int InitVideoRender(QWidget*);
	virtual int DestroyVideoRender();

signals:
	void sigDecoderInited(quint64);

protected slots:
	void OnDecoderInited(quint64);
	void OnTimeout();

protected:
	std::unique_ptr<IDecoder> m_pDecoder;
	std::unique_ptr<IRender> m_pVideoRender;
	
	QTimer* m_pTimer = nullptr;
};