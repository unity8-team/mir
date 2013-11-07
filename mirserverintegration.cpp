#include "mirserverintegration.h"

#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixservices_p.h>

#include <qpa/qplatformwindow.h>

// local
#include "mirserver/qmirserver.h"

MirServerIntegration::MirServerIntegration()
{
    // Start Mir server only once Qt has initialized its event dispatcher, see initialize()
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

void MirServerIntegration::initialize()
{
    // Create instance of and start the Mir server in a separate thread
    m_mirServer = new QMirServer();
}

QPlatformFontDatabase *MirServerIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *MirServerIntegration::services() const
{
    return m_services.data();
}
