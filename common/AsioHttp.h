#pragma once
#include "EventLoop.h"
#include "Dic.h"
#include "http_parser.h"

namespace boost {
	namespace asio {
		namespace ssl {
			class context;

			template <typename Stream>
			class stream;
		}
	}
}

class AsioHttpClient : public std::enable_shared_from_this<AsioHttpClient>
{
public:
	typedef std::function<void(std::string_view, Dictionary)> DataCb;
	typedef std::function<void(Dictionary)> ErrorCb;

	enum error
	{
		parse_response_error,
		parse_response_header_error,
		no_content_length,
		content_length_more_than_8M,
		header_length_more_than_1M
	};

	class custom_error_category : public boost::system::error_category
	{
	public:
		const char* name() const  BOOST_NOEXCEPT
		{
			return __FUNCTION__;
		}

		std::string message(int ev) const BOOST_NOEXCEPT
		{
			switch (ev)
			{
			case AsioHttpClient::parse_response_error:
				return "parse error";
			case AsioHttpClient::parse_response_header_error:
				return "parse header error";
			case AsioHttpClient::no_content_length:
				return "no content length in response header";
			case AsioHttpClient::content_length_more_than_8M:
				return  "body length > 8M";
			case AsioHttpClient::header_length_more_than_1M:
				return "haeder length > 1M";
			}

			return "custome message " + std::to_string(ev);
		}
	};

public:
	AsioHttpClient();
	virtual ~AsioHttpClient();

	int Get(std::string, DataCb, ErrorCb);
	int Abort();
	virtual int64_t ContentLength();

protected:
	void OnResolver(const boost::system::error_code&, boost::asio::ip::tcp::resolver::results_type);
	void OnConnect(const boost::system::error_code&);
	virtual void OnSendComplete(const boost::system::error_code&, std::size_t);
	virtual void OnReadHeader(const boost::system::error_code&, std::size_t);
	virtual void OnReadBody(const boost::system::error_code&, std::size_t);
	virtual void OnError(const boost::system::error_code&);

protected:
	virtual void DoSend();
	virtual int  ParseHeader(const std::string_view&);

protected:
	std::string m_strHost;
	std::string m_strPath;
	std::string m_strScheme;
	std::string m_strUrl;

	std::string m_strResponReason;
	std::string m_strResponVersion;
	std::string m_strResponCode;
	std::map<std::string, std::string> m_mapResponHeaders;

	std::string m_responBuf;
	std::unique_ptr<boost::asio::ip::tcp::resolver> m_pResolver;
	std::unique_ptr<boost::asio::ip::tcp::socket> m_pSocket;
	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_pSslSocket;

	int64_t m_iContentLength = 0;
	int64_t m_iContentReaded = 0;
	int64_t m_iContentOffset = 0;

	DataCb  m_cbData;
	ErrorCb m_cbError;
};

class AsioHttpFile : public AsioHttpClient
{
public:
	typedef std::function<void(std::string_view, bool, Dictionary)> ProgressCb;

public:
	void SetProgressCb(ProgressCb cb)
	{
		m_cbProgress = std::move(cb);
	}

protected:
	void OnReadBody(const boost::system::error_code&, std::size_t) override;

protected:
	ProgressCb m_cbProgress;
};

struct AsioHttpClient1Private;
class AsioHttpClient1 : public AsioHttpClient
{
public:
	AsioHttpClient1();
	~AsioHttpClient1();

protected:
	void OnSendComplete(const boost::system::error_code&, std::size_t) override;
	virtual void OnReadPart(const boost::system::error_code&, std::size_t);

protected:
	virtual void DoRead();

private:
	AsioHttpClient1Private* m_pPrivate = nullptr;
	http_parser          m_parser;
	http_parser_settings m_settings;
};

int SimpleHttpGet(std::string, Dictionary&, int timeout_ms = 8000);