#include "AsioHttp.h"
#include "log.h"
#include "ParseUrl.h"
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;

void testAsioHttp()
{
	Eventloop loop;
	auto thread = std::thread([&loop]() {
		loop.Run();
		});

	auto dataCb = [](std::string_view data, Dictionary info) {
		LOG() << "got message";

		std::ofstream fileOut;
		fileOut.open("d:/1.txt", std::ofstream::binary | std::ofstream::out);
		if (fileOut.is_open())
		{
			fileOut.write(data.data(), data.size());
			fileOut.close();
		}
	};
	auto errorCb = [](Dictionary info) {
		 LOG() << info.find("message")->second.to<std::string>();
	};

	//auto pClient = std::make_shared<AsioHttpClient>();
	auto pClient = std::make_shared<AsioHttpFile>();
	auto pFile = new std::ofstream("d:/1.txt", std::ofstream::binary | std::ofstream::out);
	pClient->SetProgressCb([pFile](std::string_view data, bool bDone, Dictionary dic) 
		{
			if (!pFile->is_open())
			{
				return;
			}

			LOG() << "read " << data.length() << " done " << bDone;
			pFile->write(data.data(), data.length());
			if (bDone)
			{
				pFile->close();
			}
		});

	//pClient->Get("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048", dataCb, errorCb);
	//pClient->Get("https://res06.bignox.com/full/20220301/8cbb8a56e4ae46b0bb069d19ad1a7723.exe?filename=nox_setup_v7.0.2.2_full.exe", dataCb, errorCb);
	//pClient->Get("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865", dataCb, errorCb);
	pClient->Get("https://cdnhp17.yesky.com/622465e5/6f7b262c103dc7d8d92ea8882d88c911/newsoft/3__5000557__3f7372633d6c6d266c733d6e32393464323930613961__68616f2e3336302e636e__0c6b.exe", dataCb, errorCb);
	pClient.reset();

	thread.join();
}

AsioHttpClient::AsioHttpClient()
{
	m_responBuf.reserve(512 * 1024); // 512kb
}

AsioHttpClient::~AsioHttpClient()
{
	Abort();
}

int AsioHttpClient::Get(std::string url, DataCb dataCb, ErrorCb errorCb)
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

	auto pThis = shared_from_this();
	auto pResolver = std::make_shared<asio::ip::tcp::resolver>(GetLoop().AsioQueue().Context());
	if (!pResolver)
	{
		return CodeOK;
	}

	pResolver->async_resolve(host, port != 0 ? std::to_string(port) : scheme,
		[pThis, pResolver](const system::error_code& err, asio::ip::tcp::resolver::results_type res)
		{
			pThis->OnResolver(err, res);
		});

	m_strUrl = url;
	m_strHost = host + (port != 0 ? ":" + std::to_string(port) : "");
	m_strScheme = scheme;
	m_strPath = path;
	m_cbData = std::move(dataCb);
	m_cbError = std::move(errorCb);

	return CodeOK;
}

int AsioHttpClient::Abort()
{
	system::error_code err;

	if (m_pSocket)
	{
		m_pSocket->cancel(err);
		if (err)
		{
			LOG() << "tcp socket cancel:" << err.message();
		}

		m_pSocket->close(err);
		m_pSocket.reset();
	}
	else if (m_pSslSocket)
	{
		m_pSslSocket->lowest_layer().cancel(err);
		if (err)
		{
			LOG() << "ssl::stream lowest_layer cancel:" << err.message();
		}

		m_pSslSocket->shutdown(err);
		if (err)
		{
			if (err != asio::ssl::error::stream_truncated)
				LOG() << "ssl::stream shutdown:" << err.message();
		}

		m_pSslSocket->lowest_layer().close(err);
		m_pSslSocket.reset();
	}
	if (err)
	{
		LOG() << __FUNCTION__" close socket " << err.message();
	}

	m_iContentLength = 0;
	m_iContentOffset = 0;
	m_iContentReaded = 0;
	m_responBuf.clear();
	m_strResponCode.clear();
	m_strResponReason.clear();
	m_strResponVersion.clear();
	m_mapResponHeaders.clear();

	return CodeOK;
}

int64_t AsioHttpClient::ContentLength()
{
	auto iter = m_mapResponHeaders.find("content-length");
	if (iter != m_mapResponHeaders.end())
	{
		auto& temp = iter->second;
		auto res = std::atoll(temp.c_str());
		return res;
	}

	return 0;
}

void AsioHttpClient::OnResolver(const boost::system::error_code& err, boost::asio::ip::tcp::resolver::results_type ep)
{
	if (err)
	{
		OnError(err);
		return;
	}

	if (m_strScheme == "http")
	{
		m_pSocket.reset(new asio::ip::tcp::socket(GetLoop().AsioQueue().Context()));
	}
	else if (m_strScheme == "https")
	{
		auto context = asio::ssl::context(asio::ssl::context::tlsv12_client);
		system::error_code err;
		context.set_default_verify_paths(err);
		if (err)
		{
			OnError(err);
			return;
		}

		m_pSslSocket.reset(
			new asio::ssl::stream<boost::asio::ip::tcp::socket>(GetLoop().AsioQueue().Context(), context)
		);
		m_pSslSocket->set_verify_mode(asio::ssl::verify_none);

		// 如果不调用下面这句，在某些网站上，可能会握手失败
		SSL_set_tlsext_host_name(m_pSslSocket->native_handle(), m_strHost.c_str());
	}
	else
	{
		return;
	}

	if (m_pSocket)
	{
		m_pSocket->async_connect(*ep,
			[pThis = shared_from_this()](const boost::system::error_code& err)
		{
			pThis->OnConnect(err);
		});
	}
	else if (m_pSslSocket)
	{
		m_pSslSocket->lowest_layer().async_connect(*ep,
			[pThis = shared_from_this()](const boost::system::error_code& err)
		{
			pThis->OnConnect(err);
		});
	}
}

void AsioHttpClient::OnConnect(const boost::system::error_code& err)
{
	if (err)
	{
		OnError(err);
		return;
	}

	if (m_pSocket)
	{
		DoRequest();
	}
	else if(m_pSslSocket)
	{
		m_pSslSocket->async_handshake(asio::ssl::stream<asio::ip::tcp::socket>::client, 
			[pThis = shared_from_this()](const boost::system::error_code& err) 
		{
			if (err)
			{
				pThis->OnError(err);
				return;
			}

			pThis->DoRequest();
		});
	}
}

void AsioHttpClient::OnReadHeader(const boost::system::error_code& err, std::size_t header_len)
{
	if (err)
	{
		OnError(err);
		return;
	}

	auto header = std::string_view(m_responBuf.c_str(), header_len);
	if (CodeOK != ParseHeader(header))
	{
		OnError(system::error_code(
			AsioHttpClient::parse_response_header_error,
			AsioHttpClient::custom_error_category()));
		return;
	}

	auto code = std::atoi(m_strResponCode.c_str());
	if (code == 301 || code == 302)
	{
		auto iter = m_mapResponHeaders.find("location");
		if (iter != m_mapResponHeaders.end())
		{
			auto strUrl = iter->second;
			LOG() << "redirect to " << strUrl;

			Abort();
			Get(strUrl, m_cbData, m_cbError);
			return;
		}
	}

	m_iContentLength = ContentLength();
	m_iContentReaded = 0;
	m_iContentOffset = header_len;

	auto body_readed = m_responBuf.length() - header_len;
	OnReadBody(err, body_readed);
}

void AsioHttpClient::OnReadBody(const boost::system::error_code& err, std::size_t)
{
	auto header_len = m_iContentOffset;
	auto body_len = m_iContentLength;
	if (body_len == 0)
	{
		OnError(system::error_code(
			AsioHttpClient::no_content_length,
			AsioHttpClient::custom_error_category()));
		return;
	}
	if (body_len > 8 * 1024 * 1024)
	{
		OnError(system::error_code(
			AsioHttpClient::content_length_more_than_8M,
			AsioHttpClient::custom_error_category()));
		return;
	}

	auto notify_complete_message = [this, header_len, body_len]() {
		if (m_cbData)
		{
			auto message = std::string_view(m_responBuf.data() + header_len, body_len);

			Dictionary other_info;
			other_info.insert("result", std::atoi(m_strResponCode.c_str()));
			other_info.insert("url", m_strUrl);

			m_cbData(message, other_info);
		}
	};

	size_t body_wait_read = body_len - (m_responBuf.length() - header_len);
	if (body_wait_read <= 0)
	{
		notify_complete_message();
	}
	else
	{
		auto readBodyCb = [pThis = shared_from_this(), notify_complete_message](const system::error_code& err, std::size_t)
		{
			if (err)
			{
				pThis->OnError(err);
				return;
			}

			notify_complete_message();
		};
		if (m_pSslSocket)
			asio::async_read(*m_pSslSocket,
				asio::dynamic_buffer(m_responBuf),
				asio::transfer_at_least(body_wait_read),
				std::move(readBodyCb));
		else if (m_pSocket)
			asio::async_read(*m_pSocket,
				asio::dynamic_buffer(m_responBuf),
				asio::transfer_at_least(body_wait_read),
				std::move(readBodyCb));
	}
}

void AsioHttpClient::OnError(const boost::system::error_code& err)
{
	if (m_cbError)
	{
		Dictionary dic;
		dic.insert("message", err.message());
		dic.insert("url", m_strUrl);

		m_cbError(dic);
		return;
	}

	if (err)
	{
		LOG() << err.message();
	}
}

void AsioHttpClient::DoRequest()
{
	std::stringstream ss;

	ss << "GET" << ' ' << m_strPath << ' ' << "HTTP/1.1" << "\r\n";
	ss << "Host: " << m_strHost << "\r\n";
	ss << "Connection: keep-alive" << "\r\n";
	ss << "User-Agent: Mozilla / 5.0 (Windows NT 10.0; Win64; x64; rv:97.0) Gecko / 20100101 Firefox / 97.0" << "\r\n";
	ss << "Accept: text/plain, */*; q=0.01" << "\r\n";
	ss << "Accept-Language: zh-CN,zh;q=0.8,zh-TW;q=0.7,zh-HK;q=0.5,en-US;q=0.3,en;q=0.2" << "\r\n";
	ss << "\r\n";

	auto pRequestData = std::make_shared<std::string>(ss.str());
	auto requestBuf = asio::buffer(pRequestData->data(), pRequestData->length());
	auto requestLenAtLeast = asio::transfer_at_least(pRequestData->length());
	auto requestCb = [pRequestData, pThis = shared_from_this(), this](const system::error_code& err, std::size_t n)
	{
		if (err)
		{
			OnError(err);
			return;
		}

		// read http respon header
		auto readHeaderCb = [pThis = shared_from_this(), this](const system::error_code& err, std::size_t header_len)
		{
			OnReadHeader(err, header_len);
		};

		auto buf = asio::dynamic_buffer(m_responBuf);
		if (m_pSslSocket)
		{
			asio::async_read_until(*m_pSslSocket, buf, "\r\n\r\n", std::move(readHeaderCb));
		}
		else if (m_pSocket)
		{
			asio::async_read_until(*m_pSocket, buf, "\r\n\r\n", std::move(readHeaderCb));
		}
	};

	if (m_pSocket)
	{
		asio::async_write(*m_pSocket, requestBuf, requestLenAtLeast, std::move(requestCb));
	}
	else if (m_pSslSocket)
	{
		asio::async_write(*m_pSslSocket, requestBuf, requestLenAtLeast, std::move(requestCb));
	}
}

int AsioHttpClient::ParseHeader(const std::string_view& strData)
{
	size_t prePos(0), pos(strData.find("\r\n"));
	std::string_view line;

	while (pos != std::string_view::npos)
	{
		line = std::string_view(strData.data() + prePos, pos - prePos);
		if (line.empty())
		{
			break;
		}

		if (prePos == 0)
		{
			// first line. respon line
			std::vector<std::string> parts;
			boost::split(parts, std::string(line), boost::is_any_of(" "), boost::token_compress_on);
			if (!parts.empty())
				m_strResponVersion = parts[0];
			if (parts.size() > 1)
				m_strResponCode = parts[1];
			if (parts.size() > 2)
				m_strResponReason = parts[2];
		}
		else
		{
			// respon header
			size_t colonPos = line.find(':');
			std::string header, value;
			if (colonPos > 0 && colonPos < line.length() - 1)
			{
				header = std::string(line.data(), colonPos);
				value = std::string(line.data() + colonPos + 1, line.length() - colonPos - 1);

				boost::to_lower(header);
				boost::trim_left(value);

				m_mapResponHeaders.insert(std::make_pair(std::move(header), std::move(value)));
			}
			else
			{
				return CodeNo;
			}
		}

		pos += 2;
		prePos = pos;
		pos = strData.find("\r\n", prePos);
	}

	return CodeOK;
}


void AsioHttpFile::OnReadBody(const boost::system::error_code& err, std::size_t readed_len)
{
	bool bDone = false;

	if (err)
	{
		if (err == asio::error::eof)
		{
			bDone = true;
		}
		else
		{
			OnError(err);
			return;
		}
	}

	auto readedData = std::string_view(m_responBuf.data() + m_iContentOffset, readed_len);
	m_iContentReaded += readed_len;
	if (m_iContentReaded >= m_iContentLength)
	{
		bDone = true;
	}

	if (m_cbProgress)
	{
		Dictionary dic;
		dic.insert("result", m_strResponCode);
		dic.insert("content-length", m_iContentLength);
		dic.insert("url", m_strUrl);

		m_cbProgress(std::move(readedData), bDone, dic);
	}

	if (bDone)
	{
		return;
	}

	m_iContentOffset = 0;
	m_responBuf.clear();

	if (m_pSocket)
	{
		asio::async_read(*m_pSocket, asio::dynamic_buffer(m_responBuf), asio::transfer_at_least(1),
			[pThis = shared_from_this(), this](const system::error_code& err, std::size_t readedLen)
		{
			OnReadBody(err, readedLen);
		});
	}
	else if (m_pSslSocket)
	{
		asio::async_read(*m_pSslSocket, asio::dynamic_buffer(m_responBuf), asio::transfer_at_least(1),
			[pThis = shared_from_this(), this](const system::error_code& err, std::size_t readedLen)
		{
			OnReadBody(err, readedLen);
		});
	}
}