#pragma once

#include "IRender.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>

#include "FFmpegDemuxer.h"

// render rgba data
class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT
public:
	explicit VideoGLWidget(QWidget* parent);
	~VideoGLWidget();
	void SetVideoSize(int w, int h);
	virtual void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	virtual void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData = nullptr);
	virtual const char* GetShaderSoure(int);

	void CalculateMat(int canvasWidth, int canvasHeight);
	void CalculateVertex(int canvasWidth, int canvasHeight);

protected:
	void cleanup();
	void initializeGL() override;
	void resizeGL(int, int) override;
	void paintGL() override;

protected:
	GLuint m_programID        = 0;
	GLuint m_vertexShaderID   = 0;
	GLuint m_fragmentShaderID = 0;
	GLuint m_videoTextureID[3] = { 0 };

	GLuint m_bufferID = 0;
	GLint m_attr_vertex_id   = -1;
	GLint m_attr_tecCoord_id = -1;

	QMatrix4x4 m_matProjection;
	QMatrix4x4 m_matModelView;
	QVector3D  m_eye;

	int m_iVideoWidth = 0;
	int m_iVideoHeight = 0;
	GLfloat* m_pQuadVertices = nullptr;
	bool m_bClearBackground=true;
};

// render yuv420p data
class VideoGLWidgetYUV420P : public VideoGLWidget
{
public:
	explicit VideoGLWidgetYUV420P(QWidget*);
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData);
	const char* GetShaderSoure(int);

protected:
	void resizeGL(int, int);
};

// render nv12 data
class VideoGLWidgetNV12 : public VideoGLWidget
{
public:
	explicit VideoGLWidgetNV12(QWidget*);
	void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData);
	const char* GetShaderSoure(int);

protected:
	void resizeGL(int, int);
};

class VideoRenderOpenGLWidget : public IRender
{
public:
	VideoRenderOpenGLWidget(VideoGLWidget* pWidget);
	~VideoRenderOpenGLWidget();

	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Pause() override;
	int Stop() override;
	int Seek(int64_t) override;
	int GetRenderTime(int64_t&) override;

protected:
	int ConfigureRender(int, int, AVPixelFormat);

protected:
	VideoGLWidget* m_pVideoWidget = nullptr;

	int m_iWidth = 0;
	int m_iHeight = 0;
	AVPixelFormat m_format = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pImageConvert;
};