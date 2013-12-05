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

#ifndef DBUSWINDOWSTACK_H
#define DBUSWINDOWSTACK_H

#include <QObject>
#include <QDBusArgument>

class ApplicationManager;

class AppIdDesktopFile
{
public:
    QString app_id;
    QString desktop_file;
};

QDBusArgument &operator<<(QDBusArgument &a, const AppIdDesktopFile &aidf);
const QDBusArgument &operator>>(const QDBusArgument &a, AppIdDesktopFile &aidf);

class WindowInfo
{
public:
    unsigned int window_id;
    QString app_id;
    bool focused;
    unsigned int stage;
};

QDBusArgument &operator<<(QDBusArgument &a, const WindowInfo &aidf);
const QDBusArgument &operator>>(const QDBusArgument &a, WindowInfo &aidf);

class DBusWindowStack : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.canonical.Unity.WindowStack")
public:
    explicit DBusWindowStack(ApplicationManager* parent);
    ~DBusWindowStack();

    Q_INVOKABLE Q_SCRIPTABLE AppIdDesktopFile GetAppIdFromPid(unsigned int pid);
    Q_INVOKABLE Q_SCRIPTABLE QList<WindowInfo> GetWindowStack();
    Q_INVOKABLE Q_SCRIPTABLE QStringList GetWindowProperties(unsigned int window_id, const QString &app_id, const QStringList &names);

signals:
    void FocusedWindowChanged(unsigned int window_id, const QString &app_id, unsigned int stage);
    void WindowCreated(unsigned int window_id, const QString &app_id);
    void WindowDestroyed(unsigned int window_id, const QString &app_id);
};

Q_DECLARE_METATYPE(AppIdDesktopFile);
Q_DECLARE_METATYPE(WindowInfo);

#endif // DBUSWINDOWSTACK_H
