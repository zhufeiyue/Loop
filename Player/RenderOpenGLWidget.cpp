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

	CalculateVertex(width(), height());
	CreateVideoTexture(w, h);

	doneCurrent();
}

void VideoGLWidget::CalculateMat(int w, int h)
{
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
}

void VideoGLWidget::CalculateVertex(int canvasWidth, int canvasHeight)
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

	GLfloat temp[]{
		-w1 / 2.0f, -h1 / 2.0f, 0.0f,
		0.0, 1.0,0,
		-w1 / 2.0f, h1 / 2.0f, 0.0f,
		0.0, 0.0,0,
		w1 / 2.0f, -h1 / 2.0f, 0.0f,
		1.0, 1.0,0,
		w1 / 2.0f, h1 / 2.0f, 0.0f,
		1.0, 0,0
	};

	if (!m_pQuadVertices)
	{
		m_pQuadVertices = new GLfloat[sizeof(temp) / sizeof(GLfloat)];
	}
	memcpy(m_pQuadVertices, temp, sizeof(temp));

	m_attr_vertex_id = glGetAttribLocation(m_programID, "vertex");
	m_attr_tecCoord_id = glGetAttribLocation(m_programID, "texCoord");

	if (m_bufferID == 0)
	{
		glGenBuffers(1, &m_bufferID);
	}
	if (0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(temp), m_pQuadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(m_attr_vertex_id);
		glEnableVertexAttribArray(m_attr_tecCoord_id);
		glVertexAttribPointer(m_attr_vertex_id, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), m_pQuadVertices);
		glVertexAttribPointer(m_attr_tecCoord_id, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), m_pQuadVertices + 3);
	}
	else
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_bufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(temp), m_pQuadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(m_attr_vertex_id);
		glEnableVertexAttribArray(m_attr_tecCoord_id);
		glVertexAttribPointer(m_attr_vertex_id, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), 0);
		glVertexAttribPointer(m_attr_tecCoord_id, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), reinterpret_cast<void*>(3 * sizeof(GLfloat)));
	}
}

const char* VideoGLWidget::VideoGLWidget::GetShaderSoure(int type)
{
	if (type == GL_VERTEX_SHADER)
	{
		static const char* pVertexShaderSource =
			"attribute vec4 vertex;\n"
			"attribute vec2 texCoord;\n"
			"varying vec2 texc;\n"
			"uniform mat4 projMatrix;\n"
			"uniform mat4 mvMatrix;\n"
			"void main() {\n"
			"   gl_Position = projMatrix * mvMatrix * vertex;\n"
			"	texc = texCoord; \n"
			"}";

		return pVertexShaderSource;
	}
	else if (type == GL_FRAGMENT_SHADER)
	{
		static const char* pFragmentShaderSource_RGB =
			"#ifdef GL_ES\n"
				"precision lowp float;\n"
			"#endif\n"

			"uniform sampler2D textureRGB;\n"
			"varying vec2 texc;\n"
			"void main(void)\n"
			"{\n"
			"	gl_FragColor = texture2D(textureRGB, texc.st); \n"
			"}";

		return pFragmentShaderSource_RGB;
	}
	else
	{
		return nullptr;
	}
}

void VideoGLWidget::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
	if (m_programID == 0)
	{
		return;
	}

	if (m_videoTextureID[0] != 0)
	{
		glDeleteTextures(1, &m_videoTextureID[0]);
		m_videoTextureID[0] = 0;
	}

	glGenTextures(1, &m_videoTextureID[0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (pData == nullptr)
	{
		auto pTemp = new uint8_t[videoWidth * videoHeight * 4];
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, videoWidth, videoHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pTemp);
		delete[] pTemp;
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, videoWidth, videoHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pData[0]);
	}

	auto textureLocation = glGetUniformLocation(m_programID, "textureRGB");
	glUniform1i(textureLocation, 0);
}

void VideoGLWidget::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int*)
{
	if (m_videoTextureID == 0)
	{
		LOG() << "no video texture";
		return;
	}

	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_BGRA, GL_UNSIGNED_BYTE, pData[0]);
}

void VideoGLWidget::cleanup()
{
	for (int i = 0; i < sizeof(m_videoTextureID) / sizeof(m_videoTextureID[0]); ++i)
	{
		glDeleteTextures(1, &m_videoTextureID[i]);
		m_videoTextureID[i] = 0;
	}

	if (m_bufferID != 0)
	{
		glDeleteBuffers(1, &m_bufferID);
		m_bufferID = 0;
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

	LOG() << glGetString(GL_VERSION);
	m_bIsOpenGLES = context()->isOpenGLES();

	int iStatus = 0;

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);

	m_vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	const char* pVertexShaderSource = GetShaderSoure(GL_VERTEX_SHADER);
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
	const char* pFragmentShaderSource = GetShaderSoure(GL_FRAGMENT_SHADER);

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
}

void VideoGLWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);

	CalculateMat(w, h);
	CalculateVertex(w, h);
	m_bClearBackground = true;


	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
}

void VideoGLWidget::paintGL()
{
	if (m_bIsOpenGLES || m_bClearBackground)
	{
		m_bClearBackground = false;

		glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	if (m_videoTextureID[0] != 0)
	{
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
}


VideoGLWidgetYUV420P::VideoGLWidgetYUV420P(QWidget*parent):
	VideoGLWidget(parent)
{
}

void VideoGLWidgetYUV420P::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
{
	bool bSetRowLength = false;

	// 如果数据的行宽，大于需要，通过设置GL_UNPACK_ROW_LENGTH参数，可以达到裁剪、抠图的目的
	if (pLineSize[0] > videoWidth)
	{
		bSetRowLength = true;
	}
	else if (pLineSize[0] == videoWidth)
	{
	}
	else
	{
		LOG() << "fatal error: " << __FUNCTION__ <<  " lineSize less than video width";
		return;
	}

	if (pLineSize[1] != pLineSize[2])
	{
		LOG() << "fatal error: " << __FUNCTION__ <<  " U V lineSize not equal";
		return;
	}

	if (bSetRowLength)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[0]);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

	if (bSetRowLength)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[1]);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RED, GL_UNSIGNED_BYTE, pData[1]);

	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[2]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RED, GL_UNSIGNED_BYTE, pData[2]);

	// 恢复为0
	if (bSetRowLength)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void VideoGLWidgetYUV420P::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

	glDeleteTextures(3, m_videoTextureID);
	glGenTextures(3, m_videoTextureID);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	std::unique_ptr<uint8_t[]> ptemp;
	ptemp.reset(new uint8_t[videoWidth * videoHeight * 4]);
	if (!pData)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, ptemp.get());
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pData[0]);
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (!pData)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, ptemp.get());
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, pData[1]);
	}

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (!pData)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, ptemp.get());
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, pData[2]);
	}

	auto location_textureY = glGetUniformLocation(m_programID, "textureY");
	auto location_textureU = glGetUniformLocation(m_programID, "textureU");
	auto location_textureV = glGetUniformLocation(m_programID, "textureV");
	glUniform1i(location_textureY, 0);
	glUniform1i(location_textureU, 1);
	glUniform1i(location_textureV, 2);
}

const char* VideoGLWidgetYUV420P::GetShaderSoure(int type)
{
	if (type == GL_VERTEX_SHADER)
	{
		static const char* pVertexShaderSource =
			"attribute vec4 vertex;\n"
			"attribute vec2 texCoord;\n"
			"varying vec2 texCoordOut;\n"
			"uniform mat4 projMatrix;\n"
			"uniform mat4 mvMatrix;\n"
			"void main() {\n"
			"   gl_Position = projMatrix * mvMatrix * vertex;\n"
			"	texCoordOut = texCoord; \n"
			"}";

		return pVertexShaderSource;
	}
	else if (type == GL_FRAGMENT_SHADER)
	{
		static const char* pFragmentShaderSource_YUV420P =
			"#ifdef GL_ES\n"
				"precision lowp float;\n"
			"#endif\n"

			"uniform sampler2D textureY;\n"
			"uniform sampler2D textureU;\n"
			"uniform sampler2D textureV;\n"
			"varying vec2 texCoordOut;\n"

			"const vec3 yuv2r = vec3(1.164, 0.0, 1.596);\n"
			"const vec3 yuv2g = vec3(1.164, -0.391, -0.813);\n"
			"const vec3 yuv2b = vec3(1.164, 2.018, 0.0);\n"

			"void main() {\n"
				"vec3 yuv, rgb;\n"

				"yuv.x = texture2D(textureY, texCoordOut).r - 0.0625;\n"
				"yuv.y = texture2D(textureU, texCoordOut).r - 0.5;\n"
				"yuv.z = texture2D(textureV, texCoordOut).r - 0.5;\n"

				"rgb.x = dot(yuv, yuv2r);\n"
				"rgb.y = dot(yuv, yuv2g);\n"
				"rgb.z = dot(yuv, yuv2b);\n"

				"gl_FragColor = vec4(rgb, 1.0);"
			"}";

		return pFragmentShaderSource_YUV420P;
	}
	return nullptr;
}

void VideoGLWidgetYUV420P::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);

	CalculateMat(w, h);
	CalculateVertex(w, h);
	m_bClearBackground = true;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[2]);
}


VideoGLWidgetNV12::VideoGLWidgetNV12(QWidget* parent):
	VideoGLWidget(parent)
{
}

void VideoGLWidgetNV12::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
{
	bool bSetRowLength = false;

	if (pLineSize[0] > videoWidth)
	{
		bSetRowLength = true;
	}
	else if (pLineSize[0] == videoWidth)
	{
	}
	else
	{
		LOG() << "fatal error: " << __FUNCTION__ << " lineSize less than video width";
		return;
	}

	if (pLineSize[1] != pLineSize[0])
	{
		LOG() << "fatal error: " << __FUNCTION__ << " Y UV lineSize not equal";
		return;
	}

	if (bSetRowLength)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[0]);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

	if (bSetRowLength)
	{
		// ffmpeg中的lineSize以字节为单位，GL_UNPACK_ROW_LENGTH参数需要像素为单位。UV分量每行的像素数，为图像宽的一半，每个像素需要两个字节
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[1] / 2);
	}
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RG, GL_UNSIGNED_BYTE, pData[1]);

	if (bSetRowLength)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void VideoGLWidgetNV12::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

	glDeleteTextures(2, m_videoTextureID);
	glGenTextures(2, m_videoTextureID);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	std::unique_ptr<uint8_t[]> ptemp;
	ptemp.reset(new uint8_t[videoWidth * videoHeight * 4]);
	if (!pData)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, ptemp.get());
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pData[0]);
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	if (!pData)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoWidth/2, videoHeight/2, 0, GL_RG, GL_UNSIGNED_BYTE, ptemp.get());
	}
	else
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoWidth/2, videoHeight/2, 0, GL_RG, GL_UNSIGNED_BYTE, pData[1]);
	}

	auto location_textureY = glGetUniformLocation(m_programID, "textureY");
	auto location_textureUV = glGetUniformLocation(m_programID, "textureUV");
	glUniform1i(location_textureY, 0);
	glUniform1i(location_textureUV, 1);
}

const char* VideoGLWidgetNV12::GetShaderSoure(int type)
{
	if (type == GL_VERTEX_SHADER)
	{
		static const char* pVertexShaderSource =
			"attribute vec4 vertex;\n"
			"attribute vec2 texCoord;\n"
			"varying vec2 texCoordOut;\n"
			"uniform mat4 projMatrix;\n"
			"uniform mat4 mvMatrix;\n"
			"void main() {\n"
			"   gl_Position = projMatrix * mvMatrix * vertex;\n"
			"	texCoordOut = texCoord; \n"
			"}";

		return pVertexShaderSource;
	}
	else if (type == GL_FRAGMENT_SHADER)
	{
		static const char* pFragmentShaderSource_NV12 =
			"#ifdef GL_ES\n"
				"precision lowp float;\n"
			"#endif\n"

			"uniform sampler2D textureY;\n"
			"uniform sampler2D textureUV;\n"
			"varying vec2 texCoordOut;\n"

			"const vec3 yuv2r = vec3(1.164, 0.0, 1.596);\n"
			"const vec3 yuv2g = vec3(1.164, -0.391, -0.813);\n"
			"const vec3 yuv2b = vec3(1.164, 2.018, 0.0);\n"

			"void main(void)\n"
			"{\n"
				"vec3 yuv; \n"
				"vec3 rgb; \n"
				"yuv.x = texture2D(textureY, texCoordOut.st).r - 0.0625; \n"
				"yuv.y = texture2D(textureUV, texCoordOut.st).r - 0.5; \n"
				"yuv.z = texture2D(textureUV, texCoordOut.st).g - 0.5; \n"

				"rgb.x = dot(yuv, yuv2r);\n"
				"rgb.y = dot(yuv, yuv2g);\n"
				"rgb.z = dot(yuv, yuv2b);\n"

				"gl_FragColor = vec4(rgb, 1.0);"
			"}\n";

		return pFragmentShaderSource_NV12;
	}
	return nullptr;
}

void VideoGLWidgetNV12::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);

	CalculateMat(w, h);
	CalculateVertex(w, h);
	m_bClearBackground = true;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
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

	AVPixelFormat formatWant = AV_PIX_FMT_BGRA;
	if (dynamic_cast<VideoGLWidgetNV12*>(m_pVideoWidget) != nullptr)
	{
		formatWant = AV_PIX_FMT_NV12;
	}
	else if (dynamic_cast<VideoGLWidgetYUV420P*>(m_pVideoWidget) != nullptr)
	{
		formatWant = AV_PIX_FMT_YUV420P;
	}

	if (format != formatWant)
	{
		m_pImageConvert.reset(new FFmpegImageScale());
		if (CodeOK != m_pImageConvert->Configure(width, height, format, width, height, formatWant))
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
	auto pImagedata = pFrame->data;
	auto pImageLineSize = pFrame->linesize;

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

		pImagedata = m_pImageConvert->Frame()->data;
		pImageLineSize = m_pImageConvert->Frame()->linesize;
	}

	m_pVideoWidget->UpdateVideoTexture(m_iWidth, m_iHeight, pImagedata, pImageLineSize);
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