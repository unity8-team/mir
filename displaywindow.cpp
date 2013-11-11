#include "displaywindow.h"

#include "mir/geometry/size.h"

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformscreen.h>

static WId newWId()
{
    static WId id = 0;

    if (id == std::numeric_limits<WId>::max())
        qWarning("MirServer QPA: Out of window IDs");

    return ++id;
}

DisplayWindow::DisplayWindow(QWindow *window, mir::graphics::DisplayBuffer *displayBuffer)
    : QPlatformWindow(window)
    , m_winId(newWId())
    , m_displayBuffer(displayBuffer)
{
    qWarning("Window %p: %p 0x%x\n", this, window, uint(m_winId));

    QRect screenGeometry(screen()->availableGeometry());
    if (window->geometry() != screenGeometry) {
        QWindowSystemInterface::handleGeometryChange(window, screenGeometry);
    }
    window->setSurfaceType(QSurface::OpenGLSurface);
}

void DisplayWindow::setGeometry(const QRect &)
{
    // We only support full-screen windows
    QRect rect(screen()->availableGeometry());
    QWindowSystemInterface::handleGeometryChange(window(), rect);

    QPlatformWindow::setGeometry(rect);
}

void DisplayWindow::swapBuffers()
{
    m_displayBuffer->post_update();
}

void DisplayWindow::makeCurrent()
{
    m_displayBuffer->make_current();
}

void DisplayWindow::doneCurrent()
{
    m_displayBuffer->release_current();
}

