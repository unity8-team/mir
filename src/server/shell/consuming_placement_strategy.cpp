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

#include "consuming_placement_strategy.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/shell/display_layout.h"
#include "mir/shell/surface.h"
#include "mir/geometry/rectangle.h"

#include "mir_toolkit/client_types.h"

#include <algorithm>

namespace msh = mir::shell;
namespace geom = mir::geometry;

msh::ConsumingPlacementStrategy::ConsumingPlacementStrategy(
    std::shared_ptr<msh::DisplayLayout> const& display_layout)
    : display_layout(display_layout)
{
}

msh::SurfaceCreationParameters msh::ConsumingPlacementStrategy::place(
    msh::SurfaceCreationParameters const& request_parameters) const
{
    mir::graphics::DisplayConfigurationOutputId const output_id_invalid{
        mir_display_output_id_invalid};
    auto placed_parameters = request_parameters;

    geom::Rectangle rect{request_parameters.top_left, request_parameters.size};

    if (request_parameters.output_id != output_id_invalid)
    {
        display_layout->place_in_output(request_parameters.output_id, rect);
    }
    else if (request_parameters.size.width > geom::Width{0} &&
             request_parameters.size.height > geom::Height{0})
    {
        display_layout->clip_to_output(rect);
    }
    else
    {
        display_layout->size_to_output(rect);
    }

    placed_parameters.top_left = rect.top_left;
    placed_parameters.size = rect.size;

    return placed_parameters;
}

void msh::ConsumingPlacementStrategy::place(msh::Surface& surface) const
{
    geometry::Rectangle rect{surface.top_left(), surface.size()};
    bool changed = false;

    /*
     * This could all be move into a shell::Surface::place() really. But
     * it seems more cohesive to use PlacementStrategy while we still rely
     * on this class for managing SurfaceCreationParameters.
     * Also keep in mind that placement doesn't just need updating on state
     * changes, but also when the layout changes (like screen resolution).
     */

    switch (surface.state())
    {
        case mir_surface_state_fullscreen:
            // TODO save restore rect
            display_layout->size_to_output(rect);
            changed = true;
            break;
        case mir_surface_state_restored:
            // TODO: if rect != restored_rect then set it
            break;
        default:
            break;
    }

    if (changed)
    {
        surface.resize(rect.size);
        surface.move_to(rect.top_left);
    }
}
