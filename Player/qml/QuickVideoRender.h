#pragma once

#include <common/Log.h>
#include "./IDecoder.h"

#include <QQuickFramebufferObject>
#include <QOpenGLFramebufferObject>
#include <QThread>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QMatrix4x4>

class QuickVideoRenderObject : public QQuickFramebufferObject
{
	Q_OBJECT
public:
	static void Register();

	struct QuickRenderData
	{
		int width = 0;
		int height = 0;
		int format = -1;
		const int* pLineSize = nullptr;
		const uint8_t* const* pData = nullptr;
	};

public:
	QuickVideoRenderObject(QQuickItem* parent = nullptr);
	~QuickVideoRenderObject();

	void SetDataCallback(std::function<int(QuickRenderData&)> func)
	{
		m_funcGetCurrentVideoFrame = std::move(func);
	}

	const std::function<int(QuickRenderData&)>& GetDataCallback() const
	{
		return m_funcGetCurrentVideoFrame;
	}

private:
	Renderer* createRenderer() const override;

private:
	std::function<int(QuickRenderData&)> m_funcGetCurrentVideoFrame;
};

class VideoRender : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
	VideoRender(const QuickVideoRenderObject* );
	~VideoRender();

	void render() override;
	void synchronize(QQuickFramebufferObject*) override;
	QOpenGLFramebufferObject* createFramebufferObject(const QSize&) override;

private:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData = nullptr);
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

private:
	QOpenGLShaderProgram m_program;
	int m_vertexAttr = -1;
	int m_colorsAttr = -1;
	int m_textAttr = -1;
	int m_matrixUniform = -1;

	int m_iVideoWidth = 0;
	int m_iVideoHeight = 0;
	GLuint m_videoTextureID[3] = { 0 };

	const QuickVideoRenderObject* m_pRenderObject = nullptr;
	QuickVideoRenderObject::QuickRenderData m_renderData;
};