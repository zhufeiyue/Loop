#include "RenderGraphicsView.h"
#include <common/Log.h>
#include "FFmpegDemuxer.h"


VideoItem::VideoItem():
	QGraphicsItem(nullptr)
{
	SetNewImage(128, 128, QImage::Format::Format_RGB32);
}

int VideoItem::SetNewImage(int w, int h, QImage::Format  f)
{
	prepareGeometryChange();

	m_image = QImage(w, h, f);
	m_image.fill(QColor(20, 20, 20, 0));
	m_rectBound = QRectF(0, 0, m_image.width(), m_image.height());
	//m_rectDraw = m_rectBound;

	return CodeOK;
}

int VideoItem::SetCanvasSize(int canvasWidth, int canvasHeight)
{
	auto scale = (std::min)(1.0 * canvasWidth / m_image.width(), 1.0 * canvasHeight / m_image.height());
	auto drawWidth = m_image.width() * scale;
	auto drawHeight = m_image.height() * scale;
	auto drawX = (canvasWidth - drawWidth) / 2;
	auto drawY = (canvasHeight - drawHeight) / 2;

	prepareGeometryChange();
	m_rectBound = QRectF(0, 0, canvasWidth, canvasHeight);
	m_rectDraw = QRectF(drawX, drawY, drawWidth, drawHeight);

	return CodeOK;
}

QRectF VideoItem::boundingRect() const
{
	return m_rectBound;
}

void VideoItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget*)
{
	//painter->drawImage(m_rectDraw, m_image);
	painter->drawImage(m_rectDraw, m_image, 
		QRectF(0, 0, m_image.width(), m_image.height()),
		Qt::AutoColor | Qt::ThresholdDither |Qt::NoOpaqueDetection);
}


VideoRenderGraphicsView::VideoRenderGraphicsView(QGraphicsView* pView):
	m_pRenderView(pView)
{
	if (!pView)
	{
		return;
	}

	auto pScene = pView->scene();
	m_pVideoItem = new VideoItem();
	m_pVideoItem->setVisible(false);
	pScene->addItem(m_pVideoItem);

	m_pDetectResize = new GraphicsViewEventFilter(this, pView);
}

VideoRenderGraphicsView::~VideoRenderGraphicsView()
{
	if (m_pDetectResize)
	{
		delete m_pDetectResize;
	}

	if (m_pRenderView)
	{
		auto pScene = m_pRenderView->scene();
		if (m_pVideoItem)
		{
			pScene->removeItem(m_pVideoItem);
			delete m_pVideoItem;
			m_pVideoItem = nullptr;
		}
	}
}

int VideoRenderGraphicsView::ConfigureRender(RenderInfo mediaInfo)
{
	auto type = mediaInfo.find("type")->second.to<std::string>();
	auto canvasWidth = m_pRenderView->width();
	auto canvasHeight = m_pRenderView->height();


	if (type == "init")
	{
		auto iter = mediaInfo.find("hasVideo");
		if (iter == mediaInfo.end() || !iter->second.to<int>())
		{
			return CodeNo;
		}

		auto width = mediaInfo.find("width")->second.to<int>();
		auto height = mediaInfo.find("height")->second.to<int>();
		auto rate = mediaInfo.find("videorate")->second.to<double>(1.0);
		auto format = mediaInfo.find("videoformat")->second.to<int>(-1);
		return ConfigureRender(width, height, (AVPixelFormat)format, canvasWidth, canvasHeight);
	}
	else if (type == "resize")
	{
		LOG() << "resize view " << canvasWidth << ' ' << canvasHeight;
		if (m_iWidth < 1 || m_iHeight < 1 || m_format == AV_PIX_FMT_NONE)
		{
			return CodeNo;
		}

		if (m_pVideoItem)
			m_pVideoItem->SetCanvasSize(canvasWidth, canvasHeight);
		return CodeOK;
	}
	else
	{
		return CodeNo;
	}
}

int VideoRenderGraphicsView::ConfigureRender(int width, int height, AVPixelFormat format, int canvasWidth, int canvasHeight)
{
	if (width < 1 ||
		height < 1 ||
		format == AV_PIX_FMT_NONE ||
		canvasWidth < 1 ||
		canvasHeight < 1)
	{
		return CodeNo;
	}

	m_iWidth = width;
	m_iHeight = height;
	m_format = format;

	std::map<QImage::Format, AVPixelFormat> mapQImageFFmpegFormat = {
		{QImage::Format::Format_RGB32, AV_PIX_FMT_BGRA},
		{QImage::Format::Format_ARGB32, AV_PIX_FMT_BGRA},
		{QImage::Format::Format_RGB888, AV_PIX_FMT_RGB24}
	};
	auto iter = mapQImageFFmpegFormat.find(QImage::Format::Format_RGB32);

	if (width != m_pVideoItem->ImageWidth()
		|| height != m_pVideoItem->ImageHeight()
		|| iter->first != m_pVideoItem->ImageFormat())
	{
		m_pVideoItem->SetNewImage(width, height, iter->first);
	}

	if (format != iter->second)
	{
		m_pImageConvert.reset(new FFmpegImageScale());
		if (CodeOK != m_pImageConvert->Configure(width, height, format, width, height, iter->second))
		{
			return CodeNo;
		}
	}
	else
	{
		m_pImageConvert.reset();
	}

	m_pVideoItem->SetCanvasSize(canvasWidth, canvasHeight);
	m_pVideoItem->setVisible(true);


	return CodeOK;
}

int VideoRenderGraphicsView::UpdataFrame(FrameHolderPtr data)
{
	if (!data)
	{
		return CodeInvalidParam;
	}

	AVFrame* pFrame = data->FrameData();
	if (pFrame->width != m_iWidth ||
		pFrame->height != m_iHeight ||
		pFrame->format != m_format)
	{
		ConfigureRender(
			pFrame->width,
			pFrame->height,
			(AVPixelFormat)pFrame->format,
			m_pRenderView->width(),
			m_pRenderView->height());
	}

	if (m_pImageConvert)
	{
		uint8_t* dst[2] = { m_pVideoItem->ImageData(), nullptr };
		int stride[2] = { m_pVideoItem->ImageBytesPerLine(), 0 };
		int res = m_pImageConvert->Convert(pFrame->data, pFrame->linesize, dst, stride);
		if (res < 0)
		{
			return CodeNo;
		}
	}
	else
	{
		if (m_pVideoItem->ImageBytesPerLine() != pFrame->linesize[0])
		{
			return CodeNo;
		}
		memcpy(m_pVideoItem->ImageData(), pFrame->data[0], pFrame->height * pFrame->linesize[0]);
	}

	m_pVideoItem->update();

	return CodeOK;
}


GraphicsViewEventFilter::GraphicsViewEventFilter(VideoRenderGraphicsView* pRender, QGraphicsView* pCanvas):
	m_pRender(pRender),
	m_pCanvas(pCanvas)
{
	if (m_pCanvas)
	{
		m_pCanvas->installEventFilter(this);
	}
}

GraphicsViewEventFilter::~GraphicsViewEventFilter()
{
	if (m_pCanvas)
	{
		m_pCanvas->removeEventFilter(this);
	}
}

bool GraphicsViewEventFilter::eventFilter(QObject* watched, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::Resize)
	{
		Dictionary dic;
		dic.insert("type", "resize");
		if (m_pRender)
		{
			m_pRender->ConfigureRender(std::move(dic));
		}
	}
	return QObject::eventFilter(watched, pEvent);
}