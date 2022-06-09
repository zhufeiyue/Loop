#include "TsProxy.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>

static std::map<std::string, TsProxy*> mapTsProxy;

int StartTsProxy(const std::string& strOriginAddress, std::string& strProxyAddress)
{
	//auto pTsProxy = new TsProxy(strOriginAddress);
	//auto pTsProxy = new TsDownloadProxy(strOriginAddress);
	auto pTsProxy = new TsDownloadStreamProxy(strOriginAddress);
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

	static int64_t segNo = 0;
	QString strTemp = QString("/%1_%2.ts")
		.arg(QString(QCryptographicHash::hash(QByteArray(strTsAddress.c_str(), strTsAddress.length()), QCryptographicHash::Md5).toHex()))
		.arg(segNo++);
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

int TsDownloadProxy::DownloadFinish(Dic&)
{
	m_bDownloadFinish = true;
	qDebug() << "download finish";
	return 0;
}

int TsDownloadProxy::DownloadError(Dic& dic)
{
	m_iTotalSize = -1;
	m_data = QByteArray();
	return 0;
}

int TsDownloadProxy::DownloadProgress(QByteArray& data, Dic& dic)
{
	if (dic.get<int>("httpCode") != 200)
	{
		return -1;
	}

	m_data.append(data);
	m_iTotalSize = dic.get<int64_t>("totalSize");
	return 0;
}

int TsDownloadProxy::HandleTsRequest(HttpConnectionPtr pConn)
{
	if (m_iTotalSize < 0)
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

	if (m_iTotalSize < 0)
	{
		qDebug() << "dont know ts size . yet";
		return TsDownloadProxy::HandleTsRequest(p);
	}

	if (m_bDownloadFinish)
	{
		p->Send(m_data);
	}
	else
	{
		qDebug() << "ts download not finish";
		p->SendPart(m_data, m_iTotalSize, true);
		m_conns.push_back(p);
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
	if (0 != TsDownloadProxy::DownloadProgress(data, dic))
	{
		return -1;
	}

	for (size_t i = 0; i < m_conns.size(); ++i)
	{
		m_conns[i]->SendPart(data, m_iTotalSize, false);
	}

	return 0;
}