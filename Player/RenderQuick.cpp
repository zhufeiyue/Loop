#include "RenderQuick.h"
#include "qml/QuickVideoRender.h"

VideoRenderQuick::VideoRenderQuick(QQuickItem* pRenderObject)
{
	m_pRenderObject = pRenderObject;
	static_cast<QuickVideoRenderObject*>(m_pRenderObject)->SetDataCallback(
		[this](QuickVideoRenderObject::QuickRenderData& renderData) {

		auto videoFrame = m_videoFrameData;
		if (!videoFrame)
		{
			return CodeAgain;
		}

		auto pFrame = videoFrame->FrameData();
		if (pFrame->width != m_iWidth ||
			pFrame->height != m_iHeight ||
			pFrame->format != m_format)
		{
			int result = ConfigureRender(
				pFrame->width,
				pFrame->height,
				(AVPixelFormat)pFrame->format);
			if (result != CodeOK)
			{
				return CodeNo;
			}
		}

		renderData.width = pFrame->width;
		renderData.height = pFrame->height;

		if (m_pImageConvert)
		{
			int result = m_pImageConvert->Convert(pFrame->data, pFrame->linesize);
			if (result < 0)
			{
				return CodeNo;
			}

			renderData.format = m_pImageConvert->Frame()->format;
			renderData.pData = m_pImageConvert->Frame()->data;
			renderData.pLineSize = m_pImageConvert->Frame()->linesize;
		}
		else
		{
			renderData.format = pFrame->format;
			renderData.pData = pFrame->data;
			renderData.pLineSize = pFrame->linesize;
		}

		return CodeOK;
	});
}

VideoRenderQuick::~VideoRenderQuick()
{
}

int VideoRenderQuick::ConfigureRender(RenderInfo mediaInfo)
{
	auto type = mediaInfo.find("type")->second.to<std::string>();

	if (type == "init")
	{
		if (!mediaInfo.contain_key_value("hasVideo", 1))
		{
			return CodeNo;
		}

		if (!m_pRenderObject)
		{
			return CodeNo;
		}

		auto width = mediaInfo.find("width")->second.to<int>();
		auto height = mediaInfo.find("height")->second.to<int>();
		auto rate = mediaInfo.find("videoRate")->second.to<double>(1.0);
		auto format = mediaInfo.find("videoFormat")->second.to<int>(-1);

		//return ConfigureRender(width, height, (AVPixelFormat)format);
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}

int VideoRenderQuick::ConfigureRender(int width, int height, AVPixelFormat format)
{
	LOG() << __FUNCTION__ << " " << width << " " << height << " " << format;

	m_iWidth = width;
	m_iHeight = height;
	m_format = format;

	auto formatWants = static_cast<QuickVideoRenderObject*>(m_pRenderObject)->GetSupportedPixformat();

	if (std::find(formatWants.begin(), formatWants.end(), format) == formatWants.end())
	{
		m_pImageConvert.reset(new FFmpegImageScale());
		if (CodeOK != m_pImageConvert->Configure(width, height, format, width, height, formatWants[0]))
		{
			return CodeNo;
		}
	}
	else
	{
		m_pImageConvert.reset();
	}

	return CodeOK;
}

int VideoRenderQuick::UpdataFrame(FrameHolderPtr data)
{
	m_videoFrameData = std::move(data);
	m_pRenderObject->update();
	return CodeOK;
}

int VideoRenderQuick::Start()
{
	return CodeOK;
}

int VideoRenderQuick::Pause(bool)
{
	return CodeOK;
}

int VideoRenderQuick::Stop()
{
	m_videoFrameData.reset();
	return CodeOK;
}

int VideoRenderQuick::Reset()
{
	return CodeOK;
}

int VideoRenderQuick::GetRenderTime(int64_t&)
{
	return CodeOK;
}