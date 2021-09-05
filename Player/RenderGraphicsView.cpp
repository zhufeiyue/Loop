#include "RenderGraphicsView.h"
#include <common/Log.h>
#include "FFmpegDemuxer.h"


VideoItem::VideoItem():
	QGraphicsItem(nullptr)
{

}

QRectF VideoItem::boundingRect() const
{
}

void VideoItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
}


VideoRenderGraphicsView::VideoRenderGraphicsView(QGraphicsView* pView):
	m_pRenderView(pView)
{
	if (!pView)
	{
		return;
	}

	auto pScene = pView->scene();
	m_pImageItem = new QGraphicsPixmapItem();
	m_pImageItem->setVisible(false);
	m_pImageItem->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
	m_pImageItem->setTransformationMode(Qt::FastTransformation);
	//m_pImageItem->setTransformationMode(Qt::SmoothTransformation);

	pScene->addItem(m_pImageItem);

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
		if (m_pImageItem)
		{
			pScene->removeItem(m_pImageItem);
			delete m_pImageItem;
			m_pImageItem = nullptr;
		}
	}
}

int VideoRenderGraphicsView::ConfigureRender(RenderInfo mediaInfo)
{
	auto type = mediaInfo.find("type")->second.to<std::string>();

	int canvasWidth = m_pRenderView->width();
	int canvasHeight = m_pRenderView->height();

	if (type == "init")
	{
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
		auto scale = (std::min)(1.0 * canvasWidth / m_iWidth, 1.0 * canvasHeight / m_iHeight);
		m_pImageItem->setScale(scale);
		LOG() << "new scale " << m_pImageItem->scale();
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

	if (width != m_image.width() || height != m_image.height() || iter->first != m_image.format())
	{
		m_image = QImage(width, height, iter->first);
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

	auto scale = (std::min)(1.0 * canvasWidth / width, 1.0 * canvasHeight / height);
	m_pImageItem->setTransformOriginPoint(width / 2.0f, height / 2.0f);
	m_pImageItem->setScale(scale);
	m_pImageItem->setVisible(true);
	auto pos = m_pImageItem->pos();
	auto pos1 = m_pImageItem->scenePos();

	return CodeOK;
}

int VideoRenderGraphicsView::UpdataFrame(FrameHolderPtr data)
{
	if (!data)
	{
		return CodeInvalidParam;
	}

	AVFrame* pFrame = data->operator AVFrame * ();
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
		AVFrame* pTemp = pFrame;
		int res = m_pImageConvert->Convert(pTemp->data, pTemp->linesize);
		if (res < 0)
		{
			return CodeNo;
		}

		pFrame = m_pImageConvert->m_pFrame;
	}

	if (pFrame->linesize[0] != m_image.bytesPerLine())
	{
		return CodeNo;
	}

	memcpy(m_image.bits(), pFrame->data[0], pFrame->linesize[0] * pFrame->height);
	m_pImageItem->setPixmap(QPixmap::fromImage(m_image));

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