#include "HLSProxy.h"
#include "SimpleHttpServer.h"

#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

void testHlsProxy(int argc, char* argv[])
{
	QApplication app(argc, argv);

	auto pHlsProxy = new HlsProxy();
	std::string strProxyAddress;
	pHlsProxy->StartProxy(
		//"https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048", 
		//https://hls.cntv.myhwcdn.cn/asp/hls/main/0303000a/3/default/575aa972b9cd41a0969a91f19c0eb436/main.m3u8?maxbr=2048,
		//"https://dhls.cntv.qcloudcdn.com/asp/enc/hls/main/0303000a/3/default/8dbb6f5e94af47b2a41fd07341e03bad/main.m3u8?maxbr=2048&contentid=18120319242338",
		//"https://cctvcncc.v.wscdns.com/live/cctv15_1/index.m3u8?contentid=2820180516001&b=800-2100",
		//"http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865",
		//"http://112.74.200.9:88/tv000000/m3u8.php?/migu/627198191",
		"http://dys1.v.myalicdn.com/lhls/lhls_c20_2-llhls.m3u8?aliyunols=on",

		strProxyAddress);
	LOG() << strProxyAddress.c_str();

	//auto pManager = new HlsProxyManager();
	//pManager->Start();

	app.exec();
}

static int64_t CreateRandomNumber()
{
	static std::mt19937 rng(std::random_device{}());

	int64_t result = rng() * rng();
	return result;
}

static int GetServer(std::shared_ptr<SimpleHttpServer>& got)
{
	static std::shared_ptr<SimpleHttpServer> gpServer;
	if (gpServer)
	{
		got = gpServer;
		return 0;
	}

	static std::mutex gLock;
	std::lock_guard<std::mutex> guard(gLock);
	if (gpServer)
	{
		got = gpServer;
		return 0;
	}

	auto pServer = new SimpleHttpServer(nullptr);
	if (0 != pServer->Start(8000))
	{
		delete pServer;
		return -1;
	}

	gpServer = std::shared_ptr<SimpleHttpServer>(pServer, 
		[](SimpleHttpServer* p) 
		{
			if (p)
			{
				p->Stop();
				delete p;
			}
		});
	got = gpServer;
	return 0;
}

static int SendJson(HttpConnection& conn, QJsonObject jsonRespon)
{
	auto strRespon =  QJsonDocument(jsonRespon).toJson(QJsonDocument::Compact).toStdString();
	conn.SetResponHeader("Content-Type", "application/json");
	conn.Send(std::move(strRespon));

	return 0;
}

static int SendInvalidParameter(HttpConnection& conn)
{
	QJsonObject obj;
	obj["code"] = 1;
	obj["message"] = "invalid parameter";
	SendJson(conn, obj);

	return 0;
}

static int SendDuplicate(HttpConnection& conn)
{
	QJsonObject obj;
	obj["code"] = 2;
	obj["message"] = "duplicate";
	SendJson(conn, obj);

	return 0;
}

HlsProxy::HlsProxy()
{
}

HlsProxy::~HlsProxy()
{
}

int HlsProxy::StartProxy(std::string strOriginAddress, std::string& strProxyAddress)
{
	int ret = -1;
	std::shared_ptr<SimpleHttpServer> pServer;
	ret = GetServer(pServer);
	if (ret != 0)
	{
		return ret;
	}

	std::lock_guard<std::mutex> guard(m_lock);
	if (m_pOriginPlaylist)
	{
		return -1;
	}

	m_strOriginM3U8Address = strOriginAddress;
	m_pOriginPlaylist.reset(new HlsPlaylist());
	ret = m_pOriginPlaylist->InitPlaylist(strOriginAddress);
	if (0 != ret)
	{
		return ret;
	}

	m_iProxySegNo = 0;
	//m_strProxyName = "/" + std::to_string(CreateRandomNumber()) + ".m3u8";
	m_strProxyName = "/1.m3u8";

	strProxyAddress = "http://127.0.0.1:" + std::to_string(pServer->Port()) + m_strProxyName;
	pServer->RegisterRouter(m_strProxyName, [this](HttpConnection& conn) 
		{
			std::string strContent;
			auto ret = GetContent(strContent);
			if (ret != 0)
			{
				LOG() << "GetContent error " << ret;
				conn.Send("500", 500);
				return 0;
			}

			conn.SetResponHeader("Content-Type", "application/vnd.apple.mpegurl");
			conn.Send(std::move(strContent), 200);

			return 0;
		});

	return 0;
}

int HlsProxy::StopProxy()
{
	std::shared_ptr<SimpleHttpServer> pServer;
	int ret = GetServer(pServer);
	if (ret != 0)
	{
		return ret;
	}

	std::lock_guard<std::mutex> guard(m_lock);
	if (m_pOriginPlaylist)
	{
		m_pOriginPlaylist.reset();

		pServer->UnRegisterRouter(m_strProxyName);
		m_strProxyName = "";
	}

	return 0;
}

int HlsProxy::GetContent(std::string& strContent)
{
	std::lock_guard<std::mutex> guard(m_lock);
	if (!m_pOriginPlaylist)
	{
		LOG() << "m_pOriginPlaylist is null";
		return -1;
	}

	std::stringstream ss;
	double duration;
	std::shared_ptr<HlsSegment> pSeg;
	std::shared_ptr<HlsVariant> pVariant;

	m_pOriginPlaylist->GetCurrentVariant(pVariant);
	if (!pVariant)
	{
		LOG() << "current variant is null";
		return -1;
	}

	pVariant->GetCurrentSegment(pSeg);
	if (!pSeg)
	{
		LOG() << "current segment is null";
		if (!m_strLastResponCntent.empty())
		{
			strContent = m_strLastResponCntent;
			goto End;
		}
		else
		{
			return -1;
		}
	}

	duration = pSeg->GetDuration();
	LOG() << "respon seg no: " << pSeg->GetNo()
		<< " duration: " << duration
		<< " target duration: " << pVariant->GetTargetDuration();

	duration -= 3;
	if (duration < 1)
	{
		duration = 1;
	}

	ss << "#EXTM3U\n";
	ss << "#EXT-X-VERSION:7\n";
	ss << "#EXT-X-TARGETDURATION:" << pVariant->GetTargetDuration() << "\n";
	ss << "#EXT-X-MEDIA-SEQUENCE:" << m_iProxySegNo << "\n";
	ss << "#EXT-X-PLAYLIST-TYPE:LIVE\n";
	ss << "#EXTINF:" << duration << ",\n";
	ss << pSeg->GetURL() << "\n";

	m_iProxySegNo += 1;
	strContent = ss.str();
	m_strLastResponCntent = strContent;

End:
	QTimer::singleShot(10, [pVariant]()
		{
			if (pVariant)
			{
				pVariant->Prepare();
			}
		});

	return 0;
}


HlsProxyManager::HlsProxyManager()
{
}

HlsProxyManager::~HlsProxyManager()
{
}

int HlsProxyManager::Start()
{
	int ret = -1;
	std::shared_ptr<SimpleHttpServer> pServer;
	ret = GetServer(pServer);
	if (ret != 0)
	{
		return ret;
	}

	pServer->RegisterRouter("/hls_proxy_manager", [this](HttpConnection& conn) 
		{
			QStringList strQuery = QString::fromStdString(conn.GetQuery()).split('&');
			QString strAction;
			QString strOriginAddress;
			QString strSessionId;

			QJsonObject obj;
			obj["code"] = 0;
			obj["message"] = "OK";

			for (int i = 0; i < strQuery.length(); ++i)
			{
				auto temp = strQuery[i].split('=');
				if (temp.length() == 2)
				{
					if (temp[0].compare("action", Qt::CaseInsensitive) == 0)
						strAction = temp[1];
					else if (temp[0].compare("originAddress", Qt::CaseInsensitive) == 0)
						strOriginAddress = temp[1];
					else if (temp[0].compare("sessionId", Qt::CaseInsensitive) == 0)
						strSessionId = temp[1];
				}
			}

			if (strAction == "start")
			{
				if (strOriginAddress.isEmpty() || strSessionId.isEmpty())
				{
					SendInvalidParameter(conn);
					return 0;
				}

				if (m_mapProxy.find(strSessionId.toStdString()) != m_mapProxy.end())
				{
					SendDuplicate(conn);
					return 0;
				}

				std::string strProxyAddress;
				auto pHlsProxy = new HlsProxy();
				if (0 != pHlsProxy->StartProxy(strOriginAddress.toStdString(), strProxyAddress))
				{
					obj["code"] = 3;
					obj["message"] = "start proxy fail";

					delete pHlsProxy;
				}
				else
				{
					obj["proxyAddress"] = strProxyAddress.c_str();

					m_mapProxy[strSessionId.toStdString()] = pHlsProxy;
				}

				SendJson(conn, obj);
			}
			else if (strAction == "stop")
			{
				auto iter = m_mapProxy.find(strSessionId.toStdString());
				if (iter == m_mapProxy.end())
				{
					SendInvalidParameter(conn);
					return 0;
				}

				iter->second->StopProxy();
				delete iter->second;
				m_mapProxy.erase(iter);

				SendJson(conn, obj);
			}
			else
			{
				SendInvalidParameter(conn);
			}

			return 0;
		});

	return 0;
}

int HlsProxyManager::Stop()
{
	return 0;
}