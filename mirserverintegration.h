#ifndef MIRSERVERINTEGRATION_H
#define MIRSERVERINTEGRATION_H

// qt
#include <qpa/qplatformintegration.h>

// local
#include "qmirserver.h"
#include "mirserverconfiguration.h"
#include "display.h"
#include "nativeinterface.h"

class MirServerIntegration : public QPlatformIntegration
{
public:
    MirServerIntegration();
    ~MirServerIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;

    QAbstractEventDispatcher *createEventDispatcher() const override;

    void initialize() override;

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;

    QPlatformAccessibility *accessibility() const override;

    QPlatformNativeInterface *nativeInterface() const override;

private:
    QScopedPointer<QPlatformAccessibility> m_accessibility;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
    QMirServer *m_mirServer;
    Display *m_display;
    MirServerConfiguration *m_mirConfig;
    NativeInterface *m_nativeInterface;
};

#endif // MIRSERVERINTEGRATION_H
