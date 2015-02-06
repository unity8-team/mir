/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_GEOMETRY_OVERRIDES_H_
#define MIR_GEOMETRY_OVERRIDES_H_

#include "mir_toolkit/common.h"

#include "mir/geometry/rectangle.h"

#include <map>

namespace mir
{
namespace geometry
{
struct Overrides
{
    inline MirOrientation get_orientation(uint32_t display_id, MirOrientation original)
    {
        if (end(overrides) == overrides.find(display_id))
            return original;
        return overrides[display_id];
    }
    inline Rectangle transform_rectangle(uint32_t display_id, Rectangle const& rect, MirOrientation original)
    {
        if (end(overrides) == overrides.find(display_id))
            return rect;
        if (original == overrides[display_id])
            return rect;

        switch(original)
        {
        case mir_orientation_left:
        case mir_orientation_right:
            switch(overrides[display_id])
            {
            case mir_orientation_left:
            case mir_orientation_right:
            default:
                return rect;
            case mir_orientation_inverted:
            case mir_orientation_normal:
                return swap_width_height(rect);
            }
        case mir_orientation_normal:
        case mir_orientation_inverted:
            switch(overrides[display_id])
            {
            case mir_orientation_left:
            case mir_orientation_right:
                return swap_width_height(rect);
            case mir_orientation_inverted:
            case mir_orientation_normal:
            default:
                return rect;
            }
        default:
            return rect;
        }
    }

    inline Rectangle swap_width_height(Rectangle rect)
    {
        return Rectangle{rect.top_left, Size{rect.size.height.as_int(), rect.size.width.as_int()}};
    }

    inline void add_override(uint32_t display, MirOrientation orientation)
    {
        overrides[display] = orientation;
    }
    std::map<uint32_t, MirOrientation> overrides;
};
}
}

#endif
