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

#ifndef QMIRSERVERAPPLICATION_H
#define QMIRSERVERAPPLICATION_H

#include <QGuiApplication>

class ShellServerConfiguration;
class QMirServerApplicationPrivate;

class QMirServerApplication : public QGuiApplication
{
    Q_OBJECT

public:
    explicit QMirServerApplication(int &argc, char **argv, ShellServerConfiguration* serverConfiguration);
    ~QMirServerApplication();

    ShellServerConfiguration* server() const;

protected:
    QMirServerApplicationPrivate * const d_ptr;

private:
    Q_DECLARE_PRIVATE(QMirServerApplication)
};


// class factory
extern "C" {
    QMirServerApplication *createQMirServerApplication(int &argc, char **argv,
                                                       ShellServerConfiguration *serverConfiguration);

    ShellServerConfiguration* server(QMirServerApplication *mirServer);

    void destroyQMirServerApplication(QMirServerApplication *mirServer);
}

#endif // QMIRSERVERAPPLICATION_H
