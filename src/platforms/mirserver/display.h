#ifndef DISPLAY_H
#define DISPLAY_H

#include <QObject>
#include <qpa/qplatformscreen.h>

#include "mir/graphics/display.h"
#include "mir/default_server_configuration.h"

class Display : public QObject
{
    Q_OBJECT
public:
    Display(mir::DefaultServerConfiguration*, QObject *parent = 0);
    ~Display();

    QList<QPlatformScreen *> screens() const { return m_screens; }

private:
    QList<QPlatformScreen *> m_screens;
    mir::DefaultServerConfiguration* m_mirConfig;
};

#endif // DISPLAY_H
