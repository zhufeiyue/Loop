#pragma once
#include <algorithm>
#include <string>
#include <sstream>
#include <set>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <deque>

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QDateTime>
#include <QLocale>

#include "http_parser.h"

void testHttpServer(int, char**);

class SimpleHttpServer;
class HttpConnection : public QObject, public std::enable_shared_from_this<HttpConnection>
{
	//这个类是单线程的，所有操作都是串行的
	Q_OBJECT
public:
	struct string_case_cmp_less
	{
		bool operator()(const std::string& s1, const std::string& s2) const
		{
#ifdef _WIN32
			return _stricmp(s1.c_str(), s2.c_str()) < 0;
#else
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
#endif
		}
	};

public:
	HttpConnection(QTcpSocket*, SimpleHttpServer*);
	~HttpConnection();

	int                GetMethod() const;
	const std::string& GetURL() const { return m_strURL; }
	const std::string& GetQuery() const { return m_strQuery; }

	int Send(std::string&&, int responCode = 200);
	int Send(QByteArray, int responCode = 200);
	int SendPart(QByteArray, int64_t, bool firstPart, int responCode = 200);
	int SendFile(std::string, int responCode = 200);
	int SetResponHeader(std::string, std::string);
	int Terminate();

private:
	int DoSend();
	int DoSendData();

protected Q_SLOTS:
	void OnError(QAbstractSocket::SocketError);
	void OnDisconnect();
	void OnReadyRead();
	void OnWritten(qint64);

private:
	SimpleHttpServer*    m_pServer = nullptr;
	QTcpSocket*          m_pSocket = nullptr;
	http_parser          m_parser;
	http_parser_settings m_settings;
	QByteArray           m_recvBuf;
	
	std::map<std::string, std::string, string_case_cmp_less> m_mapResponHeader;
	std::deque<std::pair<QByteArray, int64_t>> m_sendQueue;
	int                  m_code = 500;
	bool                 m_bWriting = false;
	qint64               m_iResponWritten = 0;
	qint64               m_iResponHeaderLength = 0;
	qint64               m_iResponBodyLength = 0;

public:
	std::string m_strURL;
	std::string m_strQuery;
	bool m_bRequestHeaderDone = false;
	bool m_bRequestDone       = false;
	std::vector<std::pair<std::string, std::string>> m_vRequestHeader;
};

typedef std::shared_ptr<HttpConnection> HttpConnectionPtr;

class SimpleHttpServer : public QObject
{
	Q_OBJECT
public:
	typedef std::function<int(HttpConnectionPtr)> RouterCb;

public:
	SimpleHttpServer(QObject*);
	~SimpleHttpServer();
	int Start(uint16_t port = 0);
	int Stop();
	uint16_t Port();

	int RegisterRouter(std::string, RouterCb);
	int UnRegisterRouter(std::string);
	int Router(HttpConnectionPtr);

	int StartConnection(HttpConnectionPtr);
	int EndConnection(HttpConnectionPtr);

protected Q_SLOTS:
	void OnNewConnection();
	void OnAcceptError(QAbstractSocket::SocketError);

private:
	QTcpServer*                     m_pServer = nullptr;
	std::map<std::string, RouterCb> m_mapRouter;
	std::set<HttpConnectionPtr>     m_connections;
};