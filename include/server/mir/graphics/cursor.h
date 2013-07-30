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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_GRAPHICS_CURSOR_H_
#define MIR_GRAPHICS_CURSOR_H_

#include "mir/geometry/size.h"
#include "mir/geometry/point.h"

namespace mir
{
namespace graphics
{
class Cursor
{
public:
    virtual void set_image(void const* raw_argb, geometry::Size size) = 0;
    virtual void move_to(geometry::Point position) = 0;
    virtual void set_hotspot(geometry::Point hotspot) = 0;

protected:
    Cursor() = default;
    virtual ~Cursor() = default;
    Cursor(Cursor const&) = delete;
    Cursor& operator=(Cursor const&) = delete;
};
}
}


#endif /* MIR_GRAPHICS_CURSOR_H_ */
