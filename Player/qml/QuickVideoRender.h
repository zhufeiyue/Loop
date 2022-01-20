#pragma once

#include <common/Log.h>

#include <QtQuick/QQuickFramebufferObject>
#include <QtGui/QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

class VideoRender : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
	VideoRender();
	~VideoRender();
    void render() override;
	QOpenGLFramebufferObject* createFramebufferObject(const QSize&) override;
};

class QuickVideoRender : public QQuickFramebufferObject
{
	Q_OBJECT
public:
	static void Register();
	Renderer* createRenderer() const;
};