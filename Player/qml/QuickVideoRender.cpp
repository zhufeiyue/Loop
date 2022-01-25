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
    auto pRender = new QuickRenderBgra(this);
    pRender->CreateProgram();
    return pRender;
}


QuickRenderBgra::QuickRenderBgra(const QuickVideoRenderObject* pRenderObject):
    m_pRenderObject(pRenderObject)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	initializeOpenGLFunctions();
}

QuickRenderBgra::~QuickRenderBgra()
{
}

void QuickRenderBgra::render()
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

    m_program.bind();

    UpdateVideoTexture(m_iVideoWidth, m_iVideoHeight, m_renderData.pData, m_renderData.pLineSize);

    int width = framebufferObject()->width();
    int height = framebufferObject()->height();

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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    m_program.disableAttributeArray(m_vertexAttr);
    m_program.disableAttributeArray(m_textAttr);
    m_program.release();
}

void QuickRenderBgra::synchronize(QQuickFramebufferObject*)
{
    //LOG() << __FUNCTION__;
}

QOpenGLFramebufferObject* QuickRenderBgra::createFramebufferObject(const QSize& size)
{
    LOG() << __FUNCTION__ << ' ' << QThread::currentThreadId();

	QOpenGLFramebufferObjectFormat format;
	//format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	//format.setSamples(4);

	return new QOpenGLFramebufferObject(size, format);
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

    auto textureLocation = glGetUniformLocation(m_program.programId(), "textureRGB");
    glUniform1i(textureLocation, 0);
}

void QuickRenderBgra::UpdateVideoTexture(int videoWidth, int videoHeight, const uint8_t* const* pData, const int* pLineSize)
{
    glBindTexture(GL_TEXTURE_2D, m_videoTextureID[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iVideoWidth, m_iVideoHeight, GL_BGRA, GL_UNSIGNED_BYTE, pData[0]);
}