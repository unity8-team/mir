#ifndef NATIVEINTEGRATION_H
#define NATIVEINTEGRATION_H

// qt
#include <qpa/qplatformnativeinterface.h>

// local
#include "mirserverconfiguration.h"

class NativeInterface : public QPlatformNativeInterface
{
public:
    NativeInterface(MirServerConfiguration*);

    virtual void *nativeResourceForIntegration(const QByteArray &resource);

private:
    MirServerConfiguration *m_mirConfig;
};

#endif // NATIVEINTEGRATION_H
