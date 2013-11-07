#include "mirserverintegration.h"

#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixservices_p.h>

#include <qpa/qplatformwindow.h>

MirServerIntegration::MirServerIntegration()
{
    // Create instance of and start the Mir server
}

MirServerIntegration::~MirServerIntegration()
{
    // Stop the mir server
}

bool MirServerIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL: return true;
    case ThreadedOpenGL: return true;
    case WindowManagement: return true;
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *MirServerIntegration::createPlatformWindow(QWindow *window) const
{
    // One window per display buffer only.
    QPlatformWindow* platformWindow;
    platformWindow->requestActivateWindow();
    return platformWindow;
}

QPlatformBackingStore *MirServerIntegration::createPlatformBackingStore(QWindow *window) const
{

}

QPlatformOpenGLContext *MirServerIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{

}

QAbstractEventDispatcher *MirServerIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QPlatformFontDatabase *MirServerIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *MirServerIntegration::services() const
{
    return m_services.data();
}
