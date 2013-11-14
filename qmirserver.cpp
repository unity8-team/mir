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

// Qt
#include <QObject>
#include <QCoreApplication>
#include <QDebug>

// local
#include "qmirserver.h"
#include "mirserverconfiguration.h"


QMirServer::QMirServer(MirServerConfiguration *config, QObject *parent)
    : QObject(parent)
    , m_mirServer(new MirServerWorker(*config))
{
    m_mirServer->moveToThread(&m_mirThread);

    connect(this, &QMirServer::run, m_mirServer, &MirServerWorker::run);
    connect(this, &QMirServer::stop, m_mirServer, &MirServerWorker::stop);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &QMirServer::shutDown);
    connect(m_mirServer, &MirServerWorker::stopped, [=]() { m_mirThread.quit(); }); // &m_mirThread, &QThread::quit); // why doens't this work??

    m_mirThread.start(QThread::TimeCriticalPriority);
    Q_EMIT run();
}

QMirServer::~QMirServer()
{
    shutDown();
}

void QMirServer::shutDown()
{
    if (m_mirThread.isRunning()) {
        m_mirServer->stop();
        //Q_EMIT stop(); // why doesn't this work??
        m_mirThread.wait();
    }

    if (m_mirServer) 
        delete m_mirServer; //->deleteLater(); // causes crash, but does clean up. Probably SGRender thread not finished.
}
