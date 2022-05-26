#include "AsioHttp.h"
#include "log.h"
#include "ParseUrl.h"
#include <boost/asio/ssl.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost;

static void testSyncAsioHttp()
{
	Dictionary result;
	//auto ret = SimpleHttpGet("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865", result, 5000);
	auto ret = SimpleHttpGet("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048", result, 5000);
	LOG() << result.get<std::string>("message");
}

void testAsioHttp()
{
	//testSyncAsioHttp();
	//return;

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
		LOG() << info.get<std::string>("message");
	};

	//auto pClient = std::make_shared<AsioHttpClient>();

	auto pClient = std::make_shared<AsioHttpClient1>();

	//auto pClient = std::make_shared<AsioHttpFile>();
	//auto pFile = new std::ofstream("d:/1.txt", std::ofstream::binary | std::ofstream::out);
	//pClient->SetProgressCb([pFile](std::string_view data, bool bDone, Dictionary dic)
	//	{
	//		if (!pFile->is_open())
	//		{
	//			return;
	//		}

	//		LOG() << "read " << data.length() << " done " << bDone;
	//		pFile->write(data.data(), data.length());
	//		if (bDone)
	//		{
	//			pFile->close();
	//		}
	//	});

	//pClient->Get("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048", dataCb, errorCb);
	//pClient->Get("http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865", dataCb, errorCb);
	//pClient->Get("https://cdnhp17.yesky.com/625e78ab/4cd89b23a17d8d735c9d7bd74b71dd97/newsoft/IQIYIsetup_tj%40kb002.exe", dataCb, errorCb);
	//pClient->Get("http://xiazai.qishucn.com/txt/%E4%B9%9D%E6%9E%81%E5%89%91%E7%A5%9E.txt", dataCb, errorCb);
	pClient->Get("https://cctvcncc.v.wscdns.com/live/cctv15_1/index.m3u8?contentid=2820180516001&b=800-2100", dataCb, errorCb);
	
	pClient.reset();

	std::this_thread::sleep_for(std::chrono::seconds(600));
}

class HttpThread
{
public:
	HttpThread()
	{
		m_thread = std::thread([this]() 
			{
				m_loop.Run();
			});
	}
	~HttpThread()
	{
		m_loop.Exit();
		if (m_thread.joinable())
		{
			m_thread.join();
		}
	}

	Eventloop& Loop() { return m_loop; }
private:
	std::thread m_thread;
	Eventloop   m_loop;
};

static Eventloop& HttpLoop()
{
	static HttpThread ins;
	return ins.Loop();
}

static asio::io_context& HttpIoContext()
{
	return HttpLoop().AsioQueue().Context();
}

AsioHttpClient::AsioHttpClient()
{
	m_responBuf.reserve(32 * 1024); // 32kb
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

	if (!m_pResolver)
	{
		m_pResolver = std::make_unique<asio::ip::tcp::resolver>(HttpIoContext());
	}
	if (!m_pResolver)
	{
		return CodeOK;
	}
	m_pResolver->async_resolve(host, port != 0 ? std::to_string(port) : scheme,
		[pThis = shared_from_this()](const system::error_code& err, asio::ip::tcp::resolver::results_type res)
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

	if (m_pResolver)
	{
		m_pResolver->cancel();
	}

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
		m_pSocket = std::make_unique<asio::ip::tcp::socket>(HttpIoContext());
	}
	else if (m_strScheme == "https")
	{
		auto ssl_context = asio::ssl::context(asio::ssl::context::tlsv12_client);
		system::error_code err;
		ssl_context.set_default_verify_paths(err);
		if (err)
		{
			OnError(err);
			return;
		}

		m_pSslSocket = std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(HttpIoContext(), ssl_context);
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
		DoSend();
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

			pThis->DoSend();
		});
	}
}

void AsioHttpClient::OnSendComplete(const boost::system::error_code& err, std::size_t n)
{
	if (err)
	{
		OnError(err);
		return;
	}

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

void AsioHttpClient::DoSend()
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
		OnSendComplete(err, n);
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


struct AsioHttpClient1Private
{
	bool bHeaderComplete = false;
	bool bMessageComplete = false;
	std::string strStatus;
	std::string strHeaderKey;
	std::string strHeaderValue;
	std::map<std::string, std::string> mapHeader;
	std::string strBody;
};

static int OnParserURL(http_parser* pParser, const char* at, size_t len)
{
	LOG() << "Asio http " __FUNCTION__;
	return 0;
}

static int OnParserStatus(http_parser* pParser, const char* at, size_t len)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}

	p->strStatus = std::string(at, len);

	return 0;
}

static int OnHeaderField(http_parser* pParser, const char* at, size_t len)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}
	p->strHeaderKey = std::string(at, len);
	boost::to_lower(p->strHeaderKey);

	return 0;
}

static int OnHeaderValue(http_parser* pParser, const char* at, size_t len)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}
	p->strHeaderValue = std::string(at, len);
	p->mapHeader.insert(std::make_pair(std::move(p->strHeaderKey), std::move(p->strHeaderValue)));

	return 0;
}

static int OnHeaderComplete(http_parser* pParser)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}

	p->bHeaderComplete = true;

	return 0;
}

static int OnMessageBegin(http_parser* pParser)
{
	return 0;
}

static int OnMessageComplete(http_parser* pParser)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}

	p->bMessageComplete = true;

	return 0;
}

/*
body是分段接收的，不一定一次性得到全部内容
chunk的内容同样走这个回调
*/
static int OnBody(http_parser* pParser, const char* at, size_t len)
{
	auto p = static_cast<AsioHttpClient1Private*>(pParser->data);
	if (!p)
	{
		return -1;
	}

	p->strBody.append(std::string(at, len));

	return 0;
}

static int OnChunkHeader(http_parser* pParser)
{
	LOG() << "chunk length " << pParser->content_length;
	return 0;
}

static int OnChunkComplete(http_parser* pParser)
{
	return 0;
}

AsioHttpClient1::AsioHttpClient1()
{
	http_parser_settings_init(&m_settings);
	m_settings.on_message_begin = OnMessageBegin;
	m_settings.on_message_complete = OnMessageComplete;
	m_settings.on_url = OnParserURL;
	m_settings.on_status = OnParserStatus;
	m_settings.on_header_field = OnHeaderField;
	m_settings.on_header_value = OnHeaderValue;
	m_settings.on_headers_complete = OnHeaderComplete;
	m_settings.on_body = OnBody;
	m_settings.on_chunk_header = OnChunkHeader;
	m_settings.on_chunk_complete = OnChunkComplete;

	m_pPrivate = new AsioHttpClient1Private;
	m_parser.data = m_pPrivate;
	http_parser_init(&m_parser, HTTP_RESPONSE);
	http_parser_set_max_header_size(2 * 1024);
}

AsioHttpClient1::~AsioHttpClient1()
{
	if (m_pPrivate)
		delete m_pPrivate;
}

void AsioHttpClient1::OnSendComplete(const boost::system::error_code& err, std::size_t)
{
	if (err)
	{
		OnError(err);
		return;
	}

	DoRead();
}

void AsioHttpClient1::DoRead()
{
	auto readCb = [pThis = shared_from_this(), this](const system::error_code& err, std::size_t len)
	{
		OnReadPart(err, len);
	};

	auto buf = asio::dynamic_buffer(m_responBuf);
	if (m_pSslSocket)
	{
		asio::async_read(*m_pSslSocket, buf, asio::transfer_at_least(1), std::move(readCb));
	}
	else if (m_pSocket)
	{
		asio::async_read(*m_pSocket, buf, asio::transfer_at_least(1), std::move(readCb));
	}
}

void AsioHttpClient1::OnReadPart(const boost::system::error_code& err, std::size_t len)
{
	if (err)
	{
		OnError(err);
		return;
	}

	size_t ret;
	ret = http_parser_execute(&m_parser, &m_settings, m_responBuf.c_str(), m_responBuf.length());
	if (http_errno::HPE_OK != m_parser.http_errno)
	{
		LOG() << http_errno_name((http_errno)m_parser.http_errno);

		OnError(system::error_code(AsioHttpClient::parse_response_error,
			AsioHttpClient::custom_error_category()));
		return;
	}

	if (m_pPrivate->bHeaderComplete)
	{
		m_strResponCode = std::to_string(m_parser.status_code);
		m_strResponReason = std::move(m_pPrivate->strStatus);
		m_strResponVersion = std::to_string(m_parser.http_major) + "/" + std::to_string(m_parser.http_minor);
		m_mapResponHeaders = std::move(m_pPrivate->mapHeader);

		m_pPrivate->bHeaderComplete = false;

		if (m_parser.status_code == 301 || m_parser.status_code == 302)
		{
			auto iter = m_mapResponHeaders.find("location");
			if (iter != m_mapResponHeaders.end())
			{
				auto strUrl = iter->second;
				LOG() << "redirect to " << strUrl;

				Abort();

				delete m_pPrivate;
				m_pPrivate = new AsioHttpClient1Private;
				m_parser.data = m_pPrivate;
				http_parser_init(&m_parser, HTTP_RESPONSE);

				Get(strUrl, m_cbData, m_cbError);
				return;
			}
		}
	}

	if (m_pPrivate->bMessageComplete)
	{
		if (m_cbData)
		{
			Dictionary other_info;
			other_info.insert("result", std::atoi(m_strResponCode.c_str()));
			other_info.insert("url", m_strUrl);

			m_cbData(m_pPrivate->strBody, other_info);
		}
	}
	else
	{
		if (ret == m_responBuf.length())
		{
			m_responBuf = std::string();
		}
		else if (ret < m_responBuf.length())
		{
			m_responBuf = m_responBuf.substr(ret);
		}
		else
		{
			OnError(system::error_code(AsioHttpClient::parse_response_error,
				AsioHttpClient::custom_error_category()));
			return;
		}

		DoRead();
	}
}

int SimpleHttpGet(std::string url, Dictionary& result, int timeout)
{
	auto pHttpClient = std::make_shared<AsioHttpClient1>();

	auto promise = std::make_shared<std::promise<int>>();
	auto fu =  promise->get_future();
	auto bValid = std::make_shared<std::atomic_bool>(true);

	auto dataCb = [&result, promise, bValid](std::string_view data_view, Dictionary info) {
		if (!bValid->load())
		{
			return;
		}
		bValid->store(false);

		auto data = std::string(data_view);
		auto trueUrl = info.get<std::string>("url");
		auto httpCode = info.get<int>("result");

		result.insert("ok", 1);
		result.insert("message", "ok");
		result.insert("status", httpCode);
		result.insert("data", std::move(data));
		result.insert("url", trueUrl);

		try
		{
			promise->set_value(CodeOK);
		}
		catch (const std::exception& e)
		{
			LOG() << e.what();
		}
		catch (...)
		{
		}
	};

	auto errorCb = [&result, promise, bValid](Dictionary info) {
		if (!bValid->load())
		{
			return;
		}
		bValid->store(false);

		auto strMessage = info.get<std::string>("message");
		result.insert("ok", 0);
		result.insert("message", std::move(strMessage));

		try
		{
			promise->set_value(CodeNo);
		}
		catch (const std::exception& e)
		{
			LOG() << e.what();
		}
		catch (...)
		{
		}
	};

	result.clear();

	auto ret =  pHttpClient->Get(url, std::move(dataCb), std::move(errorCb));
	if (ret != CodeOK)
	{
		result.insert("ok", 0);
		result.insert("message", url);
		return ret;
	}

	auto status = fu.wait_for(std::chrono::milliseconds(timeout));
	if (status == std::future_status::ready)
	{
		return fu.get();
	}
	else if (status == std::future_status::timeout)
	{
		*bValid = false;
		result.insert("ok", 0);
		result.insert("message", "time out");

		HttpLoop().AsioQueue().PushEvent([pHttpClient]() 
			{
				pHttpClient->Abort();
				return 0;
			});
	}
	else if (status == std::future_status::deferred)
	{
		// never here
	}

	return CodeNo;
}