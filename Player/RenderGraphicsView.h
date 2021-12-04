#pragma once

#include "IRender.h"
#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QImage>
#include <QEvent>
#include <QStyleOptionGraphicsItem>

#include "FFmpegDemuxer.h"

class VideoItem : public QGraphicsItem
{
public:
	VideoItem();

	int ImageWidth() const
	{
		return m_image.width();
	}

	int ImageHeight() const
	{
		return m_image.height();
	}

	QImage::Format ImageFormat() const
	{
		return m_image.format();
	}

	qsizetype ImageBytesPerLine() const
	{
		return m_image.bytesPerLine();
	}

	uchar* ImageData()
	{
		return m_image.bits();
	}

	int SetCanvasSize(int w, int h);

	int SetNewImage(int w, int h, QImage::Format  f);

private:
	QRectF boundingRect() const  override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr);

private:
	QImage m_image;
	QRectF m_rectBound;
	QRectF m_rectDraw;
};

class VideoRenderGraphicsView : public IRender
{
public:
	VideoRenderGraphicsView(QGraphicsView* pView);
	~VideoRenderGraphicsView();

	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Stop() override;
	int Pause(bool) override;
	int Reset() override;
	int GetRenderTime(int64_t&) override;

protected:
	int ConfigureRender(int, int, AVPixelFormat, int, int);

protected:
	QGraphicsView* m_pRenderView = nullptr;
	VideoItem*     m_pVideoItem  = nullptr;
	QObject*       m_pDetectResize = nullptr;

	int m_iWidth  = 0;
	int m_iHeight = 0;
	AVPixelFormat m_format = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pImageConvert;

	int64_t m_iCurrentFramePTS = 0;
};

class GraphicsViewEventFilter : public QObject
{
public:
	GraphicsViewEventFilter(VideoRenderGraphicsView*, QGraphicsView*);
	~GraphicsViewEventFilter();
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	VideoRenderGraphicsView* m_pRender = nullptr;
	QGraphicsView*           m_pCanvas = nullptr;
};