#include "plugin.h"
#include "mirserverintegration.h"

QStringList MirServerIntegrationPlugin::keys() const {
    QStringList list;
    list << "mirserver";
    return list;
}

QPlatformIntegration* MirServerIntegrationPlugin::create(const QString &system, const QStringList &) {
    if (system.toLower() == "mirserver")
        return new MirServerIntegration;
    return 0;
}

