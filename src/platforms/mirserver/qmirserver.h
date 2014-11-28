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

// Qt
#include <QObject>
#include <QThread>
#include <QSharedPointer>

// local
#include "mirserverconfiguration.h"

// Wrap mir::DisplayServer with QObject, so it can be controlled via QThread
class MirServerWorker : public QObject
{
    Q_OBJECT

public:
    MirServerWorker(const QSharedPointer<MirServerConfiguration> &config)
        : config(config)
    {}

Q_SIGNALS:
    void stopped();

public Q_SLOTS:
    void run() { config->run(); Q_EMIT stopped(); }
    void stop() { config->stop(); }

private:
    const QSharedPointer<MirServerConfiguration> config;
};


class QMirServer: public QObject
{
    Q_OBJECT

public:
    QMirServer(const QSharedPointer<MirServerConfiguration> &config, QObject* parent=0);
    ~QMirServer();

Q_SIGNALS:
    void run();
    void stop();

protected Q_SLOTS:
    void shutDownMirServer();
    void shutDownQApplication();

private:
    QThread m_mirThread;
    MirServerWorker *m_mirServer;
    Q_DISABLE_COPY(QMirServer)
};

#endif // QMIRSERVER_H
