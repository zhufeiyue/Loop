#include "QuickVideoRender.h"

VideoRender::VideoRender()
{
	initializeOpenGLFunctions();
}

VideoRender::~VideoRender()
{
}

void VideoRender::render()
{
	LOG() << __FUNCTION__;

	glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);


	//update();
}

QOpenGLFramebufferObject* VideoRender::createFramebufferObject(const QSize& size)
{
	LOG() << __FUNCTION__;

	QOpenGLFramebufferObjectFormat format;
	format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	//format.setSamples(4);

	return new QOpenGLFramebufferObject(size, format);
}


void QuickVideoRender::Register()
{
	qmlRegisterType<QuickVideoRender>("QuickVideoRendering", 1, 0, "VideoRender");
}

QQuickFramebufferObject::Renderer* QuickVideoRender::createRenderer() const
{
	return new VideoRender();
}