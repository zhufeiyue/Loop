#include "QuickVideoRender.h"

void QuickVideoRenderObject::Register()
{
    qmlRegisterType<QuickVideoRenderObject>("QuickVideoRendering", 1, 0, "VideoRender");
}

QuickVideoRenderObject::QuickVideoRenderObject(QQuickItem* parent) :
    QQuickFramebufferObject(parent)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

    setFlag(QQuickItem::ItemHasContents);
}

QuickVideoRenderObject::~QuickVideoRenderObject()
{
}

QQuickFramebufferObject::Renderer* 
QuickVideoRenderObject::createRenderer() const
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();
    return new VideoRender(this);
}


VideoRender::VideoRender(const QuickVideoRenderObject* pRenderObject):
    m_pRenderObject(pRenderObject)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	initializeOpenGLFunctions();

    const char* vsrc =
        "attribute vec4 vertex;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 texCoordOut;\n"

        "uniform mat4 matrix;\n"
        "void main(void)\n"
        "{\n"
        "   gl_Position = matrix * vertex;\n"
        "   texCoordOut = texCoord;"
        "}\n";

    const char* fsrc =
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

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    if (!m_program.link())
    {
        LOG() << m_program.log().toStdString();
        return;
    }

    m_vertexAttr = m_program.attributeLocation("vertex");
    m_textAttr = m_program.attributeLocation("texCoord");
    m_matrixUniform = m_program.uniformLocation("matrix");
}

VideoRender::~VideoRender()
{
}

void VideoRender::render()
{
    //LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

    if (!m_pRenderObject || !m_pRenderObject->GetDataCallback())
    {
        return;
    }
    if (CodeOK != m_pRenderObject->GetDataCallback()(m_renderData))
    {
        return;
    }

    if (m_renderData.width != m_iVideoWidth ||
        m_renderData.height != m_iVideoHeight)
    {
        m_iVideoWidth = m_renderData.width;
        m_iVideoHeight = m_renderData.height;

        CreateVideoTexture(m_iVideoWidth, m_iVideoHeight);
    }

    UpdateVideoTexture(m_iVideoWidth, m_iVideoHeight, m_renderData.pData, m_renderData.pLineSize);

    int width = framebufferObject()->width();
    int height = framebufferObject()->height();

    QVector<QVector3D> vertices;
    vertices.push_back(QVector3D(0, 0, 0));
    vertices.push_back(QVector3D(0, height, 0));
    vertices.push_back(QVector3D(width, 0, 0));
    vertices.push_back(QVector3D(width, height, 0));

    QVector<QVector2D> texCoords;
    texCoords.push_back(QVector2D(0, 1.0));
    texCoords.push_back(QVector2D(0, 0));

    texCoords.push_back(QVector2D(1.0, 1.0));
    texCoords.push_back(QVector2D(1.0, 0));


    QMatrix4x4 matrix;
    matrix.ortho(QRect(0, 0, width, height));

    m_program.bind();
    m_program.setUniformValue(m_matrixUniform, matrix);

    m_program.enableAttributeArray(m_vertexAttr);
    m_program.enableAttributeArray(m_textAttr);
    m_program.setAttributeArray(m_vertexAttr, vertices.constData());
    m_program.setAttributeArray(m_textAttr, texCoords.constData());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());

    m_program.disableAttributeArray(m_vertexAttr);
    m_program.disableAttributeArray(m_textAttr);
    m_program.release();
}

void VideoRender::synchronize(QQuickFramebufferObject*)
{
    //LOG() << __FUNCTION__;
}

QOpenGLFramebufferObject* VideoRender::createFramebufferObject(const QSize& size)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	QOpenGLFramebufferObjectFormat format;
	//format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	//format.setSamples(4);

	return new QOpenGLFramebufferObject(size, format);
}

void VideoRender::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoWidth / 2, videoHeight / 2, 0, GL_RG, GL_UNSIGNED_BYTE, ptemp.get());
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoWidth / 2, videoHeight / 2, 0, GL_RG, GL_UNSIGNED_BYTE, pData[1]);
    }

    auto location_textureY = glGetUniformLocation(m_program.programId(), "textureY");
    auto location_textureUV = glGetUniformLocation(m_program.programId(), "textureUV");
    glUniform1i(location_textureY, 0);
    glUniform1i(location_textureUV, 1);
}

void VideoRender::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
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
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[1] / 2);
    }
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RG, GL_UNSIGNED_BYTE, pData[1]);

    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}