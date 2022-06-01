#include "SimpleHttpServer.h"
#include <fstream>
#include <QApplication>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

void testHttpServer(int argc, char** argv)
{
	QApplication app(argc, argv);

	SimpleHttpServer* pServer = new SimpleHttpServer(nullptr);
	pServer->RegisterRouter("/", [](HttpConnection& conn) 
		{
			QString s =  QString::fromLocal8Bit("ÄãºÃ");
			auto ss = s.toUtf8().toStdString();
			conn.Send(ss + "hello");

			return 0;
		});

	pServer->RegisterRouter("/json", [](HttpConnection& conn)
		{
			QJsonObject obj;
			obj["result"] = true;
			obj["data"] = rand();

			conn.SetResponHeader("Content-Type", "application/json");
			conn.Send(QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString());

			return 0;
		});
	pServer->RegisterRouter("/1.jpg", [](HttpConnection& conn)
		{
			qDebug() << "request 1.jpg";
			conn.SendFile("d:/1.jpg");
			return 0;
		});
	pServer->Start(8000);

	auto ret = app.exec();
}


static int OnParserURL(http_parser* pParser, const char* at, size_t len)
{
	auto pHandler =  (HttpConnection*)pParser->data;

	if (pHandler)
	{
		pHandler->m_strURL = std::string(at, len);
	}

	return 0;
}

static int OnParserStatus(http_parser* pParser, const char* at, size_t len)
{
	return 0;
}

static int OnHeaderField(http_parser* pParser, const char* at, size_t len)
{
	return 0;
}

static int OnHeaderValue(http_parser* pParser, const char* at, size_t len)
{
	return 0;
}

static int OnHeaderComplete(http_parser* pParser)
{
	auto pConnection = static_cast<HttpConnection*>(pParser->data);
	if (pConnection)
	{
		pConnection->m_bRequestHeaderDone = true;
	}

	return 0;
}

static int OnMessageBegin(http_parser* pParser)
{
	return 0;
}

static int OnMessageComplete(http_parser* pParser)
{
	auto pConnection = static_cast<HttpConnection*>(pParser->data);
	if (pConnection)
	{
		pParser->http_errno;
		pConnection->m_bRequestDone = true;
	}

	return 0;
}

static int OnBody(http_parser* pParser, const char* at, size_t len)
{
	return 0;
}

HttpConnection::HttpConnection(QTcpSocket* pSocket, SimpleHttpServer* p):
	m_pServer(p),
	m_pSocket(pSocket)
{
	assert(m_pServer != nullptr);
#ifdef  _DEBUG
	qDebug() << __FUNCTION__;
#endif

	QObject::connect(pSocket, SIGNAL(error(QAbstractSocket::SocketError)), 
		this, SLOT(OnError(QAbstractSocket::SocketError)));
	QObject::connect(pSocket, SIGNAL(disconnected()),
		this, SLOT(OnDisconnect()));
	QObject::connect(pSocket, SIGNAL(readyRead()),
		this, SLOT(OnReadyRead()));
	QObject::connect(pSocket, SIGNAL(bytesWritten(qint64)),
		this, SLOT(OnWritten(qint64)));

	m_pSocket->setParent(this);


	http_parser_settings_init(&m_settings);
	m_settings.on_message_begin = OnMessageBegin;
	m_settings.on_message_complete = OnMessageComplete;
	m_settings.on_url = OnParserURL;
	m_settings.on_status = OnParserStatus;
	m_settings.on_header_field = OnHeaderField;
	m_settings.on_header_value = OnHeaderValue;
	m_settings.on_headers_complete = OnHeaderComplete;
	m_settings.on_body = OnBody;

	m_parser.data = this;
	http_parser_init(&m_parser, HTTP_REQUEST);
}

HttpConnection::~HttpConnection()
{
#ifdef  _DEBUG
	qDebug() << __FUNCTION__;
#endif
}

int HttpConnection::GetMethod() const
{
	if (m_bRequestHeaderDone)
		return m_parser.method;
	else
		//return http_method::HTTP_GET;
		return -1;
}

int HttpConnection::Send(std::string&& data, int responCode)
{
	m_code = responCode;
	m_strRespon = std::move(data);

	return 0;
}

int HttpConnection::SendFile(std::string strFilePath, int responCode)
{
	std::ifstream fileIn;
	fileIn.open(strFilePath, std::ifstream::in|std::ifstream::binary);
	if (!fileIn.is_open())
	{
		m_code = 404;
		m_strRespon = "404";
		return 0;
	}

	m_strRespon.clear();
	m_code = responCode;

	char buf[1024];
	while (true)
	{
		fileIn.read(buf, sizeof(buf));
		auto n = fileIn.gcount();
		m_strRespon.append(buf, fileIn.gcount());

		if (fileIn.eof())
		{
			break;
		}
		else if (fileIn.fail())
		{
			m_code = 500;
			m_strRespon = "500";
			break;
		}

		if (m_strRespon.length() > 1024 * 1024 * 1024)
		{
			break;
		}
	}


	auto pos = strFilePath.find_last_of('.');
	if(pos != std::string::npos)
	{
		auto strExt = strFilePath.substr(pos);
		if (_stricmp(strExt.c_str(), ".m3u8") == 0)
		{
			SetResponHeader("Content-Type", "application/vnd.apple.mpegurl");
		}
		else if (_stricmp(strExt.c_str(), ".jpg") == 0)
		{
			SetResponHeader("Content-Type", "image/jpeg");
		}
		else if (_stricmp(strExt.c_str(), ".png") == 0)
		{
			SetResponHeader("Content-Type", "image/png");
		}
	}

	return 0;
}

int HttpConnection::SetResponHeader(std::string k, std::string v)
{
	if (k.empty() || v.empty() ||
		v.find_first_of("\r\n") != std::string::npos ||
		k.find_first_of("\r\n") != std::string::npos)
	{
		return -1;
	}

	auto iter = m_mapResponHeader.find(k);
	if (iter != m_mapResponHeader.end())
	{
		m_mapResponHeader.erase(iter);
	}
	m_mapResponHeader.insert(std::make_pair(std::move(k), std::move(v)));

	return 0;
}

int HttpConnection::DoSend()
{
	if (m_strRespon.empty())
	{
		m_strRespon = "500";
		m_code = 500;
	}

	if (!m_pSocket || !m_pSocket->isOpen())
	{
		this->deleteLater();
		return -1;
	}

	static QLocale locale = QLocale::English;
	auto strDate = locale.toString(QDateTime::currentDateTime().toUTC(), "ddd, dd MMM yyyy hh:mm:ss").toStdString() + " GMT";
	std::map<std::string, std::string> mapDefaultHeader{
		{"server", "Qt"},
		{"connection", "close"},
		{"date", strDate},
		{"content-length", std::to_string(m_strRespon.length())},
		{"content-type", "text/plain; charset=utf-8"}
	};

	for (auto iter = mapDefaultHeader.begin(); iter != mapDefaultHeader.end(); ++iter)
	{
		m_mapResponHeader.insert(*iter);
	}

	std::stringstream ss;
	ss << "HTTP/1.1 " << m_code << " " << http_status_str((http_status)m_code) << "\r\n";
	for (auto iter = m_mapResponHeader.begin(); iter != m_mapResponHeader.end(); ++iter)
	{
		ss << iter->first << ": " << iter->second << "\r\n";
	}
	ss << "\r\n";
	ss << m_strRespon;
	
	m_strRespon = ss.str();
	m_iWritten = 0;
	m_pSocket->write(m_strRespon.c_str(), m_strRespon.length());

	return 0;
}

void HttpConnection::OnError(QAbstractSocket::SocketError)
{
	qDebug() << __FUNCTION__;
	if (m_pSocket)
	{
		qDebug() << m_pSocket->errorString();
	}
}

void HttpConnection::OnDisconnect()
{
	qDebug() << __FUNCTION__;
	QObject::disconnect(m_pSocket, nullptr, this, nullptr);
	this->deleteLater();
}

void HttpConnection::OnReadyRead()
{
	auto data = m_pSocket->readAll();
	if (data.size() < 1)
	{
		qDebug() << "no data";
		return;
	}

	m_recvBuf.append(data);

	auto ret = http_parser_execute(&m_parser, &m_settings, m_recvBuf.data(), m_recvBuf.size());
	if (http_errno::HPE_OK !=  m_parser.http_errno)
	{
		qDebug() << http_errno_name((http_errno)m_parser.http_errno);
		m_pSocket->close();
		this->deleteLater();
		return;
	}

	if ((int)ret == m_recvBuf.size())
	{
		m_recvBuf = QByteArray();
	}
	else
	{
		qDebug() << "cached data";
		m_recvBuf = m_recvBuf.mid((int)ret);
	}


	if (m_bRequestDone)
	{
		m_bRequestDone = false;
		http_parser_init(&m_parser, HTTP_REQUEST);

		m_pServer->Router(*this);
		DoSend();
	}
}

void HttpConnection::OnWritten(qint64 len)
{
	m_iWritten += len;
	if (m_iWritten >= m_strRespon.length())
	{
		qDebug() << "send complete";

		QObject::disconnect(m_pSocket, nullptr, this, nullptr);

		m_pSocket->close();
		deleteLater();
	}
}


SimpleHttpServer::SimpleHttpServer(QObject* p):
	QObject(p)
{
}

SimpleHttpServer::~SimpleHttpServer()
{
}

int SimpleHttpServer::Start(uint16_t port)
{
	m_pServer = new QTcpServer(this);
	m_pServer->setMaxPendingConnections(30);

	QObject::connect(m_pServer, &QTcpServer::newConnection, this, &SimpleHttpServer::OnNewConnection);
	QObject::connect(m_pServer, &QTcpServer::acceptError, this, &SimpleHttpServer::OnAcceptError);

	if (!m_pServer->listen(QHostAddress::AnyIPv4, port))
	{
		qDebug() << m_pServer->errorString();
		delete m_pServer;
		m_pServer = nullptr;

		return -1;
	}

	qDebug() << "address:" << m_pServer->serverAddress().toString();
	qDebug() << "port:" << m_pServer->serverPort();

	return 0;
}

int SimpleHttpServer::Stop()
{
	if (m_pServer)
	{
		QObject::disconnect(m_pServer, nullptr, nullptr, nullptr);

		if (m_pServer->isListening())
		{
			m_pServer->close();
		}
	}

	return 0;
}

uint16_t SimpleHttpServer::Port()
{
	if (m_pServer && m_pServer->isListening())
	{
		return m_pServer->serverPort();
	}
	return 0;
}

int SimpleHttpServer::RegisterRouter(std::string strRouter, RouterCb cb)
{
	auto pos = strRouter.find('?');
	if (pos != std::string::npos)
	{
		strRouter = strRouter.substr(0, pos);
	}

	m_mapRouter[strRouter] = cb;
	return 0;
}

int SimpleHttpServer::UnRegisterRouter(std::string strRouter)
{
	auto pos = strRouter.find('?');
	if (pos != std::string::npos)
	{
		strRouter = strRouter.substr(0, pos);
	}

	m_mapRouter.erase(strRouter);
	return 0;
}

int SimpleHttpServer::Router(HttpConnection& conn)
{
	auto strURL = conn.GetURL();
	auto pos = strURL.find('?');
	if (pos != std::string::npos)
	{
		conn.m_strQuery = strURL.substr(pos + 1);
		strURL = strURL.substr(0, pos);
	}

	auto iter = m_mapRouter.find(strURL);
	if (iter == m_mapRouter.end())
	{
		qDebug() << "no register router " << strURL.c_str();
		conn.Send("404", 404);
	}
	else
	{
		iter->second(conn);
	}

	return 0;
}

void SimpleHttpServer::OnNewConnection()
{
	QTcpSocket* pSocket = nullptr;
	while ((pSocket = m_pServer->nextPendingConnection()) != nullptr)
	{
		new HttpConnection(pSocket, this);
	}
}

void SimpleHttpServer::OnAcceptError(QAbstractSocket::SocketError err)
{
	if (m_pServer)
	{
		qDebug() << __FUNCTION__ << " " << m_pServer->errorString();
	}
}