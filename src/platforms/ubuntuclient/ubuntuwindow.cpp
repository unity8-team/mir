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

// Local
#include "ubuntuinput.h"
#include "ubuntuwindow.h"
#include "ubuntuscreen.h"
#include <logging.h>

// Qt
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qwindowsysteminterface.h>

// Platform API
#include <ubuntu/application/instance.h>
#include <ubuntu/application/ui/window.h>

#include <EGL/egl.h>

#define IS_OPAQUE_FLAG 1

class UbuntuWindowPrivate
{
public:
    void createEGLSurface(EGLNativeWindowType nativeWindow);
    void destroyEGLSurface();

    UbuntuScreen* screen;
    EGLSurface eglSurface;
    WId id;
    UbuntuInput* input;
    UAUiWindow* window;
    Qt::WindowState state;
    QRect geometry;
    UApplicationInstance* uaInstance;
    UAUiWindowProperties* wProps;
};

static void eventCallback(void* context, const Event* event)
{
    DLOG("eventCallback (context=%p, event=%p)", context, event);
    DASSERT(context != NULL);
    UbuntuWindow* platformWindow = static_cast<UbuntuWindow*>(context);
    platformWindow->priv()->input->postEvent(platformWindow, event);
}

UbuntuWindow::UbuntuWindow(QWindow* w, UbuntuScreen* screen,
                           UbuntuInput* input, UApplicationInstance* instance)
    : QPlatformWindow(w)
{
    DASSERT(screen != NULL);

    d = new UbuntuWindowPrivate;
    d->screen = screen;
    d->eglSurface = EGL_NO_SURFACE;
    d->input = input;
    d->state = window()->windowState();
    d->uaInstance = instance;

    static int id = 1;
    d->id = id++;

    // Use client geometry if set explicitly, use available screen geometry otherwise.
    d->geometry = window()->geometry() != screen->geometry() ?
        window()->geometry() : screen->availableGeometry();
    createWindow();
    DLOG("UbuntuWindow::UbuntuWindow (this=%p, w=%p, screen=%p, input=%p)", this, w, screen, input);
}

UbuntuWindow::~UbuntuWindow()
{
    DLOG("UbuntuWindow::~UbuntuWindow");
    d->destroyEGLSurface();
    ua_ui_window_destroy(d->window);
    delete d;
}

void UbuntuWindowPrivate::createEGLSurface(EGLNativeWindowType nativeWindow)
{
  DLOG("UbuntuWindowPrivate::createEGLSurface (this=%p, nativeWindow=%p)",
          this, reinterpret_cast<void*>(nativeWindow));

  eglSurface = eglCreateWindowSurface(screen->eglDisplay(), screen->eglConfig(),
          nativeWindow, nullptr);

  DASSERT(eglSurface != EGL_NO_SURFACE);
}

void UbuntuWindowPrivate::destroyEGLSurface()
{
    DLOG("UbuntuWindowPrivate::destroyEGLSurface (this=%p)", this);
    if (eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(screen->eglDisplay(), eglSurface);
        eglSurface = EGL_NO_SURFACE;
    }
}

void UbuntuWindow::createWindow()
{
    DLOG("UbuntuWindow::createWindow (this=%p)", this);

    // Get surface role and flags.
    QVariant roleVariant = window()->property("role");
    int role = roleVariant.isValid() ? roleVariant.toUInt() : 1;  // 1 is the default role for apps.
    QVariant opaqueVariant = window()->property("opaque");
    uint flags = opaqueVariant.isValid() ?
        opaqueVariant.toUInt() ? static_cast<uint>(IS_OPAQUE_FLAG) : 0 : 0;

    // FIXME(loicm) Opaque flag is forced for now for non-system sessions (applications) for
    //     performance reasons.
    flags |= static_cast<uint>(IS_OPAQUE_FLAG);

    const QByteArray title = (!window()->title().isNull()) ? window()->title().toUtf8() : "Window 1"; // legacy title

    #if !defined(QT_NO_DEBUG)
    LOG("role: '%d'", role);
    LOG("flags: '%s'", (flags & static_cast<uint>(1)) ? "Opaque" : "NotOpaque");
    LOG("title: '%s'", title.constData());
    #endif

    // Get surface geometry.
    QRect geometry;
    if (d->state == Qt::WindowFullScreen) {
        printf("UbuntuWindow - fullscreen geometry\n");
        geometry = screen()->geometry();
    } else if (d->state == Qt::WindowMaximized) {
        printf("UbuntuWindow - maximized geometry\n");
        geometry = screen()->availableGeometry();
    } else {
        printf("UbuntuWindow - regular geometry\n");
        geometry = d->geometry;
    }

    DLOG("[ubuntuclient QPA] creating surface at (%d, %d) with size (%d, %d) with title '%s'\n",
            geometry.x(), geometry.y(), geometry.width(), geometry.height(), title.data());

    // Setup platform window creation properties
    d->wProps = ua_ui_window_properties_new_for_normal_window();
    ua_ui_window_properties_set_titlen(d->wProps, title.data(), title.size());
    ua_ui_window_properties_set_role(d->wProps, static_cast<UAUiWindowRole>(role));
    ua_ui_window_properties_set_event_cb_and_ctx(d->wProps, &eventCallback, this);
    ua_ui_window_properties_set_dimensions(d->wProps, geometry.width(), geometry.height());

    // Create platform window
    d->window = ua_ui_window_new_for_application_with_properties(d->uaInstance, d->wProps);

    if (geometry.x() != 0 || geometry.y() != 0)
        ua_ui_window_move(d->window, geometry.x(), geometry.y());

    DASSERT(d->window != NULL);
    d->createEGLSurface(ua_ui_window_get_native_type(d->window));
    if (d->state == Qt::WindowFullScreen) {
        ua_ui_window_request_fullscreen(d->window);
    }

    // Window manager can give us a final size different from what we asked for
    // so let's check what we ended up getting
    {
        uint32_t width, height;
        ua_ui_window_get_size(d->window, &width, &height);
        geometry.setWidth(width);
        geometry.setHeight(height);
    }

    DLOG("[ubuntuclient QPA] created surface has size (%d, %d)",
            geometry.width(), geometry.height());

    // Tell Qt about the geometry.
    QWindowSystemInterface::handleGeometryChange(window(), geometry);
    QPlatformWindow::setGeometry(geometry);
}

void UbuntuWindow::moveResize(const QRect& rect)
{
  fprintf(stderr, "\nQUbuntuWindow::moveResize (this=%p, x=%d, y=%d, w=%d, h=%d)\n", this,
          rect.x(), rect.y(), rect.width(), rect.height());
  LOG("UbuntuWindow::moveResize(width=%d, height=%d)", rect.width(), rect.height());
  ua_ui_window_move(d->window, rect.x(), rect.y());
  ua_ui_window_resize(d->window, rect.width(), rect.height());
  QWindowSystemInterface::handleGeometryChange(window(), rect);
  QPlatformWindow::setGeometry(rect);
}

void UbuntuWindow::handleResize(int width, int height)
{
    LOG("UbuntuWindow::handleResize(width=%d, height=%d)", width, height);
    QRect oldGeometry = geometry();
    QRect newGeometry = oldGeometry;
    newGeometry.setWidth(width);
    newGeometry.setHeight(height);

    QPlatformWindow::setGeometry(newGeometry);
    QWindowSystemInterface::handleGeometryChange(window(), newGeometry, oldGeometry);
    QWindowSystemInterface::flushWindowSystemEvents();
}

void UbuntuWindow::setWindowState(Qt::WindowState state)
{
    if (state == d->state)
        return;

    switch (state) {
    case Qt::WindowNoState:
        DLOG("setting window state: 'NoState'");
        moveResize(d->geometry);
        d->state = Qt::WindowNoState;
        break;

    case Qt::WindowFullScreen:
        DLOG("setting window state: 'FullScreen'");
        ua_ui_window_request_fullscreen(d->window);
        moveResize(screen()->geometry());
        d->state = Qt::WindowFullScreen;
        break;

    case Qt::WindowMaximized:
        DLOG("setting window state: 'Maximized'");
        moveResize(screen()->availableGeometry());
        d->state = Qt::WindowMaximized;
        break;

    case Qt::WindowActive:
    case Qt::WindowMinimized:
    default:
        DLOG("setting window state: 'Active|Minimized'");
        break;
    }
}

void UbuntuWindow::setGeometry(const QRect& rect)
{
    fprintf(stderr, "UbuntuWindow::setGeometry (this=%p)", this);
    d->geometry = rect;
    if (d->state != Qt::WindowFullScreen && d->state != Qt::WindowMaximized)
        moveResize(rect);
}

void UbuntuWindow::setVisible(bool visible)
{
  DLOG("UbuntuWindow::setVisible (this=%p, visible=%s)", this, visible ? "true" : "false");

  if (visible) {
    ua_ui_window_show(d->window);
    QWindowSystemInterface::handleExposeEvent(window(), QRect());
    QWindowSystemInterface::flushWindowSystemEvents();
  } else {
    ua_ui_window_hide(d->window);
  }
}

void* UbuntuWindow::eglSurface() const
{
    return d->eglSurface;
}

WId UbuntuWindow::winId() const
{
    return d->id;
}
