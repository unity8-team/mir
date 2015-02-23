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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "display_input_region.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"

#include "mir/geometry/rectangle.h"
#include "mir/geometry/rectangles.h"

#include <iostream>

namespace mi = mir::input;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

mi::DisplayInputRegion::DisplayInputRegion(
    std::shared_ptr<mg::Display> const& display)
    : display{display}
{
}

void mi::DisplayInputRegion::override_orientation(uint32_t display_id, MirOrientation orientation)
{
    overrides.add_override(display_id, orientation);
}

MirOrientation mi::DisplayInputRegion::get_orientation(geometry::Point const& /*point*/)
{
    uint32_t display_id = 0;
    //since we are going clone mode - position does not matter
    return overrides.get_orientation(display_id, mir_orientation_normal);
    /*
    MirOrientation orientation = mir_orientation_normal;
    display->for_each_display_buffer(
        [this,&display_id,point,&orientation](mg::DisplayBuffer const& buffer)
        {
            if (buffer.view_area().contains(point))
            {
                orientation = overrides.get_orientation(display_id, buffer.orientation());
            }
            ++ display_id;
        });

    return orientation;*/
}

geom::Rectangle mi::DisplayInputRegion::bounding_rectangle()
{
    geom::Width width{INT_MAX};
    geom::Height height{INT_MAX};

    display->for_each_display_buffer(
        [&width, &height](mg::DisplayBuffer const& buffer)
        {
            auto area = buffer.view_area();
            width = std::min(width, area.size.width);
            height = std::min(height, area.size.height);
        });

    return {{0,0}, {width, height}};
}

void mi::DisplayInputRegion::confine(geom::Point& point)
{
    geom::Rectangles rects;
    rects.add(bounding_rectangle());
    rects.confine(point);
}
