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

// local
#include "dbuswindowstack.h"
#include "application_manager.h"

// Qt
#include <QDBusConnection>
#include <QDBusMetaType>

DBusWindowStack::DBusWindowStack(ApplicationManager *parent) : QObject(parent)
{
    qRegisterMetaType<AppIdDesktopFile>();
    qDBusRegisterMetaType<AppIdDesktopFile>();
    qRegisterMetaType<WindowInfo>();
    qRegisterMetaType< QList<WindowInfo> >();
    qDBusRegisterMetaType<WindowInfo>();
    qDBusRegisterMetaType< QList<WindowInfo> >();

    QDBusConnection::sessionBus().registerService("com.canonical.Unity.WindowStack");
    // TODO ExportScriptableSlots shouldn't be needed but without it i don't get the methods :-/
    QDBusConnection::sessionBus().registerObject("/com/canonical/Unity/WindowStack", this, QDBusConnection::ExportAllSignals | QDBusConnection::ExportScriptableSlots |QDBusConnection::ExportScriptableInvokables );
}

DBusWindowStack::~DBusWindowStack()
{
}

AppIdDesktopFile DBusWindowStack::GetAppIdFromPid(unsigned int pid)
{
    AppIdDesktopFile res;
    ApplicationManager *appMgr = static_cast<ApplicationManager*>(parent());
    const Application* app = static_cast<Application*>(appMgr->findApplicationWithPid(pid));
    if (app) {
        res.app_id = app->appId();
        res.desktop_file = app->desktopFile();
    }
    return res;
}

QList<WindowInfo> DBusWindowStack::GetWindowStack()
{
    QList<WindowInfo> res;
    ApplicationManager *appMgr = static_cast<ApplicationManager*>(parent());
    const QList<Application*> &applications = appMgr->list();
    foreach(Application* app, applications) {
        WindowInfo wi;
        wi.window_id = 0;
        wi.app_id = app->appId();
        wi.focused = app->focused();
        wi.stage = 0;
        res << wi;
    }
    return res;
}

QStringList DBusWindowStack::GetWindowProperties(unsigned int window_id, const QString &app_id,
            const QStringList &names)
{
    Q_UNUSED(window_id);
    Q_UNUSED(app_id);
    Q_UNUSED(names);
    return QStringList();
}

QDBusArgument &operator<<(QDBusArgument &a, const AppIdDesktopFile &aidf)
{
    a.beginStructure();
    a << aidf.app_id << aidf.desktop_file;
    a.endStructure();
    return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a, AppIdDesktopFile &aidf)
{
    a.beginStructure();
    a >> aidf.app_id >> aidf.desktop_file;
    a.endStructure();

    return a;
}

QDBusArgument &operator<<(QDBusArgument &a, const WindowInfo &wi)
{
    a.beginStructure();
    a << wi.window_id << wi.app_id << wi.focused << wi.stage;
    a.endStructure();
    return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a, WindowInfo &wi)
{
    a.beginStructure();
    a >> wi.window_id >> wi.app_id >> wi.focused >> wi.stage;
    a.endStructure();

    return a;
}
