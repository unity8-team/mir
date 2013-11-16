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
#include <QtQml/QtQml>

// local
#include "application.h"
#include "application_manager.h"
#include "applicationscreenshotprovider.h"
#include "mirsurfacemanager.h"
#include "mirsurface.h"
#include "inputarea.h"
#include "inputfilterarea.h"
#include "shellinputarea.h"
#include "ubuntukeyboardinfo.h"

// unity-mir
#include "logging.h"

static QObject* applicationManagerSingleton(QQmlEngine* engine, QJSEngine* scriptEngine) {
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    DLOG("applicationManagerSingleton (engine=%p, scriptEngine=%p)", engine, scriptEngine);

    return ApplicationManager::singleton();
}

static QObject* surfaceManagerSingleton(QQmlEngine* engine, QJSEngine* scriptEngine) {
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    DLOG("surfaceManagerSingleton (engine=%p, scriptEngine=%p)", engine, scriptEngine);
    return MirSurfaceManager::singleton();
}

class UnityApplicationPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

    virtual void registerTypes(const char* uri)
    {
        DLOG("UnityApplicationPlugin::registerTypes (this=%p, uri='%s')", this, uri);
        ASSERT(QLatin1String(uri) == QLatin1String("Unity.Application"));

        qRegisterMetaType<ApplicationManager*>("ApplicationManager*"); //need for queueing signals

        qmlRegisterUncreatableType<unity::shell::application::ApplicationManagerInterface>(
                    uri, 0, 1, "ApplicationManagerInterface", "Abstract interface. Cannot be created in QML");
        qmlRegisterSingletonType<ApplicationManager>(
                    uri, 0, 1, "ApplicationManager", applicationManagerSingleton);
        qmlRegisterUncreatableType<unity::shell::application::ApplicationInfoInterface>(
                    uri, 0, 1, "ApplicationInfoInterface", "Abstract interface. Cannot be created in QML");
        qmlRegisterUncreatableType<Application>(
                    uri, 0, 1, "ApplicationInfo", "ApplicationInfo can't be instantiated");
        qmlRegisterSingletonType<MirSurfaceManager>(
                    uri, 0, 1, "SurfaceManager", surfaceManagerSingleton);
        qmlRegisterUncreatableType<MirSurface>(
                    uri, 0, 1, "MirSurface", "MirSurface can't be instantiated");
        qmlRegisterType<InputArea>(uri, 0, 1, "InputArea");
        qmlRegisterType<ShellInputArea>(uri, 0, 1, "ShellInputArea");
        qmlRegisterType<InputFilterArea>(uri, 0, 1, "InputFilterArea");
        qmlRegisterType<UbuntuKeyboardInfo>(uri, 0, 1, "UbuntuKeyboardInfo");
    }

    virtual void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        QQmlExtensionPlugin::initializeEngine(engine, uri);

        ApplicationManager* appManager = static_cast<ApplicationManager*>(applicationManagerSingleton(engine, NULL));
        engine->addImageProvider(QLatin1String("screenshot"), new ApplicationScreenshotProvider(appManager));
    }
};

#include "plugin.moc"
