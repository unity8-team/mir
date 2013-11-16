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
#include "inputfilterarea.h"

// local
#include "mirsurfacemanager.h"

// Qt
#include <QDebug>

InputFilterArea::InputFilterArea(QQuickItem *parent) : InputArea(parent)
{
    auto surface = MirSurfaceManager::singleton()->shellSurface();
    if (surface) {
        doSetSurface(surface);
    }

    connect(MirSurfaceManager::singleton(), &MirSurfaceManager::shellSurfaceChanged,
            this, &InputFilterArea::setShellSurface);

    connect(this, &InputFilterArea::enabledChanged,
            this, &InputFilterArea::blockInputChanged);
}

void InputFilterArea::setSurface(MirSurface * /*surface*/)
{
    qWarning() << "InputFilterArea::setSurface called, you're doing something wrong";
}

void InputFilterArea::setShellSurface(MirSurface *surface)
{
    doSetSurface(surface);
}

bool InputFilterArea::blockInput() const
{
    return enabled();
}

void InputFilterArea::setBlockInput(bool enable)
{
    setEnabled(enable);
}
