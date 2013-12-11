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
#include <ubuntu/application/ui/window.h>

#define IS_OPAQUE_FLAG 1

// FIXME(loicm) The fullscreen API from Ubuntu Platform isn't good enough as we can't leave
//     fullscreen. The current Ubuntu Platform fullscreen call allows the application manager to
//     know the fullscreen state of an application, it's still the application responsibility to set
//     the right surface geometry.

static void eventCallback(void* context, const Event* event)
{
    DLOG("eventCallback (context=%p, event=%p)", context, event);
    DASSERT(context != NULL);
    UbuntuWindow* window = static_cast<UbuntuWindow*>(context);
    window->input()->postEvent(window->window(), event);
}

UbuntuWindow::UbuntuWindow(QWindow* w, UbuntuScreen* screen,
                           UbuntuInput* input, UApplicationInstance* instance)
    : QPlatformWindow(w)
    , mScreen(screen)
    , mEglSurface(EGL_NO_SURFACE)
    , mInput(input)
    , mState(window()->windowState())
    , mUainstance(instance)
{
  DASSERT(screen != NULL);
  static int id = 1;
  mId = id++;

  // Use client geometry if set explicitly, use available screen geometry otherwise.
  mGeometry = window()->geometry() != screen->geometry() ?
              window()->geometry() : screen->availableGeometry();
  createWindow();
  DLOG("UbuntuWindow::UbuntuWindow (this=%p, w=%p, screen=%p, input=%p)", this, w, screen, input);
}

UbuntuWindow::~UbuntuWindow()
{
    DLOG("UbuntuWindow::~UbuntuWindow");
    destroyEGLSurface();
    ua_ui_window_destroy(mWindow);
}

void UbuntuWindow::createEGLSurface(EGLNativeWindowType nativeWindow)
{
  DLOG("UbuntuWindow::createEGLSurface (this=%p, nativeWindow=%p)",
          this, reinterpret_cast<void*>(nativeWindow));

  mEglSurface = eglCreateWindowSurface(mScreen->eglDisplay(), mScreen->eglConfig(),
          nativeWindow, nullptr);

  DASSERT(mEglSurface != EGL_NO_SURFACE);
}

void UbuntuWindow::destroyEGLSurface()
{
    DLOG("UbuntuWindow::destroyEGLSurface (this=%p)", this);
    if (mEglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mScreen->eglDisplay(), mEglSurface);
        mEglSurface = EGL_NO_SURFACE;
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
    //ASSERT(role <= ON_SCREEN_KEYBOARD_ACTOR_ROLE);
    const char* const roleString[] = {
        "Dash", "Default", "Indicator", "Notifications", "Greeter", "Launcher", "OSK", "ShutdownDialog"
    };
    LOG("role: '%s'", roleString[role]);
    LOG("flags: '%s'", (flags & static_cast<uint>(1)) ? "Opaque" : "NotOpaque");
    LOG("title: '%s'", title.constData());
    #endif

    // Get surface geometry.
    QRect geometry;
    if (mState == Qt::WindowFullScreen) {
        printf("UbuntuWindow - fullscreen geometry\n");
        geometry = screen()->geometry();
    } else if (mState == Qt::WindowMaximized) {
        printf("UbuntuWindow - maximized geometry\n");
        geometry = screen()->availableGeometry();
    } else {
        printf("UbuntuWindow - regular geometry\n");
        geometry = mGeometry;
    }

    fprintf(stderr, "creating surface at (%d, %d) with size (%d, %d) with title '%s'",
            geometry.x(), geometry.y(), geometry.width(), geometry.height(), title.data());

    // Setup platform window creation properties
    mWprops = ua_ui_window_properties_new_for_normal_window();
    ua_ui_window_properties_set_titlen(mWprops, title.data(), title.size());
    ua_ui_window_properties_set_role(mWprops, static_cast<UAUiWindowRole>(role));
    ua_ui_window_properties_set_input_cb_and_ctx(mWprops, &eventCallback, this);
    ua_ui_window_properties_set_dimensions(mWprops, geometry.width(), geometry.height());

    // Create platform window
    mWindow = ua_ui_window_new_for_application_with_properties(mUainstance, mWprops);

    if (geometry.x() != 0 || geometry.y() != 0)
        ua_ui_window_move(mWindow, geometry.x(), geometry.y());

    DASSERT(mWindow != NULL);
    createEGLSurface(ua_ui_window_get_native_type(mWindow));
    if (mState == Qt::WindowFullScreen) {
        ua_ui_window_request_fullscreen(mWindow);
    }

    // Tell Qt about the geometry.
    QWindowSystemInterface::handleGeometryChange(window(), geometry);
    QPlatformWindow::setGeometry(geometry);
}

void UbuntuWindow::moveResize(const QRect& rect)
{
  fprintf(stderr, "\nQUbuntuWindow::moveResize (this=%p, x=%d, y=%d, w=%d, h=%d)\n", this,
          rect.x(), rect.y(), rect.width(), rect.height());
  ua_ui_window_move(mWindow, rect.x(), rect.y());
  ua_ui_window_resize(mWindow, rect.width(), rect.height());
  QWindowSystemInterface::handleGeometryChange(window(), rect);
  QPlatformWindow::setGeometry(rect);
}

void UbuntuWindow::setWindowState(Qt::WindowState state)
{
    if (state == mState)
        return;

    switch (state) {
    case Qt::WindowNoState:
        DLOG("setting window state: 'NoState'");
        moveResize(mGeometry);
        mState = Qt::WindowNoState;
        break;

    case Qt::WindowFullScreen:
        DLOG("setting window state: 'FullScreen'");
        ua_ui_window_request_fullscreen(mWindow);
        moveResize(screen()->geometry());
        mState = Qt::WindowFullScreen;
        break;

    case Qt::WindowMaximized:
        DLOG("setting window state: 'Maximized'");
        moveResize(screen()->availableGeometry());
        mState = Qt::WindowMaximized;
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
    mGeometry = rect;
    if (mState != Qt::WindowFullScreen && mState != Qt::WindowMaximized)
        moveResize(rect);
}

void UbuntuWindow::setVisible(bool visible)
{
  DLOG("UbuntuWindow::setVisible (this=%p, visible=%s)", this, visible ? "true" : "false");
  mScreen->toggleSensors(visible);

  if (visible) {
    ua_ui_window_show(mWindow);
    QWindowSystemInterface::handleExposeEvent(window(), QRect());
  } else {
    ua_ui_window_hide(mWindow);
  }
}
