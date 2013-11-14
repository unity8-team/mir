#include "nativeinterface.h"

NativeInterface::NativeInterface(MirServerConfiguration *config)
    : m_mirConfig(config)
{
}

void *NativeInterface::nativeResourceForIntegration(const QByteArray &resource)
{
    void *result = nullptr;

    if (resource == "SessionAuthorizer")
        result = m_mirConfig->sessionAuthorizer();
    else if (resource == "SessionConfigurator")
        result = m_mirConfig->surfaceConfigurator();
    else if (resource == "SessionListener")
        result = m_mirConfig->sessionListener();

    return result;
}