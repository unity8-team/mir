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

#include <functional>

#include <QObject>

#include "shellserverconfiguration.h"

class QMirServer: public QObject
{
    Q_OBJECT

public:
    QMirServer(int argc, const char *argv[], QObject* parent=0);
    ~QMirServer();

    int runWithClient(std::function<int(int, const char**, ShellServerConfiguration*)>);

private:
    int m_argc;
    const char** m_argv;
    Q_DISABLE_COPY(QMirServer)
};


// class factory
extern "C" {
    QMirServer *createQMirServer(int argc, const char **argv);

    int runQMirServerWithClient(QMirServer *mirServer, std::function<int(int, const char**, ShellServerConfiguration*)> client);

    void destroyQMirServer(QMirServer *mirServer);
}

#endif // QMIRSERVER_H
