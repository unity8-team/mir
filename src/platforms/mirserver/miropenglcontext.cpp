#include "miropenglcontext.h"

#include "displaywindow.h"
#include "mirserverconfiguration.h"

#include <QDebug>

#include <QSurfaceFormat>
#include <QtPlatformSupport/private/qeglconvenience_p.h>

// Mir
#include <mir/graphics/display.h>

// Qt supports one GL context per screen, but also shared contexts.
// The Mir "Display" generates a shared GL context for all DisplayBuffers
// (i.e. individual display output buffers) to use as a common base context.

MirOpenGLContext::MirOpenGLContext(mir::DefaultServerConfiguration *config, QSurfaceFormat format)
    : m_mirConfig(config)
#if GL_DEBUG
    , m_logger(new QOpenGLDebugLogger)
#endif
{
    std::shared_ptr<mir::graphics::Display> display = m_mirConfig->the_display();

    m_format = q_glFormatFromConfig(display->the_gl_display(), display->the_gl_config(), format);

#ifndef QT_NO_DEBUG
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
    q_printEglConfig(display->the_gl_display(), display->the_gl_config());

#if GL_DEBUG
    QObject::connect(m_logger, &QOpenGLDebugLogger::messageLogged,
                     this, &MirOpenGLContext::onGlDebugMessageLogged, Qt::DirectConnection);
#endif // Qt>=5.2
#endif // debug
}

MirOpenGLContext::~MirOpenGLContext()
{
}

QSurfaceFormat MirOpenGLContext::format() const
{
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

#if GL_DEBUG
        if (!m_logger->isLogging() && m_logger->initialize()) {
            m_logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
            m_logger->enableMessages();
        }
#endif

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
    return eglGetProcAddress(procName.constData()); // Mir might want to wrap this?
}
