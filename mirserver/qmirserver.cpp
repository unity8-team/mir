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

// Mir support
#include <mir/display_server.h>

// Qt
#include <QCoreApplication>
#include <QStringList>

// local
#include "qmirserver.h"
#include "shellserverconfiguration.h"

// Wrap mir::DisplayServer with QObject, so it can be controlled via QThread
class MirServerWorker : public QObject, mir::DisplayServer {
    Q_OBJECT

    MirServerWorker(ServerConfiguration& config)
        : mir::DisplayServer(config)
    {}

public Q_SLOTS:
    void run() { mir::DisplayServer::run(); }
    void stop() { mir::DisplayServer::stop(); }
};


QMirServer::QMirServer(QObject *parent)
    : QObject(parent)
{
    QStringList args = QCoreApplication::arguments();
    // convert arguments back into argc-argv form that Mir wants
    char **argv;
    argv = new char*[args.size() + 1];
    for (int i = 0; i < args.size(); i++) {
        argv[i] = new char[strlen(args.at(i).toStdString().c_str())+1];
        memcpy(argv[i], args.at(i).toStdString().c_str(), strlen(args.at(i).toStdString().c_str())+1);
    }
    argv[args.size()] = ((char)NULL);

    auto config = new ShellServerConfiguration(args.length(), argv);
    m_mirServer = new MirServerWorker(config);
    m_mirServer->moveToThread(m_mirThread);

    connect(m_mirThread, &QThread::finished, m_mirServer, &QObject::deleteLater);
}

QMirServer::~QMirServer()
{
    m_mirThread.quit();
    m_mirThread.wait();
    deleteLater(m_mirServer);
}
