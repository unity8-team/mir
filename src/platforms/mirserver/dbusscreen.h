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

#ifndef DBUSSCREEN_H
#define DBUSSCREEN_H

// qt
#include <QObject>

class MirServerConfiguration;

class DBusScreen : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.Screen")

public:
    explicit DBusScreen(MirServerConfiguration *config, QObject *parent = 0);

    Q_INVOKABLE Q_SCRIPTABLE bool setScreenPowerMode(const QString &mode);

private:
    MirServerConfiguration *m_serverConfig;
};

#endif // DBUSSCREEN_H
