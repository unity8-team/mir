/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GEOMETRY_POINT_H_
#define MIR_GEOMETRY_POINT_H_

#include "dimensions.h"
#include <ostream>

namespace mir
{
namespace geometry
{

struct Point
{
    Point() : Point(0, 0) {}
    Point(X i, Y j) : x{i}, y{j} {}

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

inline std::ostream& operator<<(std::ostream& out, Point const& value)
{
    out << '(' << value.x << ", " << value.y << ')';
    return out;
}

}
}

#endif /* MIR_GEOMETRY_POINT_H_ */
