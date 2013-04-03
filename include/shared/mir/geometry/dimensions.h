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
 */

#ifndef MIR_GEOMETRY_DIMENSIONS_H_
#define MIR_GEOMETRY_DIMENSIONS_H_

#include <cstdint>
#include <iosfwd>

namespace mir
{

/// Basic geometry types. Types for dimensions, displacements, etc.
/// and the operations that they support.
namespace geometry
{
typedef uint32_t Width;
typedef uint32_t Height;
typedef uint32_t Stride;

typedef uint32_t X;
typedef uint32_t Y;
typedef uint32_t DeltaX;
typedef uint32_t DeltaY;
}
}

#endif /* MIR_GEOMETRY_DIMENSIONS_H_ */
