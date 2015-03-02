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
#include "mir/graphics/display_configuration.h"
#include "mir/display_changer.h"

#include "mir/geometry/rectangle.h"
#include "mir/geometry/size.h"
#include "mir/geometry/point.h"
#include "mir/geometry/rectangles.h"

#include <iostream>

namespace mi = mir::input;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

namespace
{
void update_rectangles(mg::DisplayConfiguration const& conf, geom::Rectangles& rectangles, geom::Point& display_pos)
{
    geom::Width width{std::numeric_limits<int>::max()};
    geom::Height height{std::numeric_limits<int>::max()};
    bool first_display = true;
    conf.for_each_output(
        [&](mg::DisplayConfigurationOutput const& output)
        {
            if (output.power_mode == mir_power_mode_on &&
                output.current_mode_index < output.modes.size())
            {
                auto output_size = output.modes[output.current_mode_index].size;

                if (first_display)
                {
                     first_display = false;
                     display_pos = {-output.top_left.x.as_int(), -output.top_left.y.as_int()};
                }

                width = std::min(width, output_size.width);
                height = std::min(height, output_size.height);
            }
        });

    rectangles.add({{0,0}, {width, height}});

    std::cout << "Display is offseted by" << display_pos.x.as_int() <<  " " << display_pos.y.as_int() << std::endl;
}
}

mi::DisplayInputRegion::DisplayInputRegion(
    mg::DisplayConfiguration const& initial_conf,
    std::shared_ptr<mir::DisplayChanger> const& display_changer)
    : display_pos{0, 0}
{
    update_rectangles(initial_conf, rectangles, display_pos);
    display_changer->register_change_callback(
        [this](mg::DisplayConfiguration const& conf)
        {
            std::unique_lock<std::mutex> lock(rectangles_lock);
            update_rectangles(conf, rectangles, display_pos);
        });
}

void mi::DisplayInputRegion::override_orientation(uint32_t display_id, MirOrientation orientation)
{
    std::unique_lock<std::mutex> lock(rectangles_lock);
    overrides.add_override(display_id, orientation);
}

MirOrientation mi::DisplayInputRegion::get_orientation(geometry::Point const& /*point*/)
{
    std::unique_lock<std::mutex> lock(rectangles_lock);
    uint32_t display_id = 0;
    MirOrientation orientation = mir_orientation_normal;

    return overrides.get_orientation(display_id, orientation);
}

geom::Rectangle mi::DisplayInputRegion::bounding_rectangle()
{
    std::unique_lock<std::mutex> lock(rectangles_lock);
    return rectangles.bounding_rectangle();
}

void mi::DisplayInputRegion::confine(geom::Point& point)
{
    std::unique_lock<std::mutex> lock(rectangles_lock);
    rectangles.confine(point);
}


mir::geometry::Point mi::DisplayInputRegion::get_display_position() const
{
    return display_pos;
}
