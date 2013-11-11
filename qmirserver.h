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

#ifndef QMIRSERVER_H
#define QMIRSERVER_H

#include <QObject>
#include <QThread>

// Mir
#include <mir/display_server.h>

#include "mirserverconfiguration.h"

class MirServerConfiguration;

// Wrap mir::DisplayServer with QObject, so it can be controlled via QThread
class MirServerWorker : public QObject, public mir::DisplayServer
{
    Q_OBJECT
public:
    MirServerWorker(MirServerConfiguration &config)
        : mir::DisplayServer(config)
    {}

public Q_SLOTS:
    void run() { mir::DisplayServer::run(); }
    void stop() { mir::DisplayServer::stop(); }
};


class QMirServer: public QObject
{
    Q_OBJECT

public:
    QMirServer(MirServerConfiguration*, QObject* parent=0);
    ~QMirServer();

Q_SIGNALS:
    void run();
    void stop();

protected Q_SLOTS:
    void shutDown();

private:
    QThread m_mirThread;
    MirServerWorker *m_mirServer;
    Q_DISABLE_COPY(QMirServer)
};

#endif // QMIRSERVER_H
