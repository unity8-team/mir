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

#include <mir/shell/surface_creation_parameters.h> // TODO: Remove ~racarr

#include "surfacebuilder.h"

#include "logging.h"

namespace msh = mir::shell;
namespace ms = mir::surfaces;

SurfaceBuilder::SurfaceBuilder(const std::shared_ptr<mir::shell::SurfaceBuilder>& surfaceBuilder, QObject *parent) :
    QObject(parent),
    m_surfaceBuilder(surfaceBuilder)
{
    DLOG("SurfaceBuilder::SurfaceBuilder (this=%p)", this);
}

SurfaceBuilder::~SurfaceBuilder()
{
    DLOG("SurfaceBuilder::~SurfaceBuilder (this=%p)", this);
}

void SurfaceBuilder::destroy_surface(std::weak_ptr<mir::surfaces::Surface> const& surface)
{
    return m_surfaceBuilder->destroy_surface(surface);
}


std::weak_ptr<ms::Surface> SurfaceBuilder::create_surface(msh::Session *session, const msh::SurfaceCreationParameters& params)
{
    DLOG("SurfaceBuilder::~create_surface (this=%p)", this);
    Q_UNUSED(session)

    static const ms::DepthId shellSurfaceDepth{1};
    static const ms::DepthId oskSurfaceDepth{2};
    static bool shellSurfaceFound = false;

    if (params.name == "Qml Phone Shell" && !shellSurfaceFound) { //TODO(greyback) Need to identify shell surface more securely
        DLOG("Shell depth");
        auto newParams = params;
        newParams.depth = shellSurfaceDepth;
        newParams.input_mode = mir::input::InputReceptionMode::receives_all_input;        
        shellSurfaceFound = true;
        return m_surfaceBuilder->create_surface(session, newParams);
    } else if (params.name == "MaliitOnScreenKeyboard") { // FIXME: string hardcoded into maliit
        DLOG("OSK depth");
        auto newParams = params;
        newParams.depth = oskSurfaceDepth;
        return m_surfaceBuilder->create_surface(session, newParams);
    } else {
        DLOG("Default depth");
        return m_surfaceBuilder->create_surface(session, params);
    }
}

