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

#ifndef SHELLINPUTAREA_H
#define SHELLINPUTAREA_H

#include "inputarea.h"

class MirSurface;

class ShellInputArea : public InputArea
{
    Q_OBJECT

public:
    explicit ShellInputArea(QQuickItem *parent = 0);

    virtual void setSurface(MirSurface *surface);

private Q_SLOT:
    void setShellSurface(MirSurface *surface);
};

#endif // SHELLINPUTAREA_H
