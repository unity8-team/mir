/*
 * Copyright (C) 2013,2014 Canonical, Ltd.
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
 * Authors: Gerry Boland <gerry.boland@canonical.com>
 *          Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include "displaywindow.h"

#include "mir/geometry/size.h"

#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformscreen.h>

#include <QDebug>

static WId newWId()
{
    static WId id = 0;

    if (id == std::numeric_limits<WId>::max())
        qWarning("MirServer QPA: Out of window IDs");

    return ++id;
}

DisplayWindow::DisplayWindow(QWindow *window, mir::graphics::DisplayBuffer *displayBuffer)
    : QObject(nullptr), QPlatformWindow(window)
    , m_isExposed(true)
    , m_winId(newWId())
    , m_displayBuffer(displayBuffer)
{
    qDebug() << "DisplayWindow::DisplayWindow";
    qWarning("Window %p: %p 0x%x\n", this, window, uint(m_winId));

    QRect screenGeometry(screen()->availableGeometry());
    if (window->geometry() != screenGeometry) {
        setGeometry(screenGeometry);
    }
    window->setSurfaceType(QSurface::OpenGLSurface);

    // The compositor window is always active. I.e., it's always focused so that
    // it always processes key events, etc
    requestActivateWindow();
}

QRect DisplayWindow::geometry() const
{
    // For yet-to-become-fullscreen windows report the geometry covering the entire
    // screen. This is particularly important for Quick where the root object may get
    // sized to some geometry queried before calling create().
    return screen()->availableGeometry();
}

void DisplayWindow::setGeometry(const QRect &)
{
    // We only support full-screen windows
    QRect rect(screen()->availableGeometry());
    QWindowSystemInterface::handleGeometryChange(window(), rect);
    QPlatformWindow::setGeometry(rect);
}

bool DisplayWindow::isExposed() const
{
    return m_isExposed;
}

bool DisplayWindow::event(QEvent *event)
{
    // Intercept Hide event and convert to Expose event, as Hide causes Qt to release GL
    // resources, which we don't want. Must intercept Show to un-do hide.
    if (event->type() == QEvent::Hide) {
        qDebug() << "DisplayWindow::event got QEvent::Hide";
        m_isExposed = false;
        QWindowSystemInterface::handleExposeEvent(window(), QRect());
        QWindowSystemInterface::flushWindowSystemEvents();
        return true;
    } else if (event->type() == QEvent::Show) {
        qDebug() << "DisplayWindow::event got QEvent::Show";
        m_isExposed = true;
        QRect rect(QPoint(), geometry().size());
        QWindowSystemInterface::handleExposeEvent(window(), rect);
        QWindowSystemInterface::flushWindowSystemEvents();
        return true;
    }
    return QObject::event(event);
}

void DisplayWindow::swapBuffers()
{
    m_displayBuffer->gl_swap_buffers();
    m_displayBuffer->flip();
}

void DisplayWindow::makeCurrent()
{
    m_displayBuffer->make_current();
}

void DisplayWindow::doneCurrent()
{
    m_displayBuffer->release_current();
}
