/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "default_window_manager.h"
#include "mir/scene/surface.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/shell/display_layout.h"
#include "mir/geometry/rectangle.h"

#include "mir_toolkit/client_types.h"

#include <algorithm>

namespace ms = mir::scene;
namespace msh = mir::shell;
namespace geom = mir::geometry;

msh::DefaultWindowManager::DefaultWindowManager(
    std::shared_ptr<msh::DisplayLayout> const& display_layout)
    : display_layout(display_layout)
{
}

ms::SurfaceCreationParameters msh::DefaultWindowManager::place(
    ms::Session const& /* session */,
    ms::SurfaceCreationParameters const& request_parameters)
{
    mir::graphics::DisplayConfigurationOutputId const output_id_invalid{
        mir_display_output_id_invalid};
    auto placed_parameters = request_parameters;

    geom::Rectangle rect{request_parameters.top_left, request_parameters.size};

    if (request_parameters.output_id != output_id_invalid)
    {
        display_layout->place_in_output(request_parameters.output_id, rect);
    }

    placed_parameters.top_left = rect.top_left;
    placed_parameters.size = rect.size;

    return placed_parameters;
}

int msh::DefaultWindowManager::configure(frontend::Surface& surf,
                                         MirSurfaceAttrib attrib,
                                         int value)
{
    // FIXME: Remove cast by changing ApplicationSession
    auto& surface = *dynamic_cast<scene::Surface*>(&surf);

    switch (attrib)
    {
    case mir_surface_attrib_state:
        return set_state(surface, static_cast<MirSurfaceState>(value));
    default:
        return surf.configure(attrib, value);
    }
}

int msh::DefaultWindowManager::set_state(scene::Surface& surface,
                                         MirSurfaceState desire) const
{
    auto type = surface.type();

    switch (desire)
    {
    case mir_surface_state_fullscreen:
        if (type == mir_surface_type_normal)
            return set_fullscreen(surface);
        break;
    default:
        return surface.configure(mir_surface_attrib_state, desire);
    }

    // TODO Unsupported combo: Return unchanged or exception?
    return surface.state();
}

int msh::DefaultWindowManager::set_fullscreen(scene::Surface& surface) const
{
    int old_state = surface.state();
    int new_state = surface.configure(mir_surface_attrib_state,
                                      mir_surface_state_fullscreen);

    if (new_state == mir_surface_state_fullscreen)
    {
        geometry::Rectangle rect{surface.top_left(), surface.size()};
        display_layout->size_to_output(rect);
        surface.resize(rect.size);
        surface.move_to(rect.top_left);
    }
    else
    {
        surface.configure(mir_surface_attrib_state, old_state);
    }

    return new_state;
}
