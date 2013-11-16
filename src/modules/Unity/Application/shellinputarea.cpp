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

// own
#include "shellinputarea.h"

// local
#include "mirsurfacemanager.h"

// Qt
#include <QDebug>

ShellInputArea::ShellInputArea(QQuickItem *parent) : InputArea(parent)
{
    auto surface = MirSurfaceManager::singleton()->shellSurface();
    if (surface) {
        doSetSurface(surface);
    }

    connect(MirSurfaceManager::singleton(), &MirSurfaceManager::shellSurfaceChanged,
            this, &ShellInputArea::setShellSurface);
}

void ShellInputArea::setSurface(MirSurface * /*surface*/)
{
    qWarning() << "ShellInputArea::setSurface called, you're doing something wrong";
}

void ShellInputArea::setShellSurface(MirSurface *surface)
{
    doSetSurface(surface);
}
