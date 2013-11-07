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
#include <mir/run_mir.h>
#include <mir/abnormal_exit.h>
#include <mir/default_server_configuration.h>

// Platform API
#include <application/ubuntu_application_api_mirserver_priv.h>

// Qt
#include <QCoreApplication>
#include <QDebug>
#include <QThread>

// Std
#include <thread>

// local
#include "qmirserver.h"

QMirServer::QMirServer(int argc, const char* argv[], QObject *parent)
    : QObject(parent)
    , m_argc(argc)
    , m_argv(argv)
{
}

QMirServer::~QMirServer()
{
}

int QMirServer::runWithClient(std::function<int(int, const char**, ShellServerConfiguration*)> client)
{
    // need local copies as lambda function will not capture class members
    int argc = m_argc;
    auto argv = m_argv;
    auto config = new ShellServerConfiguration(m_argc, m_argv);
    std::thread *t;

    mir::run_mir(*config, [config, &client, &argc, &argv, &t](mir::DisplayServer&) {
        ua_ui_mirserver_init(*config);

        try {
            t = new std::thread(client, argc, argv, config);
        } catch (...) {
            qDebug() << "Exception caught, quitting";
        }
    });
    if (QCoreApplication::instance()) {
        bool aboutToQuitSignaled = false;
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [&]() {
                aboutToQuitSignaled = true;
            }
        );
        QCoreApplication::quit();
        while (!aboutToQuitSignaled) {
            usleep(100000);
            QCoreApplication::quit();
        }
    }
    t->join();
    ua_ui_mirserver_finish();
    delete config;
    return 0;
}

// class factory implementation
extern "C" {
    QMirServer *createQMirServer(int argc, const char **argv) {
        return new QMirServer(argc, argv);
    }

    int runQMirServerWithClient(QMirServer *mirServer, std::function<int(int, const char**, ShellServerConfiguration*)> client) {
        return mirServer->runWithClient(client);
    }

    void destroyQMirServer(QMirServer *mirServer) {
        delete mirServer;
    }
}
