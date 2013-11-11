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

#include "surfaceconfigurator.h"

namespace msh = mir::shell;

SurfaceConfigurator::SurfaceConfigurator()
{
    qRegisterMetaType<MirSurfaceAttrib>("MirSurfaceAttrib");
}

int SurfaceConfigurator::select_attribute_value(msh::Surface const&, MirSurfaceAttrib, int requested_value)
{
    return requested_value;
}

void SurfaceConfigurator::attribute_set(msh::Surface const& surface, MirSurfaceAttrib attrib, int value)
{
    Q_EMIT surfaceAttributeChanged(&surface, attrib, value);
}
