#include <QtWidgets/QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QCloseEvent>
#include <QOpenGLWidget>
#include <QJsonDocument>

#include "FFmpegDemuxer.h"
#include "DecodeFile.h"
#include "Player.h"

#include <common/Dic.h>
#include <common/Log.h>

#include "RenderOpenAL.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	//testPlayWav();
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
	//player.StartPlay("D:/迅雷云盘/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv");
	player.StartPlay("D:/迅雷云盘/楚门的世界.1080p.国英双语.BD中英双字/楚门的世界.1080p.国英双语.BD中英双字[66影视www.66Ys.Co].mp4");
	//player.StartPlay("https://newcntv.qcloudcdn.com/asp/hls/main/0303000a/3/default/4f7655094036437c8ec19bf50ba3a8e0/main.m3u8?maxbr=2048");
	

	auto result = app.exec();

	player.DestroyVideoRender();
	player.DestroyAudioRender();

	return result;
}