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


int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	return 0;
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
	//player.StartPlay("D:/Ѹ������/�����Ӱwww.ygdy8.com.����Ů��1984.2020.BD.1080P.��Ӣ˫��˫��.mkv");
	player.StartPlay("D:/Ѹ������/1/2222/45.mp4");
	//player.StartPlay("D:/Ѹ������/Veep (2012) - S07E07 - Veep (1080p BluRay x265 Silence).mkv");
	//player.StartPlay("D:/Ѹ������/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��/���ŵ�����.1080p.��Ӣ˫��.BD��Ӣ˫��[66Ӱ��www.66Ys.Co].mp4");


	return app.exec();
}