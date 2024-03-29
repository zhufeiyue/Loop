#include "HLSProxy.h"
#include "Monitor.h"

#include <QApplication>
#include <QTimer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSharedMemory>

void testHlsProxy(int argc, char* argv[])
{
	QApplication app(argc, argv);

	if (false)
	{
		auto pHlsProxy = new HlsProxy();
		HlsProxyParam param;
		//"https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048", 
		//https://hls.cntv.myhwcdn.cn/asp/hls/main/0303000a/3/default/575aa972b9cd41a0969a91f19c0eb436/main.m3u8?maxbr=2048,
		//"https://dhls.cntv.qcloudcdn.com/asp/enc/hls/main/0303000a/3/default/8dbb6f5e94af47b2a41fd07341e03bad/main.m3u8?maxbr=2048&contentid=18120319242338",
		//"https://cctvcncc.v.wscdns.com/live/cctv15_1/index.m3u8?contentid=2820180516001&b=800-2100",
		//"http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865",
		//"http://dys1.v.myalicdn.com/lhls/lhls_c20_2-llhls.m3u8?aliyunols=on",

		param.strHlsAddress = "http://112.74.200.9:88/tv000000/m3u8.php?/migu/627198191";
		param.strDefaultVariant = "0";

		pHlsProxy->StartProxy(param);
		qDebug() << param.strProxyAddress.c_str();
	}

	if (true)
	{
		auto pManager = new HlsProxyManager();
		pManager->Start();
	}

	app.exec();
}

static int64_t CreateRandomNumber()
{
	static std::mt19937 rng(std::random_device{}());

	int64_t result = rng() * rng();
	return result;
}

int GetServer(std::shared_ptr<SimpleHttpServer>& got)
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
	if (0 != pServer->Start(0))
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

int RecordClientSid(QString);

static int SendJson(HttpConnectionPtr conn, QJsonObject jsonRespon)
{
	auto strRespon =  QJsonDocument(jsonRespon).toJson(QJsonDocument::Compact);
	conn->SetResponHeader("Content-Type", "application/json");
	conn->Send(std::move(strRespon));

	return 0;
}

static int SendInvalidParameter(HttpConnectionPtr conn){
	QJsonObject obj;
	obj["code"] = HlsProxyCode::InvalidParam;
	obj["message"] = "invalid parameter";
	SendJson(conn, obj);
	return 0;
}

static int SendDuplicate(HttpConnectionPtr conn){
	QJsonObject obj;
	obj["code"] = HlsProxyCode::Duplicate;
	obj["message"] = "duplicate";
	SendJson(conn, obj);
	return 0;
}

static QMap<QString, QString> ParseQuery(const std::string& strQuery)
{
	QMap<QString, QString> mapQuery;
	QStringList queryList = QString::fromStdString(strQuery).split('&');

	for (int i = 0; i < queryList.length(); ++i)
	{
		QString part = QByteArray::fromPercentEncoding(queryList.at(i).toUtf8());

		auto pos = part.indexOf('=');
		if (pos == -1)
			continue;
		QString k(part.mid(0, pos)), v(part.mid(pos + 1));

		mapQuery[k] = v;

		//qDebug() << k << " : " << v;
	}

	return mapQuery;
}


HlsProxy::HlsProxy()
{
}

HlsProxy::~HlsProxy()
{
}

int HlsProxy::StartProxy(HlsProxyParam& param)
{
	int ret = -1;
	std::shared_ptr<SimpleHttpServer> pServer;
	ret = GetServer(pServer);
	if (ret != 0)
	{
		return ret;
	}

	Dic dic;
	dic.insert("address", param.strHlsAddress);
	dic.insert("defaultVariant", param.strDefaultVariant);
	dic.insert("sessionId", param.strSessionId);

	m_strOriginM3U8Address = param.strHlsAddress;
	m_pOriginPlaylist.reset(new HlsPlaylist());
	ret = m_pOriginPlaylist->InitPlaylist(dic);
	if (0 != ret)
	{
		param.strErrorMessage = m_pOriginPlaylist->ErrorMessage();
		return ret;
	}

	if (true)
	{
		param.variantDuration = 0;
		param.variantStartTime = 0;

		std::shared_ptr<HlsVariant> pCurVariant;
		if (0 == m_pOriginPlaylist->GetCurrentVariant(pCurVariant))
		{
			param.variantDuration = pCurVariant->GetDuration();
			param.variantIndex = pCurVariant->GetVariantIndex();
			param.strMediaType = pCurVariant->GetType() == HlsVariant::Type::Vod ? "VOD" : "LIVE";

			if (pCurVariant->GetType() == HlsVariant::Type::Vod)
			{
				double startTime = 0;
				if (0 == pCurVariant->Seek(param.beginTime, startTime))
				{
					param.variantStartTime = startTime;
				}
			}
		}

		m_pOriginPlaylist->GetVariantInfoList(param.allVariantInfo);

		GetCDNInfo(param.strSessionId, param.strCDNSip, param.strCDNCip);
	}


	m_iProxySegNo = 0;
	m_strProxyName = "/" + std::to_string(CreateRandomNumber()) + ".m3u8";
	//m_strProxyName = "/1.m3u8";
	qDebug() << param.strSessionId.c_str() << " ---> " << m_strProxyName.c_str();

	param.strProxyAddress = "http://127.0.0.1:" + std::to_string(pServer->Port()) + m_strProxyName;
	pServer->RegisterRouter(m_strProxyName, [this](HttpConnectionPtr conn)
		{
			QMap<QString, QString> mapQuery = ParseQuery(conn->GetQuery());

			if (mapQuery["action"] == "monitor")
			{
				return HandleMonitor(conn, mapQuery);
			}
			else if (mapQuery["action"] == "seek")
			{
				return HandleSeek(conn, mapQuery);
			}
			else if (mapQuery["action"] == "switch")
			{
				return HandleSwitchVariant(conn, mapQuery);
			}

			auto iter = std::find_if(conn->m_vRequestHeader.begin(), conn->m_vRequestHeader.end(),
				[](std::pair<std::string, std::string>& p) {
					return p.first == "User-Agent" && p.second.find("CBox") != std::string::npos;
				});
			if (iter == conn->m_vRequestHeader.end())
			{
				conn->Send(std::string("404"), 404);
				return 0;
			}

			std::string strContent;
			int ret = GetContent(strContent);
			if (ret != 0)
			{
				qDebug() << "GetContent error " << ret;
				conn->Send(std::string("500"), 500);
				return 0;
			}

			conn->SetResponHeader("Content-Type", "application/vnd.apple.mpegurl");
			conn->Send(std::move(strContent), 200);

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

	qDebug() << "request stop hls proxy " << m_strProxyName.c_str();

	if (m_pOriginPlaylist)
	{
		m_pOriginPlaylist.reset();

		pServer->UnRegisterRouter(m_strProxyName);
		m_strProxyName = "";
	}

	return 0;
}

int HlsProxy::PauseProxy(bool b)
{
	m_bPauseProxy = b;
	return 0;
}

int HlsProxy::GetContent(std::string& strContent)
{
	if (!m_pOriginPlaylist)
	{
		qDebug() << "m_pOriginPlaylist is null";
		return -1;
	}

	std::stringstream ss;
	double duration = 0;
	bool   isEndSeg = false;
	std::shared_ptr<HlsSegment> pSeg;
	std::shared_ptr<HlsVariant> pVariant;

	if (m_bPauseProxy)
	{
		if (!m_strLastResponCntent.empty())
		{
			strContent = m_strLastResponCntent;
			return 0;
		}
	}

	m_pOriginPlaylist->GetCurrentVariant(pVariant);
	if (!pVariant)
	{
		qDebug() << "current variant is null";
		return -1;
	}

	pVariant->GetCurrentSegment(pSeg, isEndSeg);
	if (!pSeg)
	{
		qDebug() << "current segment is null";
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
	qDebug() << "respon seg no: " << pSeg->GetNo()
		<< " duration: " << duration
		<< " target duration: " << pVariant->GetTargetDuration()
		<< " address: " << pSeg->GetURL().c_str();

	duration -= 3;
	if (duration < 0.5)
	{
		duration = 0.5;
	}

	ss << "#EXTM3U\n";
	ss << "#EXT-X-VERSION:7\n";
	ss << "#EXT-X-TARGETDURATION:" << pVariant->GetTargetDuration() << "\n";
	ss << "#EXT-X-MEDIA-SEQUENCE:" << m_iProxySegNo << "\n";
	ss << "#EXT-X-PLAYLIST-TYPE:LIVE\n";
	ss << "#EXTINF:" << duration << ",\n";
	ss << pSeg->GetURL() << "\n";
	if (isEndSeg)
	{
		ss << "#EXT-X-ENDLIST\n";
	}

	m_iProxySegNo += 1;
	strContent = ss.str();
	m_strLastResponCntent = strContent;

End:
	QTimer::singleShot(20, [pVariant]()
		{
			if (pVariant)
			{
				pVariant->Prepare();
			}
		});

	return 0;
}

int HlsProxy::Seek(uint64_t pos, double& newStartPos)
{
	if (!m_pOriginPlaylist)
	{
		qDebug() << "m_pOriginPlaylist is null";
		return -1;
	}

	std::shared_ptr<HlsVariant> pVariant;
	m_pOriginPlaylist->GetCurrentVariant(pVariant);

	if (!pVariant)
	{
		qDebug() << "current variant is null";
		return -1;
	}

	return pVariant->Seek(pos, newStartPos);
}

int HlsProxy::HandleMonitor(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	QString strSessionId = mapQuery["sessionId"];

	std::string sip, cip;
	GetCDNInfo(strSessionId.toStdString(), sip, cip);

	QJsonObject obj;
	obj["code"] = HlsProxyCode::OK;
	obj["cdnsip"] = sip.c_str();
	obj["cdncip"] = cip.c_str();



	std::vector<TsDownloadRecord> items;
	QJsonArray a;
	GetTsDownloadInfo(strSessionId.toStdString(), items);
	for (size_t i = 0; i < items.size(); ++i)
	{
		QJsonObject temp;
		temp["originAddress"] = items[i].originAddress.c_str();
		temp["tsSize"] = items[i].tsSize;
		temp["tsDuration"] = items[i].tsDuration;
		temp["tsDownloadTime"] = items[i].tsDownloadTime;

		a.push_back(temp);
	}
	obj["tsDownload"] = a;



	std::vector<HttpRequestErrorInfo> items1;
	QJsonArray a1;
	GetHttpRequestErrorInfo(strSessionId.toStdString(), items1);
	for (size_t i = 0; i < items1.size(); ++i)
	{
		QJsonObject temp;
		temp["url"] = items1[i].strUrl.c_str();
		temp["time"] = items1[i].strTime.c_str();
		temp["message"] = items1[i].strErrorMsaage.c_str();
		temp["httpCode"] = items1[i].httpResponCode;
		temp["httpData"] = items1[i].strHttpResponData.c_str();

		a1.append(temp);
	}
	obj["httpError"] = a1;

	std::shared_ptr<HlsVariant> pVariant;
	if (m_pOriginPlaylist)
	{
		double s(0), d(0);

		m_pOriginPlaylist->GetCurrentVariant(pVariant);
		if (pVariant && 0 == pVariant->GetPreloadInfo(s, d))
		{
			obj["preloadPos"] = s + d;
		}
	}

	SendJson(conn, obj);

	return 0;
}

int HlsProxy::HandleSeek(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	QJsonObject obj;
	double newStartPos = 0;
	uint64_t pos = mapQuery["pos"].toULongLong();
	int ret = Seek(pos, newStartPos);
	if (0 != ret)
	{
		qDebug() << "seek error " << ret;
		obj["code"] = HlsProxyCode::SeekError;
	}
	else
	{
		obj["code"] = HlsProxyCode::OK;
		obj["newStartPos"] = newStartPos;
	}

	SendJson(conn, obj);
	return 0;
}

int HlsProxy::HandleSwitchVariant(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	Dic dic;
	dic.insert("newVariantIndex", mapQuery["index"].toInt());

	QJsonObject obj;
	if (m_pOriginPlaylist && 0 == m_pOriginPlaylist->SwitchVariant(dic))
	{
		obj["code"] = HlsProxyCode::OK;
		obj["newVariantIndex"] = mapQuery["index"].toInt();
		obj["newSegIndex"] = m_iProxySegNo;
	}
	else
	{
		obj["code"] = HlsProxyCode::SwitchError;
	}

	SendJson(conn, obj);
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

	static QSharedMemory sm_port("hls_proxy_port1");
	if (!sm_port.attach())
	{
		if (sm_port.create(8))
		{
			int port = pServer->Port();
			sm_port.lock();
			memcpy(sm_port.data(), &port, sizeof(port));
			sm_port.unlock();
		}
		else
		{
			return -1;
		}
	}


	pServer->RegisterRouter("/hls_proxy_manager", [this](HttpConnectionPtr conn)
		{
			auto mapQuery = ParseQuery(conn->GetQuery());
			QString strAction = mapQuery["action"];

			if (strAction == "start")
			{
				HandleStartPlayProxy(conn, mapQuery);
			}
			else if (strAction == "stop")
			{
				HandleStopPlayProxy(conn, mapQuery);
			}
			else if (strAction == "pause")
			{
				HandlePausePlayProxy(conn, mapQuery);
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

int HlsProxyManager::HandleStartPlayProxy(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	QJsonObject obj;
	obj["code"] = HlsProxyCode::OK;
	obj["message"] = "OK";

	QString strOriginAddress = mapQuery["originAddress"];
	QString strSessionId = mapQuery["sessionId"];
	QString strDefaultVariant = mapQuery["defaultVariant"];
	QString strClientSid = mapQuery["clientSid"];
	QString strBeginTime = mapQuery["beginTime"];

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

	RecordClientSid(strClientSid);

	HlsProxyParam param;
	param.strHlsAddress = strOriginAddress.toStdString();
	param.strDefaultVariant = strDefaultVariant.toStdString();
	param.strSessionId = strSessionId.toStdString();
	param.beginTime = strBeginTime.toLongLong();

	auto pHlsProxy = new HlsProxy();
	if (0 != pHlsProxy->StartProxy(param))
	{
		obj["code"] = HlsProxyCode::StartProxyError;
		obj["message"] = QString::fromStdString(param.strErrorMessage);

		delete pHlsProxy;
	}
	else
	{
		obj["proxyAddress"] = param.strProxyAddress.c_str();
		obj["initVariantDuration"] = param.variantDuration;
		obj["initVariantStartTime"] = param.variantStartTime;
		obj["initVariantIndex"] = param.variantIndex;
		obj["mediaType"] = param.strMediaType.c_str();
		obj["cdnsip"] = param.strCDNSip.c_str();
		obj["cdncip"] = param.strCDNCip.c_str();
		QJsonArray avi;
		for (size_t i = 0; i < param.allVariantInfo.size(); ++i)
		{
			QJsonObject temp;
			temp["bandwidth"] = param.allVariantInfo[i].get<int64_t>("bandwidth");
			temp["resolution"] = param.allVariantInfo[i].get<QString>("resolution");
			temp["index"] = param.allVariantInfo[i].get<int64_t>("index");

			avi.append(temp);
		}
		obj["allVariantInfo"] = avi;

		m_mapProxy[strSessionId.toStdString()] = pHlsProxy;
	}

	SendJson(conn, obj);
	return 0;
}

int HlsProxyManager::HandleStopPlayProxy(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	QString strSessionId = mapQuery["sessionId"];
	qDebug() << "request stop session " << strSessionId;

	ClearMonitorInfo(strSessionId.toStdString());

	auto iter = m_mapProxy.find(strSessionId.toStdString());
	if (iter == m_mapProxy.end())
	{
		qDebug() << "no session " << strSessionId << " " << __FUNCTION__;
		SendInvalidParameter(conn);
		return 0;
	}

	iter->second->StopProxy();
	delete iter->second;
	m_mapProxy.erase(iter);

	QJsonObject obj;
	obj["code"] = HlsProxyCode::OK;
	obj["message"] = "OK";
	SendJson(conn, obj);
	return 0;
}

int HlsProxyManager::HandlePausePlayProxy(HttpConnectionPtr conn, QMap<QString, QString>& mapQuery)
{
	QString strSessionId = mapQuery["sessionId"];

	auto iter = m_mapProxy.find(strSessionId.toStdString());
	if (iter == m_mapProxy.end())
	{
		qDebug() << "no session " << strSessionId << " " << __FUNCTION__;
		SendInvalidParameter(conn);
		return 0;
	}

	iter->second->PauseProxy(true);

	QJsonObject obj;
	obj["code"] = HlsProxyCode::OK;
	obj["message"] = "OK";
	SendJson(conn, obj);
	return 0;
}