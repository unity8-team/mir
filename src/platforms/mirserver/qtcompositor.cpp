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

#include "qtcompositor.h"

#include <QGuiApplication>
#include <QWindow>

QtCompositor::QtCompositor()
{

}

void QtCompositor::start()
{
    // (Re)Start Qt's render thread by setting all its windows to shown
    setAllWindowsVisible(true);
}

void QtCompositor::stop()
{
    // Stop Qt's render threads by setting all its windows it hidden
    setAllWindowsVisible(false);
}

void QtCompositor::setAllWindowsVisible(bool visible)
{
    auto windowList = QGuiApplication::allWindows();
    auto iterator = windowList.constBegin();
    while (iterator != windowList.constEnd()) {
        (*iterator)->setVisible(visible);
        iterator++;
    }
}
