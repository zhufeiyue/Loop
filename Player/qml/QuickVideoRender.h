#pragma once

#include <common/Log.h>
#include <Player/FFmpegDemuxer.h>

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

	AVPixelFormat GetSupportedPixformat() const
	{
		return AV_PIX_FMT_YUV420P;
		return AV_PIX_FMT_BGRA;
		return AV_PIX_FMT_NV12;
	}

private:
	Renderer* createRenderer() const override;

private:
	std::function<int(QuickRenderData&)> m_funcGetCurrentVideoFrame;
};

class QuickRenderBgra : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
	QuickRenderBgra(const QuickVideoRenderObject*);
	~QuickRenderBgra();

	void render() override;
	void synchronize(QQuickFramebufferObject*) override;
	QOpenGLFramebufferObject* createFramebufferObject(const QSize&) override;

	virtual int CreateProgram();
private:
	void CalculateMat(int canvasWidth, int canvasHeight);
	void CalculateVertex(int canvasWidth, int canvasHeight);

	virtual void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData = nullptr);
	virtual void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	QOpenGLShaderProgram m_program;
	int m_vertexAttr = -1;
	int m_colorsAttr = -1;
	int m_textAttr = -1;
	int m_matrixUniform = -1;

	int m_iVideoWidth = 0;
	int m_iVideoHeight = 0;
	int m_iCanvasWidth = 0;
	int m_iCanvasHeight = 0;

	QVector<QVector3D> m_vertices;
	QVector<QVector2D> m_texCoords;
	QMatrix4x4 m_matrix;
	GLuint m_videoTextureID[3] = { 0 };

	const QuickVideoRenderObject* m_pRenderObject = nullptr;
	QuickVideoRenderObject::QuickRenderData m_renderData;
};

class QuickRenderYUV420P : public QuickRenderBgra
{
public:
	QuickRenderYUV420P(const QuickVideoRenderObject*);
	int CreateProgram() override;
protected:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData) override;
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize) override;
};