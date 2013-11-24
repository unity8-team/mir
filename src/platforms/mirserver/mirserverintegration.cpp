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
 *
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#include "mirserverintegration.h"

#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtPlatformSupport/private/qgenericunixservices_p.h>

#include <qpa/qplatformwindow.h>
#include <qpa/qplatformaccessibility.h>
#include <qpa/qwindowsysteminterface.h>

#include <QCoreApplication>
#include <QStringList>
#include <QOpenGLContext>

#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
#include <private/qguiapplication_p.h>
#endif

#include <QDebug>

// Mir
#include <mir/graphics/display.h>
#include <mir/graphics/display_buffer.h>
#include <mir/main_loop.h>

// std
#include <csignal>

// local
#include "displaywindow.h"
#include "qmirserver.h"
#include "mirserverconfiguration.h"
#include "miropenglcontext.h"

namespace mg = mir::graphics;

MirServerIntegration::MirServerIntegration()
    : m_accessibility(new QPlatformAccessibility())
    , m_fontDb(new QGenericUnixFontDatabase())
    , m_services(new QPlatformServices())
    , m_mirServer(nullptr)
    , m_display(nullptr)
    , m_nativeInterface(nullptr)
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    , eventDispatcher_(createUnixEventDispatcher())
#endif
{
    // Start Mir server only once Qt has initialized its event dispatcher, see initialize()

    QStringList args = QCoreApplication::arguments();
    // convert arguments back into argc-argv form that Mir wants
    char **argv;
    argv = new char*[args.size() + 1];
    for (int i = 0; i < args.size(); i++) {
        argv[i] = new char[strlen(args.at(i).toStdString().c_str())+1];
        memcpy(argv[i], args.at(i).toStdString().c_str(), strlen(args.at(i).toStdString().c_str())+1);
    }
    argv[args.size()] = ((char)NULL);

    m_mirConfig = new MirServerConfiguration(args.length(), const_cast<const char**>(argv));

#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
    QGuiApplicationPrivate::instance()->setEventDispatcher(eventDispatcher_);
    initialize();
#endif
}

MirServerIntegration::~MirServerIntegration()
{
    delete m_mirConfig;
}

bool MirServerIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL: return true;
    case ThreadedOpenGL: return true;
    case SharedGraphicsCache: return true;
    case BufferQueueingOpenGL: return false; // causes slowdown on Nexus4
    case MultipleWindows: return false; // multi-monitor support
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
    case WindowManagement: return false; // platform has no WM, as this implements the WM!
    case NonFullScreenWindows: return false;
#endif
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *MirServerIntegration::createPlatformWindow(QWindow *window) const
{
    QWindowSystemInterface::flushWindowSystemEvents();

    DisplayWindow* displayWindow = nullptr;

    m_mirConfig->the_display()->for_each_display_buffer(
                [&](mg::DisplayBuffer& buffer) {
        // FIXME(gerry) this will go very bad for >1 display buffer
        displayWindow = new DisplayWindow(window, &buffer);
    });

    if (!displayWindow)
        return nullptr;

    //displayWindow->requestActivateWindow();
    return displayWindow;
}

QPlatformBackingStore *MirServerIntegration::createPlatformBackingStore(QWindow *window) const
{
    qDebug() << "createPlatformBackingStore" << window;
    return nullptr;
}

QPlatformOpenGLContext *MirServerIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    qDebug() << "createPlatformOpenGLContext" << context;
    return new MirOpenGLContext(m_mirConfig, context->format());
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
QAbstractEventDispatcher *MirServerIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}
#endif

void MirServerIntegration::initialize()
{
    // Creates instance of and start the Mir server in a separate thread
    m_mirServer = new QMirServer(m_mirConfig);

    m_display = new Display(m_mirConfig);
    m_nativeInterface = new NativeInterface(m_mirConfig);

    for (QPlatformScreen *screen : m_display->screens())
        screenAdded(screen);

    // install signal handler into the Mir event loop
    auto mainLoop = m_mirConfig->the_main_loop();

    mainLoop->register_signal_handler(
    {SIGINT, SIGTERM},
    [&](int)
    {
        qDebug() << "Signal caught, stopping Mir server..";
        QCoreApplication::quit();
    });
}

QPlatformAccessibility *MirServerIntegration::accessibility() const
{
    return m_accessibility.data();
}

QPlatformFontDatabase *MirServerIntegration::fontDatabase() const
{
    return m_fontDb.data();
}

QPlatformServices *MirServerIntegration::services() const
{
    return m_services.data();
}

QPlatformNativeInterface *MirServerIntegration::nativeInterface() const
{
    return m_nativeInterface;
}
