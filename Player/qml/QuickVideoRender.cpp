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
    return new Render(this);
}


Render::Render(const QuickVideoRenderObject* pRenderObject):
    m_pRenderObject(pRenderObject)
{
}

Render::~Render()
{
    if (m_pRender)
        delete m_pRender;
}

void Render::render()
{
    m_pRender->ClearBackground();

    if (!m_pRenderObject || !m_pRenderObject->GetDataCallback())
    {
        return;
    }

    if (CodeOK != m_pRenderObject->GetDataCallback()(m_renderData))
    {
        return;
    }

    if (m_renderData.format != m_pixFormat)
    {
        if (m_pRender)
        {
            delete m_pRender;
            m_pRender = nullptr;
        }

        if (m_renderData.format == AV_PIX_FMT_BGRA)
        {
            m_pRender = new QuickRenderBgra();
        }
        else if (m_renderData.format == AV_PIX_FMT_NV12)
        {
            m_pRender = new QuickRenderNV12();
        }
        else if (m_renderData.format == AV_PIX_FMT_YUV420P)
        {
            m_pRender = new QuickRenderYUV420P();
        }
        if (!m_pRender)
        {
            return;
        }

        m_pRender->CreateProgram();
        m_pixFormat = m_renderData.format;
    }

    m_renderData.c_width = framebufferObject()->width();
    m_renderData.c_height = framebufferObject()->height();
    m_pRender->Render(m_renderData);
}

void Render::synchronize(QQuickFramebufferObject*)
{
}

QOpenGLFramebufferObject* Render::createFramebufferObject(const QSize& size)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

    QOpenGLFramebufferObjectFormat format;
    //format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    //format.setSamples(4);
    return new QOpenGLFramebufferObject(size, format);
}


QuickRenderBgra::QuickRenderBgra()
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	initializeOpenGLFunctions();
    LOG() << "opengl version: " << glGetString(GL_VERSION);
}

QuickRenderBgra::~QuickRenderBgra()
{
    if (m_videoTextureID[0] != 0)
        glDeleteTextures(3, m_videoTextureID);
    LOG() << __FUNCTION__ << " " << glGetError();
    m_program.removeAllShaders();
}

void QuickRenderBgra::ClearBackground()
{
    glClearColor(0.078f, 0.078f, 0.078f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void QuickRenderBgra::Render(QuickVideoRenderObject::QuickRenderData& renderData)
{
    m_program.bind();

    if (renderData.width != m_iVideoWidth ||
        renderData.height != m_iVideoHeight)
    {
        m_iVideoWidth = renderData.width;
        m_iVideoHeight = renderData.height;
        CreateVideoTexture(m_iVideoWidth, m_iVideoHeight, renderData.pData);
    }

    UpdateVideoTexture(m_iVideoWidth, m_iVideoHeight, renderData.pData, renderData.pLineSize);

    int width = renderData.c_width;
    int height = renderData.c_height;

    if (m_iCanvasWidth != width || m_iCanvasHeight != height)
    {
        CalculateMat(width, height);
        CalculateVertex(width, height);

        m_iCanvasWidth = width;
        m_iCanvasHeight = height;
    }

    m_program.setUniformValue(m_matrixUniform, m_matrix);
    m_program.enableAttributeArray(m_vertexAttr);
    m_program.enableAttributeArray(m_textAttr);
    m_program.setAttributeArray(m_vertexAttr, m_vertices.constData());
    m_program.setAttributeArray(m_textAttr, m_texCoords.constData());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_program.disableAttributeArray(m_vertexAttr);
    m_program.disableAttributeArray(m_textAttr);
    m_program.release();
}

void QuickRenderBgra::CalculateMat(int w, int h)
{
    QMatrix4x4 projection;
    QMatrix4x4 mpdelView;
    QVector3D eye;

    projection.setToIdentity();
    projection.perspective(60.0f, 1.0f * w / h, 1.0f, 5000.0f);

    eye.setZ(h / 2.0f / tan(30.0 * 3.1415926 / 180.0));
    eye.setX(0);
    eye.setY(0);

    mpdelView.setToIdentity();
    mpdelView.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1.0, 0));

    m_matrix = projection * mpdelView;
}

void QuickRenderBgra::CalculateVertex(int canvasWidth, int canvasHeight)
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

    m_vertices.clear();
    m_vertices.push_back(QVector3D(-w1 / 2.0f, -h1 / 2.0f, 0.0f));
    m_vertices.push_back(QVector3D(-w1 / 2.0f, h1 / 2.0f, 0.0f));
    m_vertices.push_back(QVector3D(w1 / 2.0f, -h1 / 2.0f, 0.0f));
    m_vertices.push_back(QVector3D(w1 / 2.0f, h1 / 2.0f, 0.0f));

    m_texCoords.clear();
    m_texCoords.push_back(QVector2D(0.0, 0.0));
    m_texCoords.push_back(QVector2D(0.0, 1.0));
    m_texCoords.push_back(QVector2D(1.0, 0));
    m_texCoords.push_back(QVector2D(1.0, 1.0));
}

int  QuickRenderBgra::CreateProgram()
{
    const char* vsrc =
        "attribute vec4 vertex;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 texCoordOut;\n"
        "uniform mat4 matrix;\n"
        "void main() {\n"
        "   gl_Position = matrix * vertex;\n"
        "	texCoordOut = texCoord; \n"
        "}";

    const char* fsrc =
        "#ifdef GL_ES\n"
        "   precision lowp float;\n"
        "#endif\n"

        "uniform sampler2D textureRGB;\n"
        "varying vec2 texCoordOut;\n"
        "void main(void)\n"
        "{\n"
        "	gl_FragColor = texture2D(textureRGB, texCoordOut.st); \n"
        "}";

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    if (!m_program.link())
    {
        LOG() << m_program.log().toStdString();
        return CodeNo;
    }

    m_vertexAttr = m_program.attributeLocation("vertex");
    m_textAttr = m_program.attributeLocation("texCoord");
    m_matrixUniform = m_program.uniformLocation("matrix");

    return CodeOK;
}

void QuickRenderBgra::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, videoWidth, videoHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pData[0]);

    auto textureLocation = glGetUniformLocation(m_program.programId(), "textureRGB");
    glUniform1i(textureLocation, 0);
}

void QuickRenderBgra::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_BGRA, GL_UNSIGNED_BYTE, pData[0]);
}


int QuickRenderYUV420P::CreateProgram()
{
    const char* vsrc =
        "attribute vec4 vertex;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 texCoordOut;\n"
        "uniform mat4 matrix;\n"
        "void main() {\n"
        "   gl_Position = matrix * vertex;\n"
        "   texCoordOut = texCoord; \n"
        "}";

    const char* fsrc =
        "#ifdef GL_ES\n"
        "   precision lowp float;\n"
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

            "yuv.x = texture2D(textureY, texCoordOut).x - 0.0625;\n"
            "yuv.y = texture2D(textureU, texCoordOut).x - 0.5;\n"
            "yuv.z = texture2D(textureV, texCoordOut).x - 0.5;\n"

            "rgb.x = dot(yuv, yuv2r);\n"
            "rgb.y = dot(yuv, yuv2g);\n"
            "rgb.z = dot(yuv, yuv2b);\n"

            "gl_FragColor = vec4(rgb, 1.0);"
        "}";

    m_program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    m_program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    if (!m_program.link())
    {
        LOG() << m_program.log().toStdString();
        return CodeNo;
    }

    m_vertexAttr = m_program.attributeLocation("vertex");
    m_textAttr = m_program.attributeLocation("texCoord");
    m_matrixUniform = m_program.uniformLocation("matrix");

    return CodeOK;
}

void QuickRenderYUV420P::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
    glDeleteTextures(3, m_videoTextureID);
    glGenTextures(3, m_videoTextureID);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, pData[1]);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth / 2, videoHeight / 2, 0, GL_RED, GL_UNSIGNED_BYTE, pData[2]);

    auto location_textureY = glGetUniformLocation(m_program.programId(), "textureY");
    auto location_textureU = glGetUniformLocation(m_program.programId(), "textureU");
    auto location_textureV = glGetUniformLocation(m_program.programId(), "textureV");
    glUniform1i(location_textureY, 0);
    glUniform1i(location_textureU, 1);
    glUniform1i(location_textureV, 2);
}

void QuickRenderYUV420P::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
{
    bool bSetRowLength = false;

    // 如果数据的行宽，大于需要，通过设置GL_UNPACK_ROW_LENGTH参数，可以达到裁剪、抠图的目的
    if (pLineSize[0] > videoWidth || pLineSize[1] > videoWidth/2)
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

    if (pLineSize[1] != pLineSize[2])
    {
        LOG() << "fatal error: " << __FUNCTION__ << " U V lineSize not equal";
        return;
    }

    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[1]);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[2]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RED, GL_UNSIGNED_BYTE, pData[2]);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RED, GL_UNSIGNED_BYTE, pData[1]);

    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

    // 恢复为0
    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}


int QuickRenderNV12::CreateProgram()
{
    const char* vsrc =
        "attribute vec4 vertex;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 texCoordOut;\n"
        "uniform mat4 matrix;\n"
        "void main() {\n"
        "   gl_Position = matrix * vertex;\n"
        "	texCoordOut = texCoord; \n"
        "}";

    const char* fsrc =
        "#ifdef GL_ES\n"
        "   precision lowp float;\n"
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

    m_program.addCacheableShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    m_program.addCacheableShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    if (!m_program.link())
    {
        LOG() << m_program.log().toStdString();
        return CodeNo;
    }

    m_vertexAttr = m_program.attributeLocation("vertex");
    m_textAttr = m_program.attributeLocation("texCoord");
    m_matrixUniform = m_program.uniformLocation("matrix");
    return CodeOK;
}

void QuickRenderNV12::CreateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData)
{
    glDeleteTextures(2, m_videoTextureID);
    glGenTextures(2, m_videoTextureID);

    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoWidth, videoHeight, 0, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, videoWidth / 2, videoHeight / 2, 0, GL_RG, GL_UNSIGNED_BYTE, pData[1]);

    auto location_textureY = glGetUniformLocation(m_program.programId(), "textureY");
    auto location_textureUV = glGetUniformLocation(m_program.programId(), "textureUV");
    glUniform1i(location_textureY, 0);
    glUniform1i(location_textureUV, 1);
}

void QuickRenderNV12::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
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
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[1] / 2);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[1]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth / 2, m_iVideoHeight / 2, GL_RG, GL_UNSIGNED_BYTE, pData[1]);

    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pLineSize[0]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_RED, GL_UNSIGNED_BYTE, pData[0]);

    if (bSetRowLength)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}