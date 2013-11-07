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

#ifndef SURFACECONFIGURATOR_H
#define SURFACECONFIGURATOR_H

#include <QObject>

#include <mir/shell/surface_configurator.h>
#include <mir_toolkit/common.h>

namespace mir { namespace shell { class Surface; }}

class SurfaceConfigurator : public QObject, public mir::shell::SurfaceConfigurator
{
    Q_OBJECT

public:
    SurfaceConfigurator();

    int select_attribute_value(mir::shell::Surface const&, MirSurfaceAttrib, int) override;
    void attribute_set(mir::shell::Surface const&, MirSurfaceAttrib, int) override;

Q_SIGNALS:
    void surfaceAttributeChanged(mir::shell::Surface const*, const MirSurfaceAttrib, const int);
};

#endif // SURFACECONFIGURATOR_H
