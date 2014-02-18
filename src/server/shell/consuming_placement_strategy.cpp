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

void msh::ConsumingPlacementStrategy::place(msh::Surface& surface) const
{
    geom::Rectangle rect{surface.top_left(), surface.size()};

#if 0 // TODO: expose preferred output from shell::Surface
    mir::graphics::DisplayConfigurationOutputId const output_id_invalid{
        mir_display_output_id_invalid};

    if (request_parameters.output_id != output_id_invalid)
    {
        display_layout->place_in_output(request_parameters.output_id, rect);
    }
    else
#endif
    if (rect.size.width > geom::Width{0} && rect.size.height > geom::Height{0})
        display_layout->clip_to_output(rect);
    else
        display_layout->size_to_output(rect);

    surface.resize(rect.size);  // might fail (throw exceptions)
    surface.move_to(rect.top_left);  // only move once resize successful
}
