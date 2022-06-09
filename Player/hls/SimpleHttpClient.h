#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include "Dic.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

class HttpRequestManager 
{
public:
	HttpRequestManager();
	~HttpRequestManager();

	QNetworkReply* Get(QString url);

private:
	QNetworkAccessManager m_netManager;
};

class HttpReply : public QObject
{
	Q_OBJECT
public:
	typedef std::function<void(Dic)> HttpReplyCallback;
public:
	HttpReply(QNetworkReply*, 
		HttpReplyCallback dataCb,
		HttpReplyCallback errorCb = HttpReplyCallback(),
		QObject* parent = nullptr);
	~HttpReply();

private:
	QNetworkReply*  m_pReply = nullptr;
	HttpReplyCallback m_dataCb;
	HttpReplyCallback m_errorCb;
};

int SimpleGet(QString, Dic&, int timeout = 5000);

class HttpDownload : public HttpReply
{
	Q_OBJECT
public:
	typedef std::function<int(QByteArray, Dic)> HttpDownloadProgressCallback;

public:
	HttpDownload(QNetworkReply*,
		HttpDownloadProgressCallback progressCb,
		HttpReply::HttpReplyCallback finishCb,
		HttpReply::HttpReplyCallback errorCb);

private:
	HttpDownloadProgressCallback m_downloadProgressCb;
};