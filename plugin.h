#ifndef PLUGIN_H
#define PLUGIN_H

#include <qpa/qplatformintegrationplugin.h>

class MirServerIntegrationPlugin : public QPlatformIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPA.QPlatformIntegrationFactoryInterface.5.2"
                      FILE "mirserver.json")

public:
    QStringList keys() const;
    QPlatformIntegration* create(const QString&, const QStringList&);
};

#endif // PLUGIN_H
