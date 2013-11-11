#include "miropenglcontext.h"

#include "displaywindow.h"

// Qt supports one GL context per screen, but also shared contexts.
// The Mir "Display" generates a shared GL context for all DisplayBuffers
// (i.e. individual display output buffers) to use as a common base context.

MirOpenGLContext::MirOpenGLContext(mir::DefaultServerConfiguration *config, QSurfaceFormat format)
    : m_format(format)
    , m_mirConfig(config)
{
    // almost equivalent to Mir's Display for establishing the shared GL context,
    // and the DisplayBuffer for swapping each buffer.
    // So each QPlatformSurface is a DisplayBuffer. This then
}

MirOpenGLContext::~MirOpenGLContext()
{
}

QSurfaceFormat MirOpenGLContext::format() const
{
    // construct QSurfaceFormat from information obtained from Mir's Display
    return m_format;
}

void MirOpenGLContext::swapBuffers(QPlatformSurface *surface)
{
    // needs to call Mir's DisplayBuffer::post_update()
    DisplayWindow *displayBuffer = static_cast<DisplayWindow*>(surface);
    displayBuffer->swapBuffers(); //blocks for vsync
}

bool MirOpenGLContext::makeCurrent(QPlatformSurface *surface)
{
    // needs to call Mir's DisplayBuffer::make_current()
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
    //??
}
