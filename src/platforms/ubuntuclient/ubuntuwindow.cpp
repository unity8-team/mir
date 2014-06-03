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
#include <QMutex>
#include <QMutexLocker>
#include <QRegion>
#include <QSize>

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
    QSize bufferSize;
    QSize targetBufferSize;
    QMutex mutex;
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
    : QObject(nullptr), QPlatformWindow(w)
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

    // Assume that the buffer size matches the surface size at creation time
    d->bufferSize = geometry.size();

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

void UbuntuWindow::handleSurfaceResize(int width, int height)
{
    LOG("UbuntuWindow::handleSurfaceResize(width=%d, height=%d)", width, height);

    // The current buffer size hasn't actually changed. so just render on it and swap
    // buffers until we render on a buffer with the target size.

    bool shouldSwapBuffers;

    {
        QMutexLocker(&d->mutex);
        d->targetBufferSize.rwidth() = width;
        d->targetBufferSize.rheight() = height;

        shouldSwapBuffers = d->bufferSize != d->targetBufferSize;
    }

    if (shouldSwapBuffers) {
        QWindowSystemInterface::handleExposeEvent(window(), geometry());
    } else {
        qWarning("[ubuntuclient QPA] UbuntuWindow::handleSurfaceResize"
                 " current buffer already has the target size");
        d->targetBufferSize = QSize();
    }
}

void UbuntuWindow::handleBufferResize(int width, int height)
{
    LOG("UbuntuWindow::handleBufferResize(width=%d, height=%d)", width, height);

    QRect oldGeometry;
    QRect newGeometry;

    {
        QMutexLocker(&d->mutex);
        oldGeometry = geometry();
        newGeometry = oldGeometry;
        newGeometry.setWidth(width);
        newGeometry.setHeight(height);

        d->bufferSize.rwidth() = width;
        d->bufferSize.rheight() = height;
        d->geometry = newGeometry;
    }

    QPlatformWindow::setGeometry(newGeometry);
    QWindowSystemInterface::handleGeometryChange(window(), newGeometry, oldGeometry);
    QWindowSystemInterface::handleExposeEvent(window(), newGeometry);
}

void UbuntuWindow::forceRedraw()
{
    QWindowSystemInterface::handleExposeEvent(window(), geometry());
}

void UbuntuWindow::setWindowState(Qt::WindowState state)
{
    QMutexLocker(&d->mutex);
    if (state == d->state)
        return;

    switch (state) {
    case Qt::WindowNoState:
        DLOG("setting window state: 'NoState'");
        d->state = Qt::WindowNoState;
        break;

    case Qt::WindowFullScreen:
        DLOG("setting window state: 'FullScreen'");
        ua_ui_window_request_fullscreen(d->window);
        d->state = Qt::WindowFullScreen;
        break;

    case Qt::WindowMaximized:
        DLOG("setting window state: 'Maximized'");
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

    bool doMoveResize;

    {
        QMutexLocker(&d->mutex);
        d->geometry = rect;
        doMoveResize = d->state != Qt::WindowFullScreen && d->state != Qt::WindowMaximized;
    }

    if (doMoveResize) {
        moveResize(rect);
    }
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

    setExposed(visible);
}

bool UbuntuWindow::isExposed() const {
    return m_exposed;
}

void UbuntuWindow::setExposed(const bool exposed)
{
    DLOG("QUbuntuWindow::setExposed (this=%p, exposed=%s)", this, exposed ? "true" : "false");

    if (m_exposed == exposed)
        return;

    m_exposed = exposed;
    if (m_exposed) {
        QWindowSystemInterface::handleExposeEvent(window(), geometry());
    } else {
        QWindowSystemInterface::handleExposeEvent(window(), QRect());
    }
}

void UbuntuWindow::windowEvent(QEvent *event)
{
    if (event->type() == QEvent::Expose) {
        QRegion region = static_cast<QExposeEvent *>(event)->region();

        if (region.isEmpty()) {
            // hiding window causes Qt to release the GL context and its resources, which is a bit severe
            // Instead can use the exposure system to stop the rendering loop, but hold onto the resources
            setExposed(false);
        } else {
            setExposed(true);
        }
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

void UbuntuWindow::onBuffersSwapped_threadSafe(int newBufferWidth, int newBufferHeight)
{
    QMutexLocker(&d->mutex);

    bool sizeKnown = newBufferWidth > 0 && newBufferHeight > 0;

    if (sizeKnown && (d->bufferSize.width() != newBufferWidth ||
                d->bufferSize.height() != newBufferHeight)) {
        QMetaObject::invokeMethod(this, "handleBufferResize",
                Qt::QueuedConnection,
                Q_ARG(int, newBufferWidth), Q_ARG(int, newBufferHeight));
    } else {
        // buffer size hasn't changed
        if (d->targetBufferSize.isValid()) {
            if (d->bufferSize != d->targetBufferSize) {
                // but we still didn't reach the promised buffer size from the mir resize event.
                // thus keep swapping buffers
                QMetaObject::invokeMethod(this, "forceRedraw", Qt::QueuedConnection);
            } else {
                // target met. we have just provided a render with the target size and
                // can therefore finally rest.
                d->targetBufferSize = QSize();
            }
        }
    }
}
