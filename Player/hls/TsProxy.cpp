#include "TsProxy.h"
#include "Dic.h"
#include "Monitor.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDateTime>

static std::map<std::string, TsProxy*> mapTsProxy;

int StartTsProxy(
	const std::string& strOriginAddress, 
	const std::string& strSessionId,
	double tsDuration,
	std::string& strProxyAddress)
{
	TsProxy* pTsProxy = nullptr;
	//pTsProxy = new TsProxy(strOriginAddress);
	//pTsProxy = new TsDownloadProxy(strOriginAddress);
	pTsProxy = new TsDownloadStreamProxy(strOriginAddress);

	pTsProxy->SetSessionId(strSessionId);
	pTsProxy->SetTsDuration(tsDuration);

	strProxyAddress = pTsProxy->GetProxyAddress();
	if (strProxyAddress.empty())
	{
		qDebug() << "create ts proxy fail. origin ts address is " << strOriginAddress.c_str();
		delete pTsProxy;
		return -1;
	}

	mapTsProxy[strProxyAddress] = pTsProxy;

	return 0;
}
int StopTsProxy(const std::string& strProxyAddress)
{
	auto iter = mapTsProxy.find(strProxyAddress);
	if (iter == mapTsProxy.end())
	{
		qDebug() << "no such ts proxy " << strProxyAddress.c_str();
		return -1;
	}

	delete iter->second;
	mapTsProxy.erase(iter);

	return 0;
}

TsProxy::TsProxy(std::string strTsAddress)
{
	m_strTsAddress = std::move(strTsAddress);
	m_strTsProxyAddress = m_strTsAddress;

	int ret = -1;
	std::shared_ptr<SimpleHttpServer> pServer;
	ret = GetServer(pServer);
	if (ret != 0)
	{
		return;
	}

	QString strTemp;
	QUrl url(m_strTsAddress.c_str());
	if (!url.isValid())
	{
		return;
	}
	strTemp = url.path();

	m_strTsProxyName = strTemp.toStdString();
	ret = pServer->RegisterRouter(m_strTsProxyName, [this](HttpConnectionPtr p)
		{
			return HandleTsRequest(std::move(p));
		});
	if (ret != 0)
	{
		return;
	}
	m_strTsProxyAddress = "http://127.0.0.1:" + std::to_string(pServer->Port()) + m_strTsProxyName;
}

TsProxy::~TsProxy()
{
	if (!m_strTsProxyName.empty())
	{
		std::shared_ptr<SimpleHttpServer> pServer;
		GetServer(pServer);
		if (pServer)
		{
			pServer->UnRegisterRouter(m_strTsProxyName);
		}
	}

}

void TsProxy::SetSessionId(std::string strSessionId)
{
	m_strSessionId = std::move(strSessionId);
}

void TsProxy::SetTsDuration(double d)
{
	m_dTsDuration = d;
}

std::string TsProxy::GetProxyAddress() const
{
	return m_strTsProxyAddress;
}

int TsProxy::HandleTsRequest(HttpConnectionPtr pConn)
{
	pConn->SetResponHeader("location", m_strTsAddress);
	pConn->Send(std::string("301"), 301);
	return 0;
}


TsDownloadProxy::TsDownloadProxy(std::string s) : TsProxy(s)
{
	if (m_strTsProxyAddress.empty())
	{
		return;
	}

	qDebug() << "download " << s.c_str() << " --> " << m_strTsProxyAddress.c_str();
	m_timeDownloadStart = std::chrono::steady_clock::now();

	static HttpRequestManager manager;
	m_pHttpDownloader = new HttpDownload(manager.Get(QString::fromStdString(s)),
		[this](QByteArray data, Dic dic){ return DownloadProgress(data, dic);},
		[this](Dic dic) { DownloadFinish(dic); },
		[this](Dic dic){ DownloadError(dic); }
		);
}

TsDownloadProxy::~TsDownloadProxy()
{
	m_iTotalSize = -1;
	m_data = QByteArray();

	if (m_pHttpDownloader)
	{
		delete m_pHttpDownloader;
		m_pHttpDownloader = nullptr;
	}
}

int TsDownloadProxy::DownloadFinish(Dic& dic)
{
	m_bDownloadFinish = true;
	if (m_bDownloadError)
	{
		HttpRequestErrorInfo info;
		info.strErrorMsaage = m_strErrorMsg;
		info.strTime = QDateTime::currentDateTime().toString("yyyy.MM.dd hh:mm:ss").toStdString();
		info.strUrl = dic.get<std::string>("httpUrl");
		info.httpResponCode = dic.get<int>("httpCode");
		if (m_data.size() < 1024)
		{
			info.strHttpResponData = m_data.toStdString();
		}

		RecordHttpRequestErrorInfo(m_strSessionId, info);

		return 0;
	}

	auto now = std::chrono::steady_clock::now();
	auto downloadTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_timeDownloadStart).count();
	auto n = m_dTsDuration * 1000 / downloadTime;
	qDebug() << "download finish:"
		<< "ts size " << m_data.size()
		<< "ts duration " << m_dTsDuration * 1000
		<< "download time " << downloadTime
		<< n
		<< m_strTsAddress.c_str();

	TsDownloadRecord record;
	record.originAddress = m_strTsAddress;
	record.proxyAddress = m_strTsProxyAddress;
	record.tsDuration = m_dTsDuration * 1000;
	record.tsSize = m_data.size();
	record.tsDownloadTime = downloadTime;

	RecordTsDownloadInfo(m_strSessionId, record);

	return 0;
}

int TsDownloadProxy::DownloadError(Dic& dic)
{
	m_bDownloadError = true;
	m_strErrorMsg = dic.get<std::string>("errorMessage");
	return 0;
}

int TsDownloadProxy::DownloadProgress(QByteArray& data, Dic& dic)
{
	m_data.append(data);
	m_iTotalSize = dic.get<int64_t>("totalSize");
	return 0;
}

int TsDownloadProxy::HandleTsRequest(HttpConnectionPtr pConn)
{
	if (m_bDownloadError)
	{
		if (m_pHttpDownloader)
		{
			delete m_pHttpDownloader;
			m_pHttpDownloader = nullptr;
		}
		return TsProxy::HandleTsRequest(pConn);
	}

	if (!m_bDownloadFinish)
	{
		if (m_pHttpDownloader)
		{
			delete m_pHttpDownloader;
			m_pHttpDownloader = nullptr;
		}
		return TsProxy::HandleTsRequest(pConn);
	}

	pConn->SetResponHeader("Content-Type", "video/mp2t");
	pConn->Send(m_data, 200);

	return 0;
}


TsDownloadStreamProxy::TsDownloadStreamProxy(std::string s): TsDownloadProxy(s)
{
}

TsDownloadStreamProxy::~TsDownloadStreamProxy()
{
}

int TsDownloadStreamProxy::HandleTsRequest(HttpConnectionPtr p)
{
	qDebug() << "request " << m_strTsProxyAddress.c_str();

	if (m_bDownloadError)
	{
		return TsDownloadProxy::HandleTsRequest(p);
	}

	if (m_iTotalSize <= 0)
	{
		qDebug() << "dont know ts size . yet";
		return TsDownloadProxy::HandleTsRequest(p);
	}

	if (m_bDownloadFinish)
	{
		p->SetResponHeader("Content-Type", "video/mp2t");
		p->Send(m_data);
	}
	else
	{
		qDebug() << "ts download not finish";
		if (0 == p->SendPart(m_data, m_iTotalSize, true))
		{
			m_conns.push_back(p);
		}
		else
		{
			qDebug() << "send part fail";
		}
	}

	return 0;
}

int TsDownloadStreamProxy::DownloadFinish(Dic& dic)
{
	m_conns.clear();
	return TsDownloadProxy::DownloadFinish(dic);
}

int TsDownloadStreamProxy::DownloadError(Dic& dic)
{
	for (size_t i = 0; i < m_conns.size(); ++i)
	{
		m_conns[i]->Terminate();
	}
	m_conns.clear();

	return TsDownloadProxy::DownloadError(dic);
}

int TsDownloadStreamProxy::DownloadProgress(QByteArray& data, Dic& dic)
{
	TsDownloadProxy::DownloadProgress(data, dic);

	if (m_bDownloadError)
	{
		return -1;
	}

	for (auto iter = m_conns.begin(); iter != m_conns.end();)
	{
		if (0 != (*iter)->SendPart(data, m_iTotalSize, false))
		{
			iter = m_conns.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	return 0;
}