#pragma once

#include <memory>
#include <common/BufPool.h>
#include <common/Dic.h>

struct AVFrame;
class FrameHolder
{
public:
	FrameHolder(const std::string& type, int arg1, int arg2,int arg3);
	FrameHolder(const std::string& type, int arg1, int arg2, int arg3, int64_t arg4);
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

	AVFrame* FrameData()
	{
		return m_pFrame;
	}

	int64_t UniformPTS()
	{
		return m_iUniformPTS;
	}

	void SetUniformPTS(int64_t pts)
	{
		m_iUniformPTS = pts;
	}

private:
	AVFrame* m_pFrame = nullptr;
	int64_t  m_iUniformPTS = 0;
};

typedef std::shared_ptr<FrameHolder> FrameHolderPtr;

class FramePool
{
public:
	bool Free(FrameHolder*);
	FrameHolder* Alloc(const std::string& type, int, int, int, int64_t);
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
	virtual int Seek(int64_t, int64_t, Callback) = 0;
	virtual int GetNextFrame(FrameHolderPtr&, int) = 0;
};