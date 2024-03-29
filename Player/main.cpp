#include <QtWidgets/QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QCloseEvent>
#include <QOpenGLWidget>
#include <QJsonDocument>
#include <QSysInfo>
#include <QQmlApplicationEngine>
#include <QSslSocket>

#include <filesystem>
#include <common/Dic.h>
#include <common/Log.h>

#include "Player.h"
#include "PlayerTimer.h"
#include "RenderOpenAL.h"
#include "RenderOpenGLWidget.h"
#include "FFmpegFilter.h"
#include "qml/QuickVideoRender.h"
#include "hls/HLSPlaylist.h"
#include "hls/HLSProxy.h"
#include "qloop.h"

#ifdef _MSC_VER
#include <dwmapi.h>
#pragma comment (lib, "Dwmapi.lib")

void EnableDwm()
{
	BOOL bEnable = FALSE;
	HRESULT hr = DwmIsCompositionEnabled(&bEnable);

	if (!bEnable)
	{
		hr = DwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
	}
}
#endif

// ʹ�õ�ffmpeg�汾Ϊ4.3
static void my_log_callback(void*, int level, const char* format, va_list vl)
{
	static char buf[256] = { 0 };
	static std::mutex lock;

	if (level <= AV_LOG_WARNING)
	{
		std::lock_guard<std::mutex> guard(lock);
		vsnprintf(buf, sizeof(buf), format, vl);
		LOG() << "ffmpeg: " << buf;
	}
}

static void ChooseOpenGL()
{
	QSysInfo sysInfo;
	auto t1 = sysInfo.prettyProductName();
	auto t2 = sysInfo.productType();
	auto t3 = sysInfo.productVersion();

	auto appPath = std::filesystem::current_path();
	bool bUseSoftwareOpenGL = false;
	bool bUseOpenGLES = false;
	bool bUseDesktopOpenGL = true;

	if (t2 == "windows")
	{
		if (sysInfo.windowsVersion() < QSysInfo::WV_WINDOWS10)
		{
#ifdef _MSC_VER
			EnableDwm();
#endif
			bUseOpenGLES = true;
		}
	}

	// Ĭ�ϻ����opengl32.dll
	std::filesystem::path file_opengl32 = appPath / "opengl32.dll";
	if (std::filesystem::exists(file_opengl32))
	{
		std::filesystem::remove(file_opengl32);
	}

	if (bUseSoftwareOpenGL)
	{
		std::filesystem::path file_opengl32sw = appPath / "opengl32sw.dll";
		if (std::filesystem::exists(file_opengl32sw))
		{
			std::filesystem::copy(file_opengl32sw, file_opengl32);
		}

		QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);  // cpu���������
		//QApplication::setAttribute(Qt::AA_ShareOpenGLContexts); // cpu��������� +1 +1 +1
	}
	else if (bUseOpenGLES)
	{
		QApplication::setAttribute(Qt::AA_UseOpenGLES);
		//QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	}
	else
	{
		//QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
		//QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
	}
}

static void SetSurfaceFormat()
{
	auto openGLType = QOpenGLContext::openGLModuleType();

	auto sf = QSurfaceFormat::defaultFormat();
	auto sf_version = sf.version();
	auto sf_profile = sf.profile();
	auto sf_renderType = sf.renderableType();
	auto sf_colorSpace = sf.colorSpace();
	auto sf_swapBehavior = sf.swapBehavior();

	QSurfaceFormat fmt;
	fmt.setDepthBufferSize(24);
	fmt.setSwapBehavior(QSurfaceFormat::SwapBehavior::DoubleBuffer);

	if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
		fmt.setVersion(2, 0);
		fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	}
	else {
		fmt.setVersion(2, 0);
	}

	QSurfaceFormat::setDefaultFormat(fmt);
}

class PlayerControl : public QObject
{
public:
	PlayerControl(Player* p, QWidget* pUI)
	{
		m_pPlayer = p;
		m_pUI = pUI;
		m_pUI->installEventFilter(this);
	}

	~PlayerControl()
	{
		m_pUI->removeEventFilter(this);
	}

	bool eventFilter(QObject* watched, QEvent* event)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent* pMouseEvent = dynamic_cast<QMouseEvent*>(event);
			if (pMouseEvent->button() == Qt::LeftButton)
			{
				m_pPlayer->Pause(m_pPlayer->IsPlaying());
			}
			else if (pMouseEvent->button() == Qt::RightButton)
			{
				int64_t duration = 0;
				m_pPlayer->GetDuration(duration);

				int seekPos = 1.0f * pMouseEvent->x() / m_pUI->width() * duration * 1000;
				m_pPlayer->Seek(seekPos);
			}
		}
		return QObject::eventFilter(watched, event);
	}

	Player* m_pPlayer = nullptr;
	QWidget* m_pUI = nullptr;
};

int testBasePlayer(int argc, char* argv[])
{
	QApplication app(argc, argv);
	//SetSurfaceFormat();

	//testPlayWav();
	//return 0;
	//testFilter();
	//return 0;
	QWidget w;
	auto pScene = new QGraphicsScene(&w);
	auto pView = new QGraphicsView(pScene, &w);
	pView->setBackgroundBrush(QBrush(QColor(20, 20, 20)));
	pView->setCacheMode(QGraphicsView::CacheBackground);
	//pView->setViewport(new QOpenGLWidget());
	pView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pView->setInteractive(false);
	pView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
	pView->setOptimizationFlag(QGraphicsView::DontSavePainterState);
	pView->setRenderHint(QPainter::TextAntialiasing, false);
	pView->setFrameShape(QFrame::NoFrame); // remove border
	pView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

	auto pGLWidget = new VideoOpenGLWidget(nullptr);

	auto pLayout = new QGridLayout(&w);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->setSpacing(0);
	//pLayout->addWidget(pView, 0, 0);
	pLayout->addWidget(pGLWidget, 0, 0);

	w.resize(800, 600);
	w.show();
	//w.showMaximized();

	const char* pFile = nullptr;
	if (argc > 1)
	{
		pFile = argv[1];
	}
	else
	{
		pFile = "D:/Ѹ������/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv";
		pFile = "D:/Ѹ������/[�����Ӱwww.ygdy8.com].�˲���ĸǴı�.BD.720p.��Ӣ˫��Ļ.rmvb";
		pFile = "D:/Ѹ������/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��[66Ӱ��www.66Ys.Co].mp4";
		pFile = "D:/Ѹ������/The.Witcher.S02E01.A.Grain.of.Truth.1080p.NF.WEB-DL.DDP5.1.Atmos.x264-TEPES.mkv";
		pFile = "D:/Ѹ������/[�þ�����www.jjmjtv.com]�Ǽ�֮��.����.Stargate.Universe.S01E18.Chi_Eng.BD-HDTV.AC3.1024X576.x264-YYeTs.mkv";
		pFile = "D:/Ѹ������/������MP4��Ӱwww.boxmp4.com��2005.����.mkv";
		pFile = "D:/Ѹ������/Halo.S01E01.1080p.WEB.h264-KOGi[eztv.re].mkv";
		pFile = "https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048";
		pFile = "http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865";
		pFile = "D:/Ѹ������/�����Ӱwww.ygdy8.com.007����Ͼ����.2021.BD.1080P.��Ӣ˫��˫��.mkv";
		pFile = "D:/Ѹ������/1/015/video.wmv";
	}

	Player* player = nullptr;
	PlayerControl* playerControl = nullptr;

	player = new Player();
	playerControl = new PlayerControl(player, &w);
	player->InitVideoRender(&w);
	player->InitAudioRender(nullptr);
	player->StartPlay(pFile);

	auto result = app.exec();

	player->StopPlay();
	player->DestroyVideoRender();
	player->DestroyAudioRender();
	delete player;

	return result;
}

int testQmlPlayer(int argc, char* argv[])
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QGuiApplication app(argc, argv);

	QLoop q(nullptr);
	//q.Run(app.thread());

	QuickVideoRenderObject::Register();

	QQmlApplicationEngine engine;
	const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
	QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject* obj, const QUrl& objUrl) {
		if (!obj && url == objUrl)
			QCoreApplication::exit(-1);
	}, Qt::QueuedConnection);
	engine.load(url);

	auto rootObj = engine.rootObjects().at(0);
	auto pVideoRender = rootObj->findChild<QuickVideoRenderObject*>("render1");

	const char* pFile = nullptr;
	if (argc > 1)
	{
		pFile = argv[1];
	}
	else
	{
		pFile = "D:/Ѹ������/[�����Ӱwww.ygdy8.com].�˲���ĸǴı�.BD.720p.��Ӣ˫��Ļ.rmvb";
		pFile = "D:/Ѹ������/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv";
		pFile = "d:/myworld.wav";
		pFile = "D:/My World.mp3";
		pFile = "D:/Ѹ������/WeChat_20220328223801.mp4";
		pFile = "D:/Ѹ������/1/�����Ӱwww.ygdy8.com.007����Ͼ����.2021.BD.1080P.��Ӣ˫��˫��.mkv";
		//pFile = "http://112.51.31.67:22080/hls/live/bosma-C1-2k/index.m3u8";
		pFile = "D:/Ѹ������/�����Ӱwww.ygdy8.com.�ƶ�ׯ԰2.2022.BD.1080P.��Ӣ˫��.mkv";
		pFile = "http://112.74.200.9:88/tv000000/m3u8.php?/migu/625204865";
		pFile = "D:/Ѹ������/�����Ӱwww.ygdy8.com.007����Ͼ����.2021.BD.1080P.��Ӣ˫��˫��.mkv";
		pFile = "http://112.74.200.9:88/tv000000/m3u8.php?/migu/627198191";

	}

	Player* player = nullptr;

	q.PushEvent([=, &player] 
		{
			player = new Player();
			player->InitVideoRender(pVideoRender);
			player->InitAudioRender(nullptr);
			player->StartPlay(pFile);

			return 0;
		});

	auto result = app.exec();

	q.PushEvent([player]
		{
			player->StopPlay();
			player->DestroyVideoRender();
			player->DestroyAudioRender();
			delete player;
			return 0;
		});
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	return result;
}

void testHttpClient(int arc, char* argv[]);
void testHttpServer(int, char**);
void testMiniAudio();

int main(int argc, char* argv[])
{
	LOG() << "Qt use: " << QSslSocket::sslLibraryBuildVersionString().toStdString();

	av_log_set_callback(my_log_callback);
	ChooseOpenGL();
	testBasePlayer(argc, argv);
	//testQmlPlayer(argc, argv);
	//testHlsProxy(argc, argv);
	//testHttpClient(argc, argv);
	//testHttpServer(argc, argv);
	//testMiniAudio();

	return 0;
}