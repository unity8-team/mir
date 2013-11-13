#include "miropenglcontext.h"

#include "displaywindow.h"

#include <QSurfaceFormat>
#include <QDebug>

// Qt supports one GL context per screen, but also shared contexts.
// The Mir "Display" generates a shared GL context for all DisplayBuffers
// (i.e. individual display output buffers) to use as a common base context.

MirOpenGLContext::MirOpenGLContext(mir::DefaultServerConfiguration *config, QSurfaceFormat format)
    : m_format(format)
    , m_mirConfig(config)
{
    // Customize QSurfaceFormat from information obtained from Mir's Display
    m_format.setRenderableType(QSurfaceFormat::OpenGLES);
//    m_format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    m_format.setAlphaBufferSize(8);
    m_format.setBlueBufferSize(8);
    m_format.setGreenBufferSize(8);
    m_format.setRedBufferSize(8);
    m_format.setDepthBufferSize(8);
    m_format.setSamples(2);

    const char* string = (const char*) glGetString(GL_VENDOR);
    qDebug() << "OpenGL ES vendor: " << qPrintable(string);
    string = (const char*) glGetString(GL_RENDERER);
    qDebug() << "OpenGL ES renderer"  << qPrintable(string);
    string = (const char*) glGetString(GL_VERSION);
    qDebug() << "OpenGL ES version" << qPrintable(string);
    string = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
    qDebug() << "OpenGL ES Shading Language version:" << qPrintable(string);
    string = (const char*) glGetString(GL_EXTENSIONS);
    qDebug() << "OpenGL ES extensions:" << qPrintable(string);
}

MirOpenGLContext::~MirOpenGLContext()
{
}

QSurfaceFormat MirOpenGLContext::format() const
{
    qDebug() << "MirOpenGLContext::format()";
    qDebug() << m_format;
    return m_format;
}

void MirOpenGLContext::swapBuffers(QPlatformSurface *surface)
{
    // ultimately calls Mir's DisplayBuffer::post_update()
    DisplayWindow *displayBuffer = static_cast<DisplayWindow*>(surface);
    displayBuffer->swapBuffers(); //blocks for vsync
}

bool MirOpenGLContext::makeCurrent(QPlatformSurface *surface)
{
    // ultimately calls Mir's DisplayBuffer::make_current()
    DisplayWindow *displayBuffer = static_cast<DisplayWindow*>(surface);
    if (displayBuffer) {
        displayBuffer->makeCurrent();
        return true;
    }
    return false;
}

void MirOpenGLContext::doneCurrent()
{
    // could call Mir's DisplayBuffer::release_current(), but for what DisplayBuffer?
}

QFunctionPointer MirOpenGLContext::getProcAddress(const QByteArray &procName)
{
    qDebug() << "MirOpenGLContext::getProcAddress" << procName;
    //??
}
