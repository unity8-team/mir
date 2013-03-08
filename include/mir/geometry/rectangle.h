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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 *              Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GEOMETRY_RECTANGLE_H_
#define MIR_GEOMETRY_RECTANGLE_H_

#include "point.h"
#include "size.h"

#include <ostream>

namespace mir
{
namespace geometry
{

struct Rectangle
{
    Point top_left;
    Size size;
};

inline bool operator == (Rectangle const& lhs, Rectangle const& rhs)
{
    return lhs.top_left == rhs.top_left && lhs.size == rhs.size;
}

inline bool operator != (Rectangle const& lhs, Rectangle const& rhs)
{
    return lhs.top_left != rhs.top_left || lhs.size != rhs.size;
}

inline std::ostream& operator<<(std::ostream& out, Rectangle const& value)
{
    out << '(' << value.top_left << ", " << value.size << ')';
    return out;
}

}
}

#endif /* MIR_GEOMETRY_RECTANGLE_H_ */
