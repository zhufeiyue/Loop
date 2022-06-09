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
	pServer->RegisterRouter("/", [](HttpConnectionPtr conn)
		{
			QString s =  QString::fromLocal8Bit("ÄãºÃ");
			auto ss = s.toUtf8().toStdString();
			conn->Send(ss + "hello");

			return 0;
		});

	pServer->RegisterRouter("/json", [](HttpConnectionPtr conn)
		{
			QJsonObject obj;
			obj["result"] = true;
			obj["data"] = rand();

			conn->SetResponHeader("Content-Type", "application/json");
			conn->Send(QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString());

			return 0;
		});
	pServer->RegisterRouter("/1.jpg", [](HttpConnectionPtr conn)
		{
			qDebug() << "request 1.jpg";
			conn->SendFile("d:/1.png");
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
//#ifdef  _DEBUG
//	qDebug() << __FUNCTION__;
//#endif

	QObject::connect(pSocket, SIGNAL(error(QAbstractSocket::SocketError)), 
		this, SLOT(OnError(QAbstractSocket::SocketError)));
	QObject::connect(pSocket, SIGNAL(disconnected()),
		this, SLOT(OnDisconnect()));
	QObject::connect(pSocket, SIGNAL(readyRead()),
		this, SLOT(OnReadyRead()));
	QObject::connect(pSocket, SIGNAL(bytesWritten(qint64)),
		this, SLOT(OnWritten(qint64)));


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
//#ifdef  _DEBUG
//	qDebug() << __FUNCTION__;
//#endif
	if (m_pSocket)
	{
		delete m_pSocket;
		m_pSocket = nullptr;
	}
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
	m_iResponBodyLength = data.length();	
	m_sendQueue.push_back(std::make_pair(QByteArray(data.c_str(), data.length()), 0));

	return 0;
}

int HttpConnection::Send(QByteArray data, int responCode)
{
	m_code = responCode;
	m_iResponBodyLength = data.size();
	m_sendQueue.push_back(std::make_pair(data, 0));

	return 0;
}

int HttpConnection::SendFile(std::string strFilePath, int responCode)
{
	std::ifstream fileIn;
	std::string strContent;
	fileIn.open(strFilePath, std::ifstream::in|std::ifstream::binary);
	if (!fileIn.is_open())
	{
		m_code = 404;
		strContent = "404";
		return 0;
	}

	strContent.clear();
	m_code = responCode;

	char buf[1024];
	while (true)
	{
		fileIn.read(buf, sizeof(buf));
		strContent.append(buf, fileIn.gcount());

		if (fileIn.eof())
		{
			break;
		}
		else if (fileIn.fail())
		{
			m_code = 500;
			strContent = "500";
			break;
		}

		if (strContent.length() > 1024 * 1024 * 1024)
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

	return Send(std::move(strContent), m_code);
}

int HttpConnection::SendPart(QByteArray data_part, int64_t bodyTotalLength, bool firstPart, int responCode)
{
	m_code = responCode;

	if (firstPart)
	{
		m_iResponBodyLength = bodyTotalLength;
		m_sendQueue.push_back(std::make_pair(data_part, 0));
	}
	else
	{
		m_sendQueue.push_back(std::make_pair(data_part, 0));

		DoSendData();
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
	if (m_sendQueue.empty())
	{
		Send(std::string("500"), 500);
	}

	if (!m_pSocket || !m_pSocket->isOpen())
	{
		Terminate();
		return -1;
	}

	static QLocale locale = QLocale::English;
	auto strDate = locale.toString(QDateTime::currentDateTime().toUTC(), "ddd, dd MMM yyyy hh:mm:ss").toStdString() + " GMT";
	std::map<std::string, std::string> mapDefaultHeader{
		{"server", "Qt"},
		{"connection", "close"},
		{"date", strDate},
		{"content-length", std::to_string(m_iResponBodyLength)},
		{"content-type", "text/plain; charset=utf-8"}
	};

	for (auto iter = mapDefaultHeader.begin(); iter != mapDefaultHeader.end(); ++iter)
	{
		m_mapResponHeader.insert(*iter);
	}

	std::stringstream ss;
	// format respon header
	ss << "HTTP/1.1 " << m_code << " " << http_status_str((http_status)m_code) << "\r\n";
	for (auto iter = m_mapResponHeader.begin(); iter != m_mapResponHeader.end(); ++iter)
	{
		ss << iter->first << ": " << iter->second << "\r\n";
	}
	ss << "\r\n";
	auto strHeader = ss.str();
	auto dataHeader = QByteArray(strHeader.c_str(), strHeader.length());
	m_iResponHeaderLength = strHeader.length();
	m_sendQueue.push_front(std::make_pair(dataHeader, 0));
	
	// format respon body
	m_sendQueue;
	
	m_iResponWritten = 0;
	DoSendData();

	return 0;
}

int HttpConnection::DoSendData()
{
	if (m_bWriting)
	{
		return 0;
	}

	if (m_sendQueue.empty())
	{
		if (m_iResponWritten >= m_iResponHeaderLength + m_iResponBodyLength)
		{
			Terminate();
		}
		return 0;
	}

	m_bWriting = true;
	if (m_pSocket)
	{
		m_pSocket->write(m_sendQueue.front().first);
	}

	return 0;
}

int HttpConnection::Terminate()
{
	if (m_pSocket)
	{
		QObject::disconnect(m_pSocket, nullptr, this, nullptr);
		m_pSocket->close();
		m_pSocket = nullptr;
	}

	m_pServer->EndConnection(shared_from_this());
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
	Terminate();
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
		Terminate();
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

		m_pServer->Router(shared_from_this());
		DoSend();
	}
}

void HttpConnection::OnWritten(qint64 len)
{
	m_iResponWritten += len;

	if (m_sendQueue.empty())
	{
		qDebug() << "fatal error";
		return;
	}

	auto& current_send_data = m_sendQueue.front();
	current_send_data.second += len;
	if (current_send_data.second >= current_send_data.first.size())
	{
		m_sendQueue.pop_front();
		m_bWriting = false;
		DoSendData();
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
	m_pServer->setMaxPendingConnections(5);

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

	if (m_mapRouter.find(strRouter) != m_mapRouter.end())
	{
		qDebug() << "router exist: " << strRouter.c_str();
		return -1;
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

int SimpleHttpServer::Router(HttpConnectionPtr conn)
{
	auto strURL = conn->GetURL();
	auto pos = strURL.find('?');
	if (pos != std::string::npos)
	{
		conn->m_strQuery = strURL.substr(pos + 1);
		strURL = strURL.substr(0, pos);
	}

	auto iter = m_mapRouter.find(strURL);
	if (iter == m_mapRouter.end())
	{
		qDebug() << "no register router " << strURL.c_str();
		conn->Send(std::string("404"), 404);
	}
	else
	{
		iter->second(conn);
	}

	return 0;
}

int SimpleHttpServer::StartConnection(HttpConnectionPtr p)
{
	m_connections.insert(std::move(p));
	return 0;
}

int SimpleHttpServer::EndConnection(HttpConnectionPtr p)
{
	auto iter = m_connections.find(p);
	if (iter != m_connections.end())
	{
		m_connections.erase(iter);
	}

//#ifdef _DEBUG
//	qDebug() << "connection number: " << m_connections.size();
//#endif

	return 0;
}

void SimpleHttpServer::OnNewConnection()
{
	QTcpSocket* pSocket = nullptr;
	while ((pSocket = m_pServer->nextPendingConnection()) != nullptr)
	{
		std::shared_ptr<HttpConnection> p(new HttpConnection(pSocket, this), 
			[](HttpConnection* p) 
			{
				if (p)
					p->deleteLater();
			});

		StartConnection(std::move(p));
	}
}

void SimpleHttpServer::OnAcceptError(QAbstractSocket::SocketError err)
{
	if (m_pServer)
	{
		qDebug() << __FUNCTION__ << " " << m_pServer->errorString();
	}
}