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

#include "qtcompositor.h"
#include "displaywindow.h"

#include <QGuiApplication>
#include <QWindow>

#include <QDebug>

QtCompositor::QtCompositor()
{

}

void QtCompositor::start()
{
    // (Re)Start Qt's render thread by setting all its windows to exposed
    setAllWindowsExposed(true);
}

void QtCompositor::stop()
{
    // Stop Qt's render threads by setting all its windows it obscured
    setAllWindowsExposed(false);
}

void QtCompositor::setAllWindowsExposed(bool exposed)
{
    qDebug() << "QtCompositor::setAllWindowsExposed" << exposed;
    // likely need a lock
    DisplayWindow::m_isExposed = exposed;

    QList<QWindow *> windowList = QGuiApplication::allWindows();

    // manipulate Qt object's indirectly via posted events as we're not in Qt's GUI thread
    auto iterator = windowList.constBegin();
    while (iterator != windowList.constEnd()) {
        QWindow *window = *iterator;
        DisplayWindow *displayWindow = static_cast<DisplayWindow*>(window->handle());
        if (displayWindow) {
            QCoreApplication::postEvent(displayWindow, new QEvent(DisplayWindow::exposeEventType));
        }
        iterator++;
    }
}
