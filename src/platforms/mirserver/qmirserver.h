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

#include <condition_variable>
#include <mutex>

// Wrap mir::Server with QObject, so it can be controlled via QThread
class MirServerWorker : public QObject
{
    Q_OBJECT

public:
    MirServerWorker(const QSharedPointer<MirServerConfiguration> &config)
        : config(config)
    {}

    bool wait_for_mir_startup();

Q_SIGNALS:
    void stopped();

public Q_SLOTS:
    void run();
    void stop() { config->stop(); }

private:
    std::mutex mutex;
    std::condition_variable started_cv;
    bool mir_running{false};

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
