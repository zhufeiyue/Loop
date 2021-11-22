#include <QtWidgets/QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QCloseEvent>
#include <QOpenGLWidget>
#include <QJsonDocument>
#include <QSysInfo>

#include "FFmpegDemuxer.h"
#include "DecodeFile.h"
#include "Player.h"

#include <common/Dic.h>
#include <common/Log.h>

#include "RenderOpenAL.h"

// 使用的ffmpeg版本为4.3
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

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	
	av_log_set_callback(my_log_callback);

	//testPlayWav();
	//return 0;
	QSysInfo sysInfo;
	auto t1 = sysInfo.prettyProductName();
	auto t2 = sysInfo.productType();
	auto t3 = sysInfo.productVersion();

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

	auto pLayout = new QGridLayout(&w);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->setSpacing(0);
	pLayout->addWidget(pView, 0, 0);

	w.resize(800, 600);
	w.show();

	Player player;
	player.InitVideoRender(&w);
	player.InitAudioRender(nullptr);

	//player.StartPlay("D:/迅雷下载/阳光电影www.ygdy8.com.神奇女侠1984.2020.BD.1080P.国英双语双字.mkv");
	//player.StartPlay("D:/迅雷下载/[久久美剧www.jjmjtv.com]星际之门.宇宙.Stargate.Universe.S01E18.Chi_Eng.BD-HDTV.AC3.1024X576.x264-YYeTs.mkv");
	//player.StartPlay("D:/迅雷下载/1/1/hotel3.mp4");
	//player.StartPlay("D:/迅雷云盘/楚门的世界.1080p.国英双语.BD中英双字/楚门的世界.1080p.国英双语.BD中英双字[66影视www.66Ys.Co].mp4");
	//player.StartPlay("D:/迅雷云盘/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv");
	player.StartPlay("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048");
	
	auto result = app.exec();

	player.StopPlay();
	player.DestroyVideoRender();
	player.DestroyAudioRender();

	return result;
}