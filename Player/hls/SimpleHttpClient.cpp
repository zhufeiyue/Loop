#include "SimpleHttpClient.h"
#include "TsProxy.h"
#include "qloop.h"

#include <QApplication>
#include <QDebug>
#include <QNetworkProxy>

void testHttpClient(int argc, char* argv[])
{
	QApplication app(argc, argv);

	if (false)
	{
		Dic result;
		//SimpleGet("http://112.74.200.9:88/tv000000/m3u8.php?/migu/627198191", result, 3000);
		SimpleGet("http://dys1.v.myalicdn.com/lhls/lhls_c20_2-llhls.m3u8?aliyunols=on", result, 3000);
		qDebug() << result.get<int>("code");
	}

	HttpRequestManager manager;
	if (false)
	{
		auto pHttpDownload = new HttpDownload(
			//manager.Get("https://cdnhp17.yesky.com/629ef530/8460dd4a62622009bb10a38e833cf99b/newsoft/QQGame_5.19.57014.0_0_0_1080000167_0.exe"),
			manager.Get("https://cn.bing.com/search?form=MOZLBR&pc=MOZI&q=%E5%8C%97%E4%BA%AC+%E4%B8%AD%E9%A3%8E%E9%99%A9%E5%9C%B0%E5%8C%BA"),
			[](QByteArray data, Dic dic)
			{
				qDebug() << data.size() << " : " << dic.get<int64_t>("totalSize") << " : " << dic.get<int>("httpCode");
				return 0; 
			},
			[](Dic dic) 
			{
				qDebug() << dic.get<int>("httpCode");
			},
			[](Dic dic) 
			{
				qDebug() << dic.get<QString>("errorMessage");
			});
	}

	if (true)
	{
		//auto p = new TsDownloadStreamProxy("http://127.0.0.1/1.txt");
		auto p1 = new TsDownloadStreamProxy("https://www.csdn1.net/");
	}

	app.exec();
}

static QString strGlobalClientSid;
int RecordClientSid(QString s)
{
	strGlobalClientSid = s;
	return 0;
}

HttpRequestManager::HttpRequestManager()
{
	qDebug() << __FUNCTION__;
	m_netManager.setProxy(QNetworkProxy::NoProxy);
}

HttpRequestManager::~HttpRequestManager()
{
	qDebug() << __FUNCTION__;
}

QNetworkReply* HttpRequestManager::Get(QString url)
{
	QNetworkRequest req;
	req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	//req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
	req.setUrl(url);
	//req.setRawHeader("Cache-Control", "no-cache");
	req.setRawHeader("User-Agent", "PCCTV/5.1.1.0/Win7");
	if(!strGlobalClientSid.isEmpty())
		req.setRawHeader("clientsid", strGlobalClientSid.toUtf8());

	return m_netManager.get(req);
}

HttpReply::HttpReply(QNetworkReply* pReplay,
	HttpReplyCallback dataCb,
	HttpReplyCallback errorCb,
	QObject* parent):
	QObject(parent),
	m_pReply(pReplay),
	m_dataCb(std::move(dataCb)),
	m_errorCb(std::move(errorCb))
{
	if (!m_pReply)
	{
		qDebug() << "reply is nullptr";
		return;
	}

	QObject::connect(m_pReply, &QNetworkReply::finished, 
		this, [this]() 
		{
			auto data = m_pReply->readAll();

			if (m_dataCb)
			{
				Dic dic;
				dic.insert("httpCode", m_pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
				dic.insert("httpUrl", m_pReply->url().toString());
				dic.insert("httpData", data);

				QString cdnsip = "NV";
				QString cdncip = "NV";
				auto& headers = m_pReply->rawHeaderPairs();
				for (int i = 0; i < headers.size(); ++i)
				{
					if (headers[i].first == "cdnsip")
					{
						cdnsip = headers[i].second.trimmed();
 					}
					else if (headers[i].first == "cdncip")
					{
						cdncip = headers[i].second.trimmed();
					}
				}

				dic.insert("cdnsip", cdnsip);
				dic.insert("cdncip", cdncip);

				m_dataCb(std::move(dic));
			}
		});

	QObject::connect(m_pReply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
		this, [this](QNetworkReply::NetworkError e)
		{
			qDebug() << m_pReply->errorString();

			if (m_errorCb)
			{
				Dic dic;
				dic.insert("errorCode", (int)e);
				dic.insert("errorMessage", m_pReply->errorString());

				m_errorCb(std::move(dic));
			}
		});
}

HttpReply::~HttpReply()
{
//#ifdef _DEBUG
//	qDebug() << __FUNCTION__;
//#endif


	if (m_pReply)
	{
		QObject::disconnect(m_pReply, nullptr, this, nullptr);

		m_pReply->close();
		m_pReply->deleteLater();
		m_pReply = nullptr;
	}
}

int SimpleGet(QString url, Dic& result, int timeout)
{
#ifdef _DEBUG
	auto timeCalculate = std::chrono::steady_clock::now();
#endif
	static QLoop httpLoop;

	auto promise = std::make_shared<std::promise<int>>();
	auto fu = promise->get_future();
	auto bValid = std::make_shared<std::atomic_bool>(true);
	auto pClient = std::shared_ptr<HttpReply>();

	result.clear();
	httpLoop.PushEvent([promise, bValid, url, &pClient, &result]()
		{
			auto dataCb = [&result, promise, bValid](Dic dic)
			{
				if (!bValid->load())
				{
					return;
				}
				bValid->store(false);

				dic.insert("code", 0);
				dic.insert("message", "OK");
				result = std::move(dic);

				try
				{
					promise->set_value(0);
				}
				catch (const std::exception& e)
				{
					qDebug() << e.what();
				}
				catch (...)
				{
				}
			};
			auto errorCb = [&result, promise, bValid](Dic dic)
			{
				if (!bValid->load())
				{
					return;
				}
				bValid->store(false);

				dic.insert("code", 1);
				dic.insert("message", "http_request_error_" + dic.get<std::string>("errorMessage"));
				result = std::move(dic);

				try
				{
					promise->set_value(0);
				}
				catch (const std::exception& e)
				{
					qDebug() << e.what();
				}
				catch (...)
				{
				}
			};

			static HttpRequestManager manager;
			pClient = std::make_shared<HttpReply>(manager.Get(url), std::move(dataCb), std::move(errorCb));

			return 0;
		});

	auto status = fu.wait_for(std::chrono::milliseconds(timeout));
	if (status == std::future_status::ready)
	{
		fu.get();
	}
	else if (status == std::future_status::timeout)
	{
		*bValid = false;
		result.insert("code", 2);
		result.insert("message", QString("timeout(%1ms)").arg(timeout));
	}
	else if (status == std::future_status::deferred)
	{
	}

	httpLoop.PushEvent([pClient]()
		{
			return 0;
		});
	pClient.reset();

#ifdef _DEBUG
	qDebug() << "http request use " << 
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timeCalculate).count() << " ms";
#endif

	return 0;
}

HttpDownload::HttpDownload(
	QNetworkReply* pReply,
	HttpDownloadProgressCallback prgressCb,
	HttpReply::HttpReplyCallback finishCb,
	HttpReply::HttpReplyCallback errorCb) :
	HttpReply(pReply, finishCb, errorCb),
	m_downloadProgressCb(std::move(prgressCb))
{
	QObject::connect(pReply, &QNetworkReply::downloadProgress,
		this, [this, pReply](int64_t bytesReceived, int64_t bytesTotal)
		{
			auto data = pReply->readAll();
			if (m_downloadProgressCb)
			{
				Dic dic;
				dic.insert("httpCode", pReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
				dic.insert("httpUrl", pReply->url().toString());
				dic.insert("totalSize", bytesTotal);

				m_downloadProgressCb(data, dic);
			}
		});
}