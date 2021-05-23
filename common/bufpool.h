#pragma once

#include <memory>
#include "log.h"
#include <boost/lockfree/queue.hpp>

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
	uint32_t Size()
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
		m_q1.consume_all([](Buf* p) {if (p)delete p; });
		m_q2.consume_all([](Buf* p) {if (p)delete p; });
		m_q3.consume_all([](Buf* p) {if (p)delete p; });
	}

	Buf* Alloc(uint32_t size)
	{
		Buf* pBuf = nullptr;
		uint32_t allocSize = 0;

		if(size <= s1)
		{
			if (!m_q1.pop(pBuf))
			{
				allocSize = s1;
			}
		}
		else if (size <= s2)
		{
			if (!m_q2.pop(pBuf))
			{
				allocSize = s2;
			}
		}
		else
		{
			if (!m_q3.pop(pBuf))
			{
				allocSize = size;
			}
			else if (pBuf->Size() < size)
			{
				delete pBuf;
				pBuf = nullptr;
				allocSize = size;
			}
		}

		if (allocSize > 0)
		{
			try
			{
				LOG() << "alloc buf " << allocSize << " s1:" << s1 << " s2:" << s2;
				pBuf = new Buf(allocSize);
			}
			catch (...)
			{
			}
		}
		pBuf->PlayloadSize() = size;

		return pBuf;
	}

	bool Free(Buf* pBuf)
	{
		if (pBuf)
		{
			pBuf->PlayloadSize() = 0;
			if (pBuf->Size() <= s1)
			{
				if (m_q1.push(pBuf))
				{
					pBuf = nullptr;
				}
			}
			else if (pBuf->Size() <= s2)
			{
				if (m_q2.push(pBuf))
				{
					pBuf = nullptr;
				}
			}
			else
			{
				if (m_q3.push(pBuf))
				{
					pBuf = nullptr;
				}
			}

			if (pBuf)
			{
				LOG() << "free buf " << pBuf->Size() << " s1:" << s1 << " s2:" << s2;
				delete pBuf;
				pBuf = nullptr;
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
	boost::lockfree::queue<Buf*, 
		boost::lockfree::capacity<32>, 
		boost::lockfree::fixed_sized<true>> m_q1, m_q2, m_q3;
};