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
#include <QCoreApplication>
#include <QGuiApplication>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <qpa/qplatforminputcontext.h>
#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QOpenGLContext>

// Local
#include "ubuntubackingstore.h"
#include "ubuntuclientintegration.h"
#include "ubuntuclipboard.h"
#include "ubuntuinput.h"
#include "ubuntunativeinterface.h"
#include "ubuntuopenglcontext.h"
#include "ubuntuwindow.h"
#include "ubuntuscreen.h"
#include "../common/ubuntutheme.h"
#include <logging.h>

// platform-api
#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/id.h>
#include <ubuntu/application/options.h>

static void resumedCallback(const UApplicationOptions *options, void* context)
{
    Q_UNUSED(options)
    DASSERT(context != NULL);
    UbuntuClientIntegration* integration = static_cast<UbuntuClientIntegration*>(context);
    integration->screen()->toggleSensors(true);
    QCoreApplication::postEvent(QCoreApplication::instance(),
                                new QEvent(QEvent::ApplicationActivate));
}

static void aboutToStopCallback(UApplicationArchive *archive, void* context)
{
    Q_UNUSED(archive)
    DASSERT(context != NULL);
    UbuntuClientIntegration* integration = static_cast<UbuntuClientIntegration*>(context);
    integration->screen()->toggleSensors(false);
    integration->inputContext()->hideInputPanel();
    QCoreApplication::postEvent(QCoreApplication::instance(),
                                new QEvent(QEvent::ApplicationDeactivate));
}

UbuntuClientIntegration::UbuntuClientIntegration()
    : QPlatformIntegration()
    , mNativeInterface(new UbuntuNativeInterface)
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    , mEventDispatcher(createUnixEventDispatcher())
#endif
    , mFontDb(new QGenericUnixFontDatabase)
    , mServices(new UbuntuPlatformServices)
    , mClipboard(new UbuntuClipboard)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QGuiApplicationPrivate::instance()->setEventDispatcher(mEventDispatcher);
#endif

    setupOptions();
    setupDescription();

    // Create new application instance
    mInstance = u_application_instance_new_from_description_with_options(mDesc, mOptions);

    if (mInstance == nullptr)
        qFatal("QUbuntu: Could not create application instance");

    // Create default screen.
    mScreen = new UbuntuScreen;
    screenAdded(mScreen);

    mScreen->toggleSensors(false);

    // Initialize input.
    if (qEnvironmentVariableIsEmpty("QTUBUNTU_NO_INPUT")) {
        mInput = new UbuntuInput(this);
        mInputContext = QPlatformInputContextFactory::create();
    } else {
        mInput = nullptr;
        mInputContext = nullptr;
    }
}

UbuntuClientIntegration::~UbuntuClientIntegration()
{
    delete mClipboard;
    delete mInput;
    delete mInputContext;
    delete mScreen;
    delete mServices;
}

QPlatformServices *UbuntuClientIntegration::services() const
{
    return mServices;
}

void UbuntuClientIntegration::setupOptions()
{
    QStringList args = QCoreApplication::arguments();
    int argc = args.size() + 1;
    char **argv = new char*[argc];
    for (int i = 0; i < argc - 1; i++)
        argv[i] = qstrdup(args.at(i).toLocal8Bit());
    argv[argc - 1] = nullptr;

    mOptions = u_application_options_new_from_cmd_line(argc - 1, argv);

    for (int i = 0; i < argc; i++)
        delete [] argv[i];
    delete [] argv;
}

void UbuntuClientIntegration::setupDescription()
{
    mDesc = u_application_description_new();
    UApplicationId* id = u_application_id_new_from_stringn("QtUbuntu", 8);
    u_application_description_set_application_id(mDesc, id);

    UApplicationLifecycleDelegate* delegate = u_application_lifecycle_delegate_new();
    u_application_lifecycle_delegate_set_application_resumed_cb(delegate, &resumedCallback);
    u_application_lifecycle_delegate_set_application_about_to_stop_cb(delegate, &aboutToStopCallback);
    u_application_lifecycle_delegate_set_context(delegate, this);
    u_application_description_set_application_lifecycle_delegate(mDesc, delegate);
}

QPlatformWindow* UbuntuClientIntegration::createPlatformWindow(QWindow* window) const
{
    return const_cast<UbuntuClientIntegration*>(this)->createPlatformWindow(window);
}

QPlatformWindow* UbuntuClientIntegration::createPlatformWindow(QWindow* window)
{
    QPlatformWindow* platformWindow = new UbuntuWindow(
            window, static_cast<UbuntuScreen*>(mScreen), mInput, mInstance);
    platformWindow->requestActivateWindow();
    return platformWindow;
}

bool UbuntuClientIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps:
        return true;
        break;

    case OpenGL:
        return true;
        break;

    case ThreadedOpenGL:
        if (qEnvironmentVariableIsEmpty("QTUBUNTU_NO_THREADED_OPENGL")) {
            return true;
        } else {
            DLOG("disabled threaded OpenGL");
            return false;
        }
        break;

    default:
        return QPlatformIntegration::hasCapability(cap);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
QAbstractEventDispatcher *UbuntuClientIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}
#endif

QPlatformBackingStore* UbuntuClientIntegration::createPlatformBackingStore(QWindow* window) const
{
    return new UbuntuBackingStore(window);
}

QPlatformOpenGLContext* UbuntuClientIntegration::createPlatformOpenGLContext(
        QOpenGLContext* context) const
{
    return const_cast<UbuntuClientIntegration*>(this)->createPlatformOpenGLContext(context);
}

QPlatformOpenGLContext* UbuntuClientIntegration::createPlatformOpenGLContext(
        QOpenGLContext* context)
{
    return new UbuntuOpenGLContext(static_cast<UbuntuScreen*>(context->screen()->handle()));
}

QStringList UbuntuClientIntegration::themeNames() const
{
    return QStringList(UbuntuTheme::name);
}

QPlatformTheme* UbuntuClientIntegration::createPlatformTheme(const QString& name) const
{
    Q_UNUSED(name);
    return new UbuntuTheme;
}
