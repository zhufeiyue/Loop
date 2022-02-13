
#include "AsioSocket.h"
#include "ParseUrl.h"
#include "log.h"
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
	m_loop(loop)
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
		beast::close_socket(*m_pStreamBase);
	}
	if (m_pSSLStreamBase)
	{
		beast::close_socket(m_pSSLStreamBase->next_layer());

		system::error_code ec;
		//m_pSSLStreamBase->shutdown(ec);
		if (ec)
		{
			LOG() <<  __FUNCTION__" " << ec.message();
		}
	}

	auto size = m_buffer.size();
	if (size > 0)
	{
		m_buffer.consume(size);
	}
	
	m_response.base().clear();
	m_response.body().clear();
	m_response.clear();

	return CodeOK;
}

int HttpClient::Get(std::string url, DataCallback dataCb, ErrorCallback errorCb)
{
	std::string host;
	std::string scheme;
	std::string path;
	int port(0);

	m_cbData = std::move(dataCb);
	m_cbError = std::move(errorCb);

	if (!ParseUrl(url, scheme, host, path, port))
	{
		LOG() << "invalid url " << url;
		return CodeNo;
	}

	if (!m_pResolver)
		m_pResolver.reset(new asio::ip::tcp::resolver(m_loop.AsioQueue().Context()));
	m_pResolver->async_resolve(host, port !=0 ? std::to_string(port) : scheme,
		[pThis = shared_from_this()](const system::error_code& err, asio::ip::tcp::resolver::results_type res){
		pThis->OnResolver(err, res);
	});

	m_strUrl = url;
	m_strHost = host + (port != 0 ? ":"+std::to_string(port) : "");
	m_strScheme = scheme;
	m_strPath = path;
	m_method = beast::http::verb::get;

	return CodeOK;
}

void HttpClient::OnResolver(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::results_type ep)
{
	if (err)
	{
		OnError(err);
		return;
	}

	m_pStreamBase.reset(new beast::tcp_stream(m_loop.AsioQueue().Context()));
	m_pStreamBase->expires_after(std::chrono::seconds(15));
	m_pStreamBase->async_connect(ep,
		[pThis = shared_from_this()](const boost::system::error_code& err, asio::ip::tcp::endpoint ep){
		pThis->OnConnect(err);
	});
}

void HttpClient::OnConnect(const boost::system::error_code& err)
{
	if (err)
	{
		OnError(err);
		return;
	}

	if (m_strScheme == "https")
	{
		m_pSSLCtx.reset(new asio::ssl::context(asio::ssl::context::tlsv12_client));
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
			[pThis = shared_from_this()](const boost::system::error_code& err){
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
		OnError(err);
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
		OnError(err);
		return;
	}

	auto result = m_response.result_int();
	if (result == 301 || result == 302)
	{
		auto iter = m_response.find(beast::http::field::location);
		if (iter != m_response.end())
		{
			auto strLocation = iter->value().to_string();
			Abort();
			Get(strLocation, m_cbData, m_cbError);
			return;
		}
	}

	auto s = beast::buffers_to_string(m_response.body().data());
	Dictionary dic;
	dic.insert("result", (int)result);
	dic.insert("url", m_strUrl);

	Abort();
	if (m_cbData)
	{
		m_cbData(std::move(s), std::move(dic));
	}
}

void HttpClient::OnHandshake(const boost::system::error_code& err)
{
	if (err)
	{
		OnError(err);
		return;
	}

	DoRequest();
}

void HttpClient::OnError(const boost::system::error_code& err)
{
	LOG() << __FUNCTION__ << err.message();

	Dictionary dic;
	dic.insert("message", err.message());

	if (m_cbError)
	{
		m_cbError(std::move(dic));
	}
}

void HttpClient::DoRequest()
{
	auto pReq = std::make_shared<beast::http::request<beast::http::dynamic_body>>();
	pReq->version(11);
	pReq->set(beast::http::field::connection, "close");
	pReq->set(beast::http::field::accept, "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8");
	pReq->set(beast::http::field::user_agent, "Mozilla/5.0 (Windows NT 6.1; Win64; x64; rv:95.0) Gecko/20100101 Firefox/95.0");
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