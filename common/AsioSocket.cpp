
#include "AsioSocket.h"
#include <common/ParseUrl.h>
#include <boost/beast/ssl.hpp>

using namespace boost;

UdpClient::UdpClient(Eventloop& loop):
	m_loop(loop)
{
}

UdpClient::~UdpClient()
{
}

int UdpClient::Init(std::string ip, int port)
{
	if (m_pResolver)
	{
		return CodeNo;
	}

	auto pThis = shared_from_this();

	m_pResolver.reset(new asio::ip::udp::resolver(m_loop.AsioQueue().Context()));
	m_pResolver->async_resolve(ip, std::to_string(port),
		[pThis](const system::error_code& err, asio::ip::udp::resolver::results_type res) 
		{
			pThis->OnResolve(err, err ? asio::ip::udp::endpoint() : *res);
		});

	return CodeOK;
}

int UdpClient::Destroy()
{
	if (m_pResolver)
	{
		m_pResolver->cancel();
	}

	return CodeOK;
}


void UdpClient::OnResolve(const system::error_code& err, const asio::ip::udp::endpoint& ep)
{
	if (err)
	{
		LOG() << __FUNCTION__ << ' ' << err.message();
		OnError(err);
		return;
	}

	if (m_pSocket)
	{
		LOG() << __FUNCTION__ << ' ' << "socket is not empty";
		return;
	}
	m_pSocket.reset(new asio::ip::udp::socket(m_loop.AsioQueue().Context()));
	m_pSocket->async_connect(ep,
		[pThis = shared_from_this()](const boost::system::error_code& err)
		{
			pThis->OnConnect(err);
		});

	m_pResolver.reset();
}

void UdpClient::OnConnect(const boost::system::error_code& err)
{
	if (err)
	{
		LOG() << __FUNCTION__ << ' ' << err.message();
		OnError(err);
		return;
	}
}

void UdpClient::OnError(const boost::system::error_code& err)
{

}


HttpClient::HttpClient(Eventloop& loop):
	m_loop(loop),
	m_pSSLCtx(new asio::ssl::context(asio::ssl::context::tlsv12_client))
{
}

HttpClient::~HttpClient()
{
}

int HttpClient::Abort()
{
	if (m_pResolver)
	{
		m_pResolver->cancel();
	}
	if (m_pStreamBase && m_pStreamBase->socket().is_open())
	{
		m_pStreamBase->cancel();
		system::error_code serr;
		m_pStreamBase->socket().shutdown(asio::socket_base::shutdown_both, serr);
	}
	if (m_pSSLStreamBase)
	{
		m_pSSLStreamBase->shutdown();
	}

	auto size = m_buffer.size();
	if (size > 0)
	{
		m_buffer.consume(size);
	}

	m_response.swap(decltype(m_response)());

	return CodeOK;
}

int HttpClient::Get(std::string url)
{
	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	if (!ParseUrl(url, scheme, host, path, port))
	{
		LOG() << "invalid url " << url;
		return CodeNo;
	}

	if (m_pResolver)
	{
		return CodeNo;
	}

	m_pResolver.reset(new asio::ip::tcp::resolver(m_loop.AsioQueue().Context()));
	m_pResolver->async_resolve(host, scheme,
		[pThis = shared_from_this()](const system::error_code& err, asio::ip::tcp::resolver::results_type res)
	{
		pThis->OnResolver(err, res);
	});

	m_strHost = host;
	m_strScheme = scheme;
	m_strPath = path;
	m_method = beast::http::verb::get;

	return CodeOK;
}

void HttpClient::OnResolver(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::results_type ep)
{
	if (err)
	{
		LOG() << __FUNCTION__ << err.message();
		return;
	}

	m_pStreamBase.reset(new beast::tcp_stream(m_loop.AsioQueue().Context()));
	m_pStreamBase->expires_after(std::chrono::seconds(15));
	m_pStreamBase->async_connect(ep,
		[pThis = shared_from_this()](const boost::system::error_code& err, asio::ip::tcp::endpoint ep)
	{
		pThis->OnConnect(err);
	});
}

void HttpClient::OnConnect(const boost::system::error_code& err)
{
	if (err)
	{
		LOG() << __FUNCTION__ << err.message();
		return;
	}

	if (m_strScheme == "https")
	{
		m_pSSLStreamBase.reset(
			new beast::ssl_stream<beast::tcp_stream>(m_pStreamBase->release_socket(), *m_pSSLCtx)
		);
		m_pStreamBase.reset();

		//if (!SSL_set_tlsext_host_name(m_pSSLStreamBase->native_handle(), m_strHost.c_str()))
		//{
		//	std::cout << "SSL_set_tlsext_host_name" << std::endl;
		//	return;
		//}

		m_pSSLStreamBase->set_verify_callback([](bool p, asio::ssl::verify_context& ctx)->bool {
			char subject_name[256];
			X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
			X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

			return true;
			});

		m_pSSLStreamBase->async_handshake(asio::ssl::stream_base::client,
			[pThis = shared_from_this()](const boost::system::error_code& err)
		{
			pThis->OnHandshake(err);
		});
	}
	else
	{
		DoRequest();
	}
}

void HttpClient::OnWrite(const boost::system::error_code& err, std::size_t)
{
	if (err)
	{
		LOG() << __FUNCTION__ << err.message();
		return;
	}

	if (m_pSSLStreamBase)
	{
		beast::http::async_read(*m_pSSLStreamBase, m_buffer, m_response,
			[pThis = shared_from_this()](const boost::system::error_code& err, std::size_t n)
		{
			pThis->OnRead(err, n);
		});
	}
	else
	{
		beast::http::async_read(*m_pStreamBase, m_buffer, m_response,
			[pThis = shared_from_this()](const boost::system::error_code& err, std::size_t n)
		{
			pThis->OnRead(err, n);
		});
	}
}

void HttpClient::OnRead(const boost::system::error_code& err, std::size_t n)
{
	if (err)
	{
		LOG() << __FUNCTION__ << err.message();
		return;
	}

	try
	{
		auto result = m_response.result();
		auto type = m_response.base().at(beast::http::field::content_type).to_string();
		auto len = m_response.base().at(beast::http::field::content_length).to_string();
	}
	catch (...)
	{
	}

	auto s = beast::buffers_to_string(m_response.body().data());

	std::ofstream f;
	f.open("d:/1.html", std::ofstream::binary | std::ofstream::out);
	f.write(s.c_str(), s.length());
	f.close();
}

void HttpClient::OnHandshake(const boost::system::error_code& err)
{
	if (err)
	{
		LOG() << __FUNCTION__ << err.message();
		return;
	}

	DoRequest();
}

void HttpClient::DoRequest()
{
	auto pReq = std::make_shared<beast::http::request<beast::http::dynamic_body>>();
	pReq->version(11);
	//pReq->set(beast::http::field::connection, "keep-alive");
	//pReq->set(beast::http::field::accept_encoding, "gzip, deflate");
	pReq->set(beast::http::field::cache_control, "max-age=0");
	pReq->set(beast::http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9");
	pReq->set(beast::http::field::user_agent, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.182 Safari/537.36 Edg/88.0.705.81");
	pReq->set(beast::http::field::accept_language, "zh-CN,zh;q=0.9,en;q=0.8");
	pReq->set(beast::http::field::host, m_strHost);
	pReq->method(m_method);
	pReq->target(m_strPath);

	if (m_pSSLStreamBase)
	{
		beast::http::async_write(*m_pSSLStreamBase, *pReq,
			[pReq, pThis = shared_from_this()](const system::error_code& err, std::size_t n)
		{
			pThis->OnWrite(err, n);
		});
	}
	else
	{
		beast::http::async_write(*m_pStreamBase, *pReq,
			[pReq, pThis = shared_from_this()](const system::error_code& err, std::size_t n)
		{
			pThis->OnWrite(err, n);
		});
	}
}