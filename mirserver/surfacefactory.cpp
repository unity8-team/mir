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

// local
#include "surfacefactory.h"
#include "logging.h"

// mir
#include "mir/shell/surface_builder.h"
#include "mir/shell/surface_configurator.h"

namespace msh = mir::shell;

SurfaceFactory::SurfaceFactory(std::shared_ptr<mir::shell::SurfaceFactory> const& surfaceFactory,
                             QObject *parent)
    : QObject(parent)
    , m_surfaceFactory(surfaceFactory)
{
    qRegisterMetaType<std::shared_ptr<msh::Surface>>("std::shared_ptr<mir::shell::Surface>");
}

SurfaceFactory::~SurfaceFactory()
{
}

std::shared_ptr<msh::Surface>
SurfaceFactory::create_surface(
        mir::shell::Session *session,
        msh::SurfaceCreationParameters const& params,
        mir::frontend::SurfaceId id,
        std::shared_ptr<mir::frontend::EventSink> const& sink)
{
    DLOG("SurfaceFactory::create_surface");
    std::shared_ptr<msh::Surface> mirSurface = m_surfaceFactory->create_surface(session, params, id, sink);

    static bool shellSurfaceFound = false; //FIXME: Hack to work around fact unable to detect shell surface
    if (!shellSurfaceFound) {
        m_shellSurface = mirSurface;
        shellSurfaceFound = true;
        Q_EMIT shellSurfaceCreated(mirSurface);
    }
    return mirSurface;
}

std::shared_ptr<mir::shell::Surface> SurfaceFactory::shellSurface() const
{
    return m_shellSurface;
}
