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

#include "fullscreen_placement_strategy.h"

#include "mir/shell/surface.h"
#include "mir/shell/display_layout.h"
#include "mir/geometry/rectangle.h"

namespace me = mir::examples;
namespace msh = mir::shell;

me::FullscreenPlacementStrategy::FullscreenPlacementStrategy(
    std::shared_ptr<msh::DisplayLayout> const& display_layout)
  : display_layout(display_layout)
{
}

void me::FullscreenPlacementStrategy::place(msh::Surface& surface) const
{
    geometry::Rectangle rect{surface.top_left(), surface.size()};
    display_layout->size_to_output(rect);
    surface.resize(rect.size);
    surface.move_to(rect.top_left);
}
