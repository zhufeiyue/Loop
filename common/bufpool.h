#pragma once

#include <memory>
#include <boost/lockfree/queue.hpp>
#include "Log.h"

class Buf
{
public:
	Buf(const Buf&) = delete;
	Buf& operator=(const Buf&) = delete;

	Buf(uint32_t size)
	{
		m_iSize = size;
		m_pData = new uint8_t[m_iSize];
	}
	~Buf()
	{
		if (m_pData)
		{
			delete[]m_pData;
		}
	}

	uint8_t* Data()
	{
		return m_pData;
	}
	const uint8_t* DataConst() const
	{
		return m_pData;
	}
	uint32_t Size() const
	{
		return m_iSize;
	}
	uint32_t& PlayloadSize()
	{
		return m_iPlayloadSize;
	}

protected:
	uint8_t* m_pData = nullptr;
	uint32_t m_iSize = 0;
	uint32_t m_iPlayloadSize = 0;
};

typedef std::shared_ptr<Buf> BufPtr;

template<typename T>
class ObjectPool
{
public:
	~ObjectPool() 
	{
		m_pool.consume_all([](T* p) 
			{ 
				if (p) delete p; 
			}); 
	}

	template<typename ... Args>
	T* Get(Args ... args, bool bNewObject = true)
	{
		T* p = nullptr;
		//auto paramLength = sizeof...(args);
		if (!m_pool.pop(p) && bNewObject)
		{
			p = nullptr;
			try
			{
				p = new T(args...);
				//LOG() << __FUNCTION__ <<  " new T";
			}
			catch (...)
			{
			}
		}

		return p;
	}

	bool Put(T* p)
	{
		if (!p)
		{
			return true;
		}

		bool b = false;
		try
		{
			b = m_pool.bounded_push(p);
		}
		catch (...)
		{
			b = false;
		}
		return b;
	}


private:
	boost::lockfree::queue<T*,
		boost::lockfree::capacity<128>,
		boost::lockfree::fixed_sized<true>> m_pool;
};

template <uint32_t s1, uint32_t s2>
class BufPool
{
public:
	BufPool()
	{
		std::enable_if_t< (s1 >= 1024 && s2 > s1), int> n = 10;
	}

	~BufPool()
	{
	}

	Buf* Alloc(uint32_t size)
	{
		Buf* pBuf = nullptr;

		if(size <= s1)
		{
			pBuf = m_pool1.Get<uint32_t>(s1, true);
		}
		else if (size <= s2)
		{
			pBuf = m_pool2.Get<uint32_t>(s2, true);
		}
		else
		{
			pBuf = m_pool3.Get<uint32_t>(size, true);
			if (pBuf && pBuf->Size() < size)
			{
				delete pBuf;
				try
				{
					pBuf = new Buf(size);
				}
				catch (...)
				{
					pBuf = nullptr;
				}
			}
		}

		return pBuf;
	}

	bool Free(Buf* pBuf)
	{
		if (!pBuf)
		{
			return true;
		}

		pBuf->PlayloadSize() = 0;
		if (pBuf->Size() <= s1)
		{
			if (!m_pool1.Put(pBuf))
			{
				delete pBuf;
			}
		}
		else if (pBuf->Size() <= s2)
		{
			if (!m_pool2.Put(pBuf))
			{
				delete pBuf;
			}
		}
		else
		{
			if (!m_pool3.Put(pBuf))
			{
				delete pBuf;
			}
		}

		return true;
	}

	BufPtr AllocAutoFreeBuf(uint32_t size)
	{
		auto pBuf = Alloc(size);

		if (!pBuf)
		{
			return nullptr;
		}

		return std::shared_ptr<Buf>(pBuf, [this](Buf* p)
			{
				if (p)
					Free(p);
			});
	}

protected:
	ObjectPool<Buf> m_pool1, m_pool2, m_pool3;
};
