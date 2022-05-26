#pragma once

#include "IRender.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>

#include "FFmpegDemuxer.h"

class RenderRgb : protected QOpenGLFunctions
{
public:
	explicit RenderRgb();
	~RenderRgb();

	bool IsInited() { return m_bInited; }

	void SetIsOpenGLES(bool);
	void SetSize(int video_width, int video_height, int canvas_width, int canvas_height);

	virtual void Init();
	virtual void Destroy();
	virtual void Paint();
	virtual void Resize(int, int);
	virtual void UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize);

protected:
	virtual void CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData = nullptr);
	virtual const char* GetShaderSoure(int);

	void CalculateMat(int canvasWidth, int canvasHeight);
	void CalculateVertex(int canvasWidth, int canvasHeight);

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
	bool m_bClearBackground = true;
	bool m_bIsOpenGLES = false;
	bool m_bInited = false;
};

class RenderYUV420P : public RenderRgb
{
public:
	void Resize(int, int);
	void UpdateVideoTexture(int, int, const uint8_t* const*, const int*);
protected:
	void CreateVideoTexture(int, int, const uint8_t* const*);
	const char* GetShaderSoure(int);
};

class RenderNV12 : public RenderRgb
{
public:
	void Resize(int, int);
	void UpdateVideoTexture(int, int, const uint8_t* const*, const int*);

protected:
	void CreateVideoTexture(int, int, const uint8_t* const*);
	const char* GetShaderSoure(int);
};

class VideoRenderOpenGLWidget;
class VideoOpenGLWidget : public QOpenGLWidget
{
	Q_OBJECT
public:
	explicit VideoOpenGLWidget(QWidget* parent);
	~VideoOpenGLWidget();
	AVPixelFormat GetSupportedPixformat();
	void SetRenderDataSource(VideoRenderOpenGLWidget*);

	void SetVideoSize(int, int);
	void UpdateTexture(int, int, const uint8_t* const*, const int*);

protected:
	void initializeGL() override;
	void resizeGL(int, int) override;
	void paintGL() override;

private:
	RenderRgb*               m_pRender = nullptr;
	VideoRenderOpenGLWidget* m_pDataSource = nullptr;
};

class VideoRenderOpenGLWidget : public IRender
{
public:
	VideoRenderOpenGLWidget(VideoOpenGLWidget* pWidget);
	~VideoRenderOpenGLWidget();

	int PrepareRender();
	int ConfigureRender(RenderInfo) override;
	int UpdataFrame(FrameHolderPtr data) override;

	int Start() override;
	int Stop() override;
	int Pause(bool) override;
	int Reset() override;
	int GetRenderTime(int64_t&) override;

protected:
	int ConfigureRender(int, int, AVPixelFormat);

protected:
	VideoOpenGLWidget* m_pVideoWidget = nullptr;
	FrameHolderPtr     m_videoFrameData;

	int m_iWidth  = 0;
	int m_iHeight = 0;
	AVPixelFormat m_format = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pImageConvert;
};