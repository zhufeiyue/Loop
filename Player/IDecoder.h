#pragma once

#include <memory>
#include <common/BufPool.h>
#include <common/Dic.h>

struct AVFrame;
class FrameHolder
{
public:
	explicit FrameHolder(const std::string& type, int arg1, int arg2,int arg3);
	FrameHolder();
	~FrameHolder();

	FrameHolder(const FrameHolder&) = delete;
	FrameHolder& operator=(const FrameHolder&) = delete;

	FrameHolder(FrameHolder&& r)
	{
		m_pFrame = r.m_pFrame;
		r.m_pFrame = nullptr;
	}
	FrameHolder& operator= (FrameHolder&& r)
	{
		if (this != &r)
		{
			m_pFrame = r.m_pFrame;
			r.m_pFrame = nullptr;
		}
		return *this;
	}

	operator AVFrame* ()
	{
		return m_pFrame;
	}

	int FrameWidth() const;
	int FrameHeight() const;
	int FrameFormat() const;

private:
	AVFrame* m_pFrame = nullptr;
};

typedef std::shared_ptr<FrameHolder> FrameHolderPtr;

class FramePool
{
public:
	bool Free(FrameHolder*);
	FrameHolder* Alloc(const std::string& type, int, int, int);
	FrameHolder* Alloc();

private:
	ObjectPool<FrameHolder> m_pool;
};

class IDecoder
{
public:
	typedef typename Dictionary MediaInfo;
	typedef std::function<int(MediaInfo)> Callback;

public:
	virtual ~IDecoder() {}
	virtual int InitDecoder(std::string, Callback) = 0;
	virtual int DestroyDecoder(Callback) = 0;
	virtual int Seek(int64_t) = 0;
	virtual int GetNextFrame(FrameHolderPtr&, int) = 0;
};