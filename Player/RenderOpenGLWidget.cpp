#include "RenderOpenGLWidget.h"


VideoGLWidget::VideoGLWidget(QWidget* parent):
	QOpenGLWidget(parent)
{
}

VideoGLWidget::~VideoGLWidget()
{
	if (m_pQuadVertices)
	{
		delete[] m_pQuadVertices;
	}
}

void VideoGLWidget::SetVideoSize(int w, int h)
{
	m_iVideoWidth = w;
	m_iVideoHeight = h;

	makeCurrent();

	CalculateDisplay(width(), height());
	CreateVideoTexture(w, h);

	doneCurrent();
}

void VideoGLWidget::CalculateDisplay(int canvasWidth, int canvasHeight)
{
	int content_w = m_iVideoWidth;
	int content_h = m_iVideoHeight;

	if (content_w == 0 || content_h == 0)
	{
		content_w = canvasWidth;
		content_h = canvasHeight;
	}

	float sw = 1.0 * canvasWidth / content_w;
	float sh = 1.0 * canvasHeight / content_h;
	sw = sw < sh ? sw : sh;
	float w1 = sw * content_w;
	float h1 = sw * content_h;

	float temp[20]{
		-w1 / 2.0f,  -h1 / 2.0f, 0.0f,
		0.0, 1.0,
		-w1 / 2.0f, h1 / 2.0f, 0.0f,
		0.0,0.0,
		w1 / 2.0f,  -h1 / 2.0f, 0.0f,
		1.0,1.0,
		w1 / 2.0f,  h1 / 2.0f, 0.0f,
		1.0,0
	};

	if (!m_pQuadVertices)
	{
		m_pQuadVertices = new float[20];
	}
	memcpy(m_pQuadVertices, temp, sizeof(temp));
}

void VideoGLWidget::CreateVideoTexture(int videoWidth, int videoHeight, const void* pData)
{
	if (m_videoTextureID != 0)
	{
		glDeleteTextures(1, &m_videoTextureID);
		m_videoTextureID = 0;
	}

	glGenTextures(1, &m_videoTextureID);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (pData == nullptr)
	{
		auto pTemp = new uint8_t[videoWidth * videoHeight * 4];
		glTexImage2D(GL_TEXTURE_2D, 0, 4, videoWidth, videoHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pTemp);
		delete[] pTemp;
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, 4, videoWidth, videoHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pData);
	}
}

void VideoGLWidget::UpdateVideoTexture(int videoWidth, int videoHeight, const void* pData)
{
	if (m_videoTextureID == 0)
	{
		LOG() << "no video texture";
		return;
	}

	glBindTexture(GL_TEXTURE_2D, m_videoTextureID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_BGRA, GL_UNSIGNED_BYTE, pData);
}

void VideoGLWidget::cleanup()
{
	if (m_videoTextureID)
	{
		glDeleteTextures(1, &m_videoTextureID);
		m_videoTextureID = 0;
	}

	if (m_fragmentShaderID != 0)
	{
		glDetachShader(m_programID, m_fragmentShaderID);
		glDeleteShader(m_fragmentShaderID);
		m_fragmentShaderID = 0;
	}
	if (m_vertexShaderID != 0)
	{
		glDetachShader(m_programID, m_vertexShaderID);
		glDeleteShader(m_vertexShaderID);
		m_vertexShaderID = 0;
	}

	glDeleteProgram(m_programID);
	m_programID = 0;
}

void VideoGLWidget::initializeGL()
{
	connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &VideoGLWidget::cleanup);
	initializeOpenGLFunctions();

	int iStatus = 0;

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);

	glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
	//glClearColor(1.0f, 0, 0, 1.0f);

	m_vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	const char* pVertexShaderSource =
		"attribute vec4 vertex;\n"
		"attribute vec4 texCoord;\n"
		"varying vec4 texc;\n"
		"uniform mat4 projMatrix;\n"
		"uniform mat4 mvMatrix;\n"
		"void main() {\n"
		"   gl_Position = projMatrix * mvMatrix * vertex;\n"
		"	texc = texCoord; \n"
		"}";
	int iVertexShaderSourceLength = strlen(pVertexShaderSource);
	
	glShaderSource(m_vertexShaderID, 1, &pVertexShaderSource, &iVertexShaderSourceLength);
	glCompileShader(m_vertexShaderID);
	glGetShaderiv(m_vertexShaderID, GL_COMPILE_STATUS, &iStatus);
	if (!iStatus)
	{
		char buf[2048] = { 0 };
		GLsizei got = 0;

		glGetShaderInfoLog(m_vertexShaderID, sizeof(buf), &got, buf);
		LOG() << buf;

		glDeleteShader(m_vertexShaderID);
		m_vertexShaderID = 0;
		return;
	}

	m_fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
	const char* pFragmentShaderSource =
		"uniform sampler2D texture;\n"
		"varying vec4 texc;\n"
		"void main(void)\n"
		"{\n"
		"	gl_FragColor = texture2D(texture, texc.st); \n"
		"}";

	int iFragmentShaderSourceLength = strlen(pFragmentShaderSource);
	glShaderSource(m_fragmentShaderID, 1, &pFragmentShaderSource, &iFragmentShaderSourceLength);
	glCompileShader(m_fragmentShaderID);
	glGetShaderiv(m_fragmentShaderID, GL_COMPILE_STATUS, &iStatus);
	if (!iStatus)
	{
		char buf[2048] = { 0 };
		GLsizei got = 0;

		glGetShaderInfoLog(m_fragmentShaderID, sizeof(buf), &got, buf);
		LOG() << buf;

		glDeleteShader(m_fragmentShaderID);
		glDeleteShader(m_vertexShaderID);
		m_vertexShaderID = 0;
		m_fragmentShaderID = 0;
		return;
	}

	m_programID = glCreateProgram();
	glAttachShader(m_programID, m_vertexShaderID);
	glAttachShader(m_programID, m_fragmentShaderID);
	glLinkProgram(m_programID);
	glGetProgramiv(m_programID, GL_LINK_STATUS, &iStatus);
	if (!iStatus)
	{
		char buf[2048] = { 0 };
		GLsizei got = 0;
		glGetProgramInfoLog(m_programID, sizeof(buf), &got, buf);
		LOG() << buf;

		glDeleteShader(m_fragmentShaderID);
		glDeleteShader(m_vertexShaderID);
		m_vertexShaderID = 0;
		m_fragmentShaderID = 0;
		glDeleteProgram(m_programID);
		m_programID = 0;
		return;
	}

	glUseProgram(m_programID);

	m_attr_vertex_id   = glGetAttribLocation(m_programID, "vertex");
	m_attr_tecCoord_id = glGetAttribLocation(m_programID, "texCoord");
}

void VideoGLWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);

	m_matProjection.setToIdentity();
	m_matProjection.perspective(60.0f, 1.0f * w / h, 1.0f, 5000.0f);

	m_eye.setZ(h / 2.0f / tan(30.0 * 3.1415926 / 180.0));
	m_eye.setX(0);
	m_eye.setY(0);

	m_matModelView.setToIdentity();
	m_matModelView.lookAt(m_eye, QVector3D(0, 0, 0), QVector3D(0, 1.0, 0));

	GLint location = glGetUniformLocation(m_programID, "projMatrix");
	glUniformMatrix4fv(location, 1, GL_FALSE, m_matProjection.data());

	location = glGetUniformLocation(m_programID, "mvMatrix");
	glUniformMatrix4fv(location, 1, GL_FALSE, m_matModelView.data());

	CalculateDisplay(w, h);

	glEnableVertexAttribArray(m_attr_vertex_id);
	glEnableVertexAttribArray(m_attr_tecCoord_id);
	glVertexAttribPointer(m_attr_vertex_id, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), m_pQuadVertices);
	glVertexAttribPointer(m_attr_tecCoord_id, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), m_pQuadVertices + 3);
}

void VideoGLWidget::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT);
	if (m_videoTextureID != 0)
	{
		glBindTexture(GL_TEXTURE_2D, m_videoTextureID);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}


VideoRenderOpenGLWidget::VideoRenderOpenGLWidget(VideoGLWidget* pWidget):
	m_pVideoWidget(pWidget)
{
}

VideoRenderOpenGLWidget::~VideoRenderOpenGLWidget()
{
}

int VideoRenderOpenGLWidget::ConfigureRender(RenderInfo mediaInfo)
{
	auto type = mediaInfo.find("type")->second.to<std::string>();

	if (type == "init")
	{
		if (!mediaInfo.contain_key_value("hasVideo", 1))
		{
			return CodeNo;
		}

		auto width = mediaInfo.find("width")->second.to<int>();
		auto height = mediaInfo.find("height")->second.to<int>();
		auto rate = mediaInfo.find("videoRate")->second.to<double>(1.0);
		auto format = mediaInfo.find("videoFormat")->second.to<int>(-1);

		return ConfigureRender(width, height, (AVPixelFormat)format);
	}
	else
	{
		return CodeNo;
	}

	return CodeOK;
}

int VideoRenderOpenGLWidget::ConfigureRender(int width, int height, AVPixelFormat format)
{
	if (!m_pVideoWidget)
	{
		return CodeNo;
	}

	if (width < 1 || height < 1 || format == AV_PIX_FMT_NONE)
	{
		return CodeNo;
	}

	m_iWidth = width;
	m_iHeight = height;
	m_format = format;

	m_pVideoWidget->SetVideoSize(width, height);

	if (format != AV_PIX_FMT_BGRA)
	{
		m_pImageConvert.reset(new FFmpegImageScale());
		if (CodeOK != m_pImageConvert->Configure(width, height, format, width, height, AV_PIX_FMT_BGRA))
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

int VideoRenderOpenGLWidget::UpdataFrame(FrameHolderPtr data)
{
	if (!data)
	{
		return CodeInvalidParam;
	}

	AVFrame* pFrame = data->FrameData();
	uint8_t* pImagedata = nullptr;

	if (pFrame->width != m_iWidth ||
		pFrame->height != m_iHeight ||
		pFrame->format != m_format)
	{
		ConfigureRender(
			pFrame->width,
			pFrame->height,
			(AVPixelFormat)pFrame->format);
	}

	if (m_pImageConvert)
	{
		int res = m_pImageConvert->Convert(pFrame->data, pFrame->linesize);
		if (res < 0)
		{
			return CodeNo;
		}

		pImagedata = m_pImageConvert->Frame()->data[0];
	}
	else
	{
		pImagedata = pFrame->data[0];
	}

	m_pVideoWidget->UpdateVideoTexture(m_iWidth, m_iHeight, pImagedata);
	m_pVideoWidget->update();

	return CodeOK;
}

int VideoRenderOpenGLWidget::Start()
{
	return CodeOK;
}

int VideoRenderOpenGLWidget::Pause() 
{
	return CodeOK;
}

int VideoRenderOpenGLWidget::Stop()
{
	return CodeOK;
}

int VideoRenderOpenGLWidget::Seek(int64_t)
{
	return CodeOK;
}

int VideoRenderOpenGLWidget::GetRenderTime(int64_t&)
{
	return CodeOK;
}