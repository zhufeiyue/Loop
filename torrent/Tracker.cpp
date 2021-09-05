#include "Tracker.h"
#include <common/ParseUrl.h>
#include <random>

static BufPool<1024, 2048>& GetBufPool()
{
	static BufPool<1024, 2048> gBufPool;
	return gBufPool;
}

static UdpNet& GetUdpNet()
{
	static UdpNet gUdpNet;
	return gUdpNet;
}

template<typename T>
static void WriteInt_n(const T value, int8_t* p)
{
	static_assert(std::is_integral<T>::value, "Integral required.");

	int8_t* ptemp = (int8_t*)&value;
	auto size = sizeof(value);
	for (decltype(size) i = 0; i < size; ++i)
	{
		p[i] = ptemp[size - 1 - i];
	}
}

template<typename T>
static void ReadInt_n(const int8_t* p, T& value)
{
	static_assert(std::is_integral<T>::value, "Integral required.");

	int8_t* ptemp = (int8_t*)&value;
	auto size = sizeof(value);
	for (decltype(size) i = 0; i < size; ++i)
	{
		ptemp[i] = p[size - 1 - i];
	}
}

static int32_t CreateRandom()
{
	std::random_device rd;
	std::mt19937 mt(rd());

	return  static_cast<int32_t>(mt());
}

UdpTracker::UdpTracker()
{
	m_transaction_id = CreateRandom();
}

UdpTracker::~UdpTracker()
{
}

int32_t UdpTracker::GetTransactionId()
{
	return m_transaction_id;
}

int UdpTracker::Init(std::string strURL)
{
	if (CodeOK != GetUdpNet().Register(shared_from_this()))
	{
		return  CodeNo;
	}

	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	m_strTrackerURL = std::move(strURL);
	if (!ParseUrl(m_strTrackerURL, scheme, host, path, port))
	{
		LOG() << "invalid url " << m_strTrackerURL;
		return CodeNo;
	}

	GetUdpNet().ResolveAddress(shared_from_this(), host, port);

	return CodeOK;
}

int UdpTracker::Destroy()
{
	GetUdpNet().UnRegister(shared_from_this());

	return CodeOK;
}

void UdpTracker::SendConnectTracker()
{
	auto sendBuf = GetBufPool().AllocAutoFreeBuf(1024);
	if (!sendBuf)
	{
		LOG() << "alloc buf fail";
		return;
	}
	auto pData = (int8_t*)sendBuf->Data();

	WriteInt_n<int64_t>(0x41727101980, pData);
	WriteInt_n<int32_t>(0, pData + 8); // action 0
	WriteInt_n<int32_t>(m_transaction_id, pData + 12);
	sendBuf->PlayloadSize() = 16;

	GetUdpNet().SendDataTo(shared_from_this(), std::move(sendBuf), m_ep);
}

void UdpTracker::SendAnnounceTracker()
{
}

void UdpTracker::OnResolve(const boost::system::error_code& err, const boost::asio::ip::udp::endpoint& ep)
{
	if (err)
	{
		OnError(err);
		return;
	}

	m_ep = ep;
	SendConnectTracker();
}

void UdpTracker::OnError(const boost::system::error_code& err)
{
	LOG() << err.message();
}

void UdpTracker::OnRecvData(BufPtr buf, size_t size, boost::asio::ip::udp::endpoint endpoint)
{
}

UdpNet::UdpNet()
{
	try
	{
		auto ep = boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0);
		m_pSocket = new boost::asio::ip::udp::socket(GetLoop().AsioQueue().Context(), ep);
	}
	catch (...)
	{
		if (m_pSocket)
		{
			delete m_pSocket;
			m_pSocket = nullptr;
		}
	}

	RecvData();
}

UdpNet::~UdpNet()
{
	if (m_pSocket)
	{
		if (m_pSocket->is_open())
		{
			m_pSocket->close();
		}

		delete m_pSocket;
	}
}

int UdpNet::ResolveAddress(std::shared_ptr<UdpTracker> tracker, std::string ip, int port)
{
	if (!tracker || ip.empty())
	{
		return CodeNo;
	}

	auto pResolver = new boost::asio::ip::udp::resolver(GetLoop().AsioQueue().Context());
	pResolver->async_resolve(ip, std::to_string(port),
		[pResolver, pTracker = std::move(tracker)](const boost::system::error_code& err, boost::asio::ip::udp::resolver::results_type res){
		delete pResolver;

		pTracker->OnResolve(err, err ? boost::asio::ip::udp::endpoint() : *res);
	});

	return CodeOK;
}

int UdpNet::SendDataTo(std::shared_ptr<UdpTracker> tracker, BufPtr buf, const boost::asio::ip::udp::endpoint& ep)
{
	if (!m_pSocket || !buf)
	{
		return CodeNo;
	}

	auto buffer = boost::asio::buffer(buf->DataConst(), buf->PlayloadSize());

	m_pSocket->async_send_to(buffer, ep,
		[pSendBuf = std::move(buf), pTracker = std::move(tracker)](const boost::system::error_code& error, std::size_t bytes_transferred) {

		if (error)
		{
			pTracker->OnError(error);
			return;
		}
	});

	return CodeOK;
}

int UdpNet::Register(std::shared_ptr<UdpTracker> pTracker)
{
	std::lock_guard<std::mutex> guard(m_lock);

	if (!pTracker)
	{
		return CodeNo;
	}

	auto iter = m_mapTracker.find(pTracker->GetTransactionId());
	if (iter == m_mapTracker.end())
	{
		m_mapTracker.insert(std::make_pair(pTracker->GetTransactionId(), pTracker));
		return CodeOK;
	}
	else
	{
		if (iter->second.get() == pTracker.get())
		{
			return CodeOK;
		}

		LOG() << "same transation id";
		return CodeNo;
	}
}

int UdpNet::UnRegister(std::shared_ptr<UdpTracker> pTracker)
{
	std::lock_guard<std::mutex> guard(m_lock);

	auto iter = m_mapTracker.find(pTracker->GetTransactionId());
	if (iter != m_mapTracker.end())
	{
		m_mapTracker.erase(iter);
	}

	return CodeOK;
}

void UdpNet::RecvData()
{
	if (!m_pSocket || !m_pSocket->is_open())
	{
		return;
	}

	auto data = GetBufPool().AllocAutoFreeBuf(8192);
	if (!data)
	{
		return;
	}
	data->PlayloadSize() = 0;
	auto buf = boost::asio::buffer(data->Data(), data->Size());
	
	m_pSocket->async_receive_from(buf, m_ep,
		[data, this](const boost::system::error_code& error, std::size_t bytes_transferred) {
			if (error)
			{
				LOG() << "recv data fail " << error.message();
				return;
			}

			this->OnRecvData(data, bytes_transferred, m_ep);
			this->RecvData();
		});
}

void UdpNet::OnRecvData(BufPtr buf, size_t size, boost::asio::ip::udp::endpoint endpoint)
{
	std::lock_guard<std::mutex> guard(m_lock);
	if (size < 16)
	{
		LOG() << "too small data";
		return;
	}

	int32_t action = 0;
	int32_t transationId = 0;
	auto pData = (int8_t*)buf->Data();

	ReadInt_n<int32_t>(pData, action);
	ReadInt_n<int32_t>(pData + 4, transationId);

	auto iter = m_mapTracker.find(transationId);
	if (iter != m_mapTracker.end())
	{
		iter->second->OnRecvData(std::move(buf), size, std::move(endpoint));
	}
}