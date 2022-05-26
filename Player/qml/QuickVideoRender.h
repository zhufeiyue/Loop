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
		int c_width = 0;
		int c_height = 0;
		int format = -1;
		const int* pLineSize = nullptr;
		const uint8_t* const* pData = nullptr;
	};

	typedef std::function<int(QuickRenderData&)> RenderDataFunc;

public:
	QuickVideoRenderObject(QQuickItem* parent = nullptr);
	~QuickVideoRenderObject();

	void SetDataCallback(RenderDataFunc func)
	{
		m_funcGetCurrentVideoFrame = std::move(func);
	}

	const RenderDataFunc& GetDataCallback() const
	{
		return m_funcGetCurrentVideoFrame;
	}

	std::vector<AVPixelFormat> GetSupportedPixformat() const
	{
		return std::vector<AVPixelFormat>
		{
			AV_PIX_FMT_BGRA, AV_PIX_FMT_NV12, AV_PIX_FMT_YUV420P
		};
	}

private:
	Renderer* createRenderer() const override;

private:
	RenderDataFunc m_funcGetCurrentVideoFrame;
};

class QuickRenderBgra : protected QOpenGLFunctions
{
public:
	QuickRenderBgra();
	virtual ~QuickRenderBgra();
			void ClearBackground();
	virtual void Render(QuickVideoRenderObject::QuickRenderData&);
	virtual int  CreateProgram();
private:
	void CalculateMat(int canvasWidth, int canvasHeight);
	void CalculateVertex(int canvasWidth, int canvasHeight);

	virtual void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData = nullptr);
	virtual void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	QOpenGLShaderProgram m_program;
	int m_vertexAttr = -1;
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
};

class QuickRenderYUV420P : public QuickRenderBgra
{
public:
	int CreateProgram() override;
protected:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData) override;
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize) override;
};

class QuickRenderNV12 : public QuickRenderBgra
{
public:
	int CreateProgram() override;
protected:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData) override;
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize) override;
};

class Render : public QQuickFramebufferObject::Renderer
{
public:
	Render(const QuickVideoRenderObject*);
	~Render();

	void render() override;
	void synchronize(QQuickFramebufferObject*) override;
	QOpenGLFramebufferObject* createFramebufferObject(const QSize&) override;

private:
	const QuickVideoRenderObject* m_pRenderObject = nullptr;
	QuickVideoRenderObject::QuickRenderData m_renderData;

	int m_pixFormat = -1;
	QuickRenderBgra* m_pRender = new QuickRenderNV12();
};