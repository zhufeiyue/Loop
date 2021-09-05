#include <QtWidgets/QApplication>
#include <QWidget>
#include <QBoxLayout>
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QCloseEvent>
#include <QOpenGLWidget>

#include "FFmpegDemuxer.h"
#include "DecodeFile.h"
#include "Player.h"

#include <common/Dic.h>
#include <common/Log.h>

#include <set>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	QWidget w;
	auto pScene = new QGraphicsScene(&w);
	auto pView = new QGraphicsView(pScene, &w);
	pView->setBackgroundBrush(QBrush(QColor(20, 20, 20)));
	pView->setCacheMode(QGraphicsView::CacheBackground);
	pView->setViewport(new QOpenGLWidget());
	pView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pView->setInteractive(false);
	pView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pView->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);
	pView->setOptimizationFlag(QGraphicsView::DontSavePainterState);
	pView->setRenderHint(QPainter::TextAntialiasing, false);
	pView->setFrameShape(QFrame::NoFrame); // remove border
	auto align = pView->alignment();

	new QHBoxLayout(&w);
	w.layout()->setContentsMargins(0, 0, 0, 0);
	w.layout()->setSpacing(0);
	w.layout()->addWidget(pView);
	w.resize(800, 600);
	w.show();

	Player player;
	player.InitVideoRender(&w);
	//player.StartPlay("D:/Ѹ������/1/222/222.mp4");
	//player.StartPlay("D:/Ѹ������/veep.s07e06.web.h264-memento[ettv].mkv");
	//player.StartPlay("D:/Ѹ������/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv");
	player.StartPlay("D:/Ѹ������/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��[66Ӱ��www.66Ys.Co].mp4");


	return app.exec();
}