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

#ifndef SURFACECONTROLLER_H
#define SURFACECONTROLLER_H

#include <QObject>

#include "mir/shell/surface_builder.h"

namespace mir { namespace surfaces { class SurfaceStackModel; }}

class SurfaceBuilder : public QObject, public mir::shell::SurfaceBuilder
{
    Q_OBJECT

public:
    explicit SurfaceBuilder(const std::shared_ptr<mir::shell::SurfaceBuilder>& surfaceBuilder, QObject *parent = 0);
    ~SurfaceBuilder();

    virtual void destroy_surface(std::weak_ptr<mir::surfaces::Surface> const& surface) override;

    virtual std::weak_ptr<mir::surfaces::Surface> create_surface(
            mir::shell::Session* session,
            const mir::shell::SurfaceCreationParameters &params) override;

private:
    const std::shared_ptr<mir::shell::SurfaceBuilder> m_surfaceBuilder;
};

#endif // SURFACECONTROLLER_H
