/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/shell/surface_source.h"
#include "mir/shell/surface_builder.h"
#include "mir/shell/surface.h"
#include "mir/frontend/surface.h"

#include <cassert>

namespace ms = mir::surfaces;
namespace msh = mir::shell;

//TODO WRONG DEPENDENCY
msh::SurfaceSource::SurfaceSource()
{
}

std::shared_ptr<msh::Surface> msh::SurfaceSource::create_surface(
    std::weak_ptr<ms::Surface> const& surface,
    shell::SurfaceCreationParameters const& params,
    frontend::SurfaceId id,
    std::shared_ptr<events::EventSink> const& sink)
{
    return std::make_shared<Surface>(
        surface,
        params,
        id,
        sink);
}
