#ifndef MIRSERVERINTEGRATION_H
#define MIRSERVERINTEGRATION_H

#include <qpa/qplatformintegration.h>

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

    QPlatformFontDatabase *fontDatabase() const;
    QPlatformServices *services() const;

private:
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
};

#endif // MIRSERVERINTEGRATION_H
