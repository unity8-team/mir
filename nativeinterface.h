#include <qpa/qplatformnativeinterface.h>

#include "mirserverconfiguration.h"

class NativeInterface : public QPlatformNativeInterface
{
public:
    NativeInterface(MirServerConfiguration*);

    virtual void *nativeResourceForIntegration(const QByteArray &resource);

private:
    MirServerConfiguration *m_mirConfig;
};