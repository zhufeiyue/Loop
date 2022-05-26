#pragma once
#include <algorithm>
#include <string>
#include <sstream>
#include <functional>
#include <map>
#include <random>

#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QDateTime>
#include <QLocale>

#include <common/http_parser.h>

void testHttpServer(int, char**);

class SimpleHttpServer;
class HttpConnection : QObject
{
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
	int SendFile(std::string, int responCode = 200);
	int SetResponHeader(std::string, std::string);

private:
	int DoSend();

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
	std::string          m_strRespon;
	int                  m_code = 500;
	qint64               m_iWritten = 0;

public:
	std::string m_strURL;
	std::string m_strQuery;
	bool m_bRequestHeaderDone = false;
	bool m_bRequestDone       = false;
};

class SimpleHttpServer : public QObject
{
	Q_OBJECT
public:
	typedef std::function<int(HttpConnection&)> RouterCb;

public:
	SimpleHttpServer(QObject*);
	~SimpleHttpServer();
	int Start(uint16_t port = 0);
	int Stop();
	uint16_t Port();

	int RegisterRouter(std::string, RouterCb);
	int UnRegisterRouter(std::string);
	int Router(HttpConnection&);

protected Q_SLOTS:
	void OnNewConnection();
	void OnAcceptError(QAbstractSocket::SocketError);

private:
	QTcpServer*                     m_pServer = nullptr;
	std::map<std::string, RouterCb> m_mapRouter;
};