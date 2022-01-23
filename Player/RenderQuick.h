#pragma once

#include "IRender.h"
#include "FFmpegDemuxer.h"

#include <QQuickItem>

class VideoRenderQuick : public IRender
{
public:
	explicit VideoRenderQuick(QQuickItem*);
	~VideoRenderQuick();

	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Stop() override;
	int Pause(bool) override;
	int Reset() override;
	int GetRenderTime(int64_t&) override;

private:
	int ConfigureRender(int, int, AVPixelFormat);
	FrameHolderPtr m_videoFrameData;

private:
	QQuickItem* m_pRenderObject = nullptr;

	int m_iWidth = 0;
	int m_iHeight = 0;
	AVPixelFormat m_format = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pImageConvert;
};