/*
 * Copyright © 2012,2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GEOMETRY_POINT_H_
#define MIR_GEOMETRY_POINT_H_

#include "dimensions.h"
#include <iosfwd>
#include <cmath>

namespace mir
{
namespace geometry
{

struct Point
{
    Point() = default;
    Point(Point const&) = default;
    Point& operator=(Point const&) = default;

    template<typename XType, typename YType>
    Point(XType&& x, YType&& y) : x(x), y(y) {}

    X x;
    Y y;
};

inline bool operator == (Point const& lhs, Point const& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator != (Point const& lhs, Point const& rhs)
{
    return lhs.x != rhs.x || lhs.y != rhs.y;
}

inline Point operator *(float lhs, Point const& rhs)
{
    return Point{std::round(rhs.x.as_float()*lhs),
        std::round(rhs.y.as_float()*lhs)};
}

inline Point operator *(Point const& lhs, float rhs)
{
    return rhs*lhs;
}

inline Point operator /(Point const& lhs, float rhs)
{
    return Point{std::round(lhs.x.as_float()/rhs),
        std::round(lhs.y.as_float()/rhs)};
}



std::ostream& operator<<(std::ostream& out, Point const& value);
}
}

#endif /* MIR_GEOMETRY_POINT_H_ */
