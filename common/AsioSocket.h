#pragma once
#include "EventLoop.h"
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

class UdpClient : public std::enable_shared_from_this<UdpClient>
{
public:
	UdpClient(Eventloop&);
	virtual ~UdpClient();
	int Init(std::string, int port);
	int Destroy();

	virtual void OnResolve(const boost::system::error_code& err, const boost::asio::ip::udp::endpoint& ep);
	virtual void OnConnect(const boost::system::error_code& err);

protected:
	Eventloop& m_loop;
	std::unique_ptr<boost::asio::ip::udp::resolver> m_pResolver;
	std::unique_ptr<boost::asio::ip::udp::socket> m_pSocket;
};

class HttpClient : public std::enable_shared_from_this<HttpClient>
{
public:
	HttpClient(Eventloop&);
	virtual ~HttpClient();
	int Get(std::string);
	int Abort();

	virtual void OnResolver(const boost::system::error_code&, boost::asio::ip::tcp::resolver::results_type);
	virtual void OnConnect(const boost::system::error_code&);
	virtual void OnWrite(const boost::system::error_code&, std::size_t);
	virtual void OnRead(const boost::system::error_code&, std::size_t);
	virtual void OnHandshake(const boost::system::error_code& error); // https
	virtual void DoRequest();

protected:
	Eventloop& m_loop;
	std::string m_strHost;
	std::string m_strPath;
	std::string m_strScheme;

	std::unique_ptr<boost::asio::ip::tcp::resolver> m_pResolver;
	std::unique_ptr<boost::beast::tcp_stream> m_pStreamBase;
	std::unique_ptr<boost::beast::ssl_stream<boost::beast::tcp_stream>> m_pSSLStreamBase;
	boost::asio::ssl::context m_ctx;

	boost::beast::http::response<boost::beast::http::dynamic_body> m_response;
	boost::beast::multi_buffer m_buffer;
	boost::beast::http::verb m_method;
};