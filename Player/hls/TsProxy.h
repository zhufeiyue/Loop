#pragma once
#include "HLSProxy.h"
#include "SimpleHttpClient.h"

class TsProxy : public QObject
{
public:
	TsProxy(std::string);
	virtual~TsProxy();
	std::string GetProxyAddress() const;

protected:
	virtual int HandleTsRequest(HttpConnectionPtr);

protected:
	std::string m_strTsAddress;
	std::string m_strTsProxyName;
	std::string m_strTsProxyAddress;
};

class TsDownloadProxy : public TsProxy
{
	Q_OBJECT
public:
	TsDownloadProxy(std::string);
	~TsDownloadProxy();

protected:
	virtual int HandleTsRequest(HttpConnectionPtr) override;
	virtual int DownloadFinish(Dic&);
	virtual int DownloadError(Dic&);
	virtual int DownloadProgress(QByteArray&, Dic&);

protected:
	HttpDownload* m_pHttpDownloader = nullptr;
	int64_t    m_iTotalSize = 0;
	bool       m_bDownloadFinish = false;
	QByteArray m_data;

	std::chrono::steady_clock::time_point m_timeDownloadStart;
};

class TsDownloadStreamProxy : public TsDownloadProxy
{
	Q_OBJECT
public:
	TsDownloadStreamProxy(std::string);
	~TsDownloadStreamProxy();

protected:
	int HandleTsRequest(HttpConnectionPtr) override;
	int DownloadFinish(Dic&) override;
	int DownloadError(Dic&) override;
	int DownloadProgress(QByteArray&, Dic&) override;

protected:
	std::vector<HttpConnectionPtr> m_conns;
};