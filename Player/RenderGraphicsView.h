#pragma once

#include "IRender.h"
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QImage>
#include <QEvent>

#include "FFmpegDemuxer.h"

class VideoItem : public QGraphicsItem
{
public:
	VideoItem();

	QImage& Image()
	{
		return m_image;
	}
private:
	QRectF boundingRect() const  override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr);

private:
	QImage m_image;
};

class VideoRenderGraphicsView : public IRender
{
public:
	VideoRenderGraphicsView(QGraphicsView* pView);
	~VideoRenderGraphicsView();

	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

protected:
	int ConfigureRender(int, int, AVPixelFormat, int, int);

protected:
	QGraphicsView* m_pRenderView = nullptr;
	QGraphicsPixmapItem* m_pImageItem = nullptr;
	QObject* m_pDetectResize = nullptr;
	QImage m_image;

	int m_iWidth = 0;
	int m_iHeight = 0;
	double m_dRate = 0;
	AVPixelFormat m_format = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pImageConvert;
};

class GraphicsViewEventFilter : public QObject
{
public:
	GraphicsViewEventFilter(VideoRenderGraphicsView*, QGraphicsView*);
	~GraphicsViewEventFilter();
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	VideoRenderGraphicsView* m_pRender = nullptr;
	QGraphicsView* m_pCanvas = nullptr;

};