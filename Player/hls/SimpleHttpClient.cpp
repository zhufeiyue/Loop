#include "SimpleHttpClient.h"
#include "qloop.h"

#include <QApplication>
#include <QDebug>

void testHttpClient(int argc, char* argv[])
{
	QApplication app(argc, argv);

	if (true)
	{
		Dic result;
		//SimpleGet("http://112.74.200.9:88/tv000000/m3u8.php?/migu/627198191", result, 3000);
		SimpleGet("http://dys1.v.myalicdn.com/lhls/lhls_c20_2-llhls.m3u8?aliyunols=on", result, 3000);
		qDebug() << result.get<int>("code");
	}

	app.exec();
}

HttpRequestManager::HttpRequestManager()
{
	qDebug() << __FUNCTION__;
}

HttpRequestManager::~HttpRequestManager()
{
	qDebug() << __FUNCTION__;
}

QNetworkReply* HttpRequestManager::Get(QString url)
{
	QNetworkRequest req;
	req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	req.setUrl(url);
	req.setRawHeader("Cache-Control", "no-cache");

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
#ifdef _DEBUG
	qDebug() << __FUNCTION__;
#endif


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
				dic.insert("message", "http request error: " + dic.get<std::string>("errorMessage"));
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