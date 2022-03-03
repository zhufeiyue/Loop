#pragma once
#include "Dic.h"
#include "EventLoop.h"

class UdpClient : public std::enable_shared_from_this<UdpClient>
{
public:
	explicit UdpClient(Eventloop&);
	virtual ~UdpClient();
	int Init(std::string, int port);
	int Destroy();

protected:
	virtual void OnResolve(const boost::system::error_code& err, const boost::asio::ip::udp::endpoint& ep);
	virtual void OnConnect(const boost::system::error_code& err);
	virtual void OnError(const boost::system::error_code& err);

protected:
	Eventloop&                                      m_loop;
	std::unique_ptr<boost::asio::ip::udp::resolver> m_pResolver;
	std::unique_ptr<boost::asio::ip::udp::socket>   m_pSocket;
};

#define UseBeast 0
#if UseBeast
#include <boost/beast.hpp>
namespace boost {
	namespace asio {
		namespace ssl {
			class context;
		}
	}

	namespace beast {
		template <typename T>
		class ssl_stream;
	}
}

class HttpClient : public std::enable_shared_from_this<HttpClient>
{
public:
	typedef std::function<void(Dictionary)> ErrorCallback;
	typedef std::function<void(std::string, Dictionary)> DataCallback;

public:
	explicit HttpClient(Eventloop&);
	virtual ~HttpClient();
	int Get(std::string, DataCallback, ErrorCallback);
	int Abort();

protected:
	virtual void OnResolver(const boost::system::error_code&, boost::asio::ip::tcp::resolver::results_type);
	virtual void OnConnect(const boost::system::error_code&);
	virtual void OnWrite(const boost::system::error_code&, std::size_t);
	virtual void OnRead(const boost::system::error_code&, std::size_t);
	virtual void OnHandshake(const boost::system::error_code& error); // https
	virtual void OnError(const boost::system::error_code&);
	virtual void DoRequest();

protected:
	Eventloop&  m_loop;
	std::string m_strHost;
	std::string m_strPath;
	std::string m_strScheme;
	std::string m_strUrl;

	DataCallback  m_cbData;
	ErrorCallback m_cbError;

	std::unique_ptr<boost::asio::ip::tcp::resolver> m_pResolver;
	std::unique_ptr<boost::beast::tcp_stream> m_pStreamBase;
	std::unique_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> m_pSSLStreamBase;
	std::unique_ptr<boost::asio::ssl::context> m_pSSLCtx;

	boost::beast::flat_buffer  m_buffer;
	boost::beast::http::verb   m_method;
private:
	// message-oriented interface
	boost::beast::http::response<boost::beast::http::dynamic_body> m_response;
};
#endif