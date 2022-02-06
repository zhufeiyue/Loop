#pragma once
#include <mutex>
#include <common/bufpool.h>
#include <common/AsioSocket.h>

class UdpTracker : public std::enable_shared_from_this<UdpTracker>
{
public:
	UdpTracker();
	~UdpTracker();

	int32_t GetTransactionId();

	int Init(std::string strTrackerURL);
	int Destroy();

	void OnResolve(const boost::system::error_code& err, const boost::asio::ip::udp::endpoint& ep);
	void OnRecvData(BufPtr buf, size_t size, boost::asio::ip::udp::endpoint endpoint);
	void OnError(const boost::system::error_code& err);

protected:
	enum AnnounceEvent
	{
		none = 0,
		complete,
		started,
		stopped
	};

	void SendConnectTracker();
	void SendAnnounceTracker();

protected:
	boost::asio::ip::udp::endpoint m_ep;
	std::string m_strTrackerURL;
	int32_t m_transaction_id = 0;
	int64_t m_connection_id = 0;
};

class UdpNet
{
public:
	UdpNet();
	~UdpNet();

	int ResolveAddress(std::shared_ptr<UdpTracker>, std::string ip, int port);
	int SendDataTo    (std::shared_ptr<UdpTracker>, BufPtr buf, const boost::asio::ip::udp::endpoint& ep);

	int Register(std::shared_ptr<UdpTracker> pTracker);
	int UnRegister(std::shared_ptr<UdpTracker> pTracker);

protected:
	void RecvData();
	void OnRecvData(BufPtr buf, size_t size, boost::asio::ip::udp::endpoint endpoint);

protected:
	boost::asio::ip::udp::socket* m_pSocket = nullptr;
	boost::asio::ip::udp::endpoint m_ep;
	std::map<int32_t, std::shared_ptr<UdpTracker>> m_mapTracker;
	std::mutex m_lock;
};