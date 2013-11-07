/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qmirserverapplication.h"
#include "dbusscreen.h"

class QMirServerApplicationPrivate {
public:
    QMirServerApplicationPrivate(ShellServerConfiguration* serverConfiguration, QMirServerApplication *qq)
        : q_ptr(qq)
        , serverConfiguration(serverConfiguration)
        , dbusScreen(new DBusScreen(serverConfiguration))
    {}

    QMirServerApplication * const q_ptr;
    ShellServerConfiguration *serverConfiguration;
    QScopedPointer<DBusScreen> dbusScreen;
    Q_DECLARE_PUBLIC(QMirServerApplication)
};


QMirServerApplication::QMirServerApplication(int &argc, char **argv, ShellServerConfiguration* serverConfiguration)
    : QGuiApplication(argc, argv)
    , d_ptr(new QMirServerApplicationPrivate(serverConfiguration, this))
{
}

QMirServerApplication::~QMirServerApplication()
{
    delete d_ptr;
}

ShellServerConfiguration* QMirServerApplication::server() const
{
    Q_D(const QMirServerApplication);
    return d->serverConfiguration;
}

// class factory implemenation
extern "C" {
    QMirServerApplication *createQMirServerApplication(int &argc, char **argv, ShellServerConfiguration *serverConfiguration) {
        return new QMirServerApplication(argc, argv, serverConfiguration);
    }

    ShellServerConfiguration* server(QMirServerApplication *mirServer) {
        return mirServer->server();
    }

    void destroyQMirServerApplication(QMirServerApplication *mirServer) {
        delete mirServer;
    }
}
