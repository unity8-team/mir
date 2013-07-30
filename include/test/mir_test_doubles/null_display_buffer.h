/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_NULL_DISPLAY_BUFFER_H_
#define MIR_TEST_DOUBLES_NULL_DISPLAY_BUFFER_H_

#include "mir/graphics/display_buffer.h"

namespace mir
{
namespace test
{
namespace doubles
{

class NullDisplayBuffer : public graphics::DisplayBuffer
{
public:
    geometry::Rectangle view_area() const { return geometry::Rectangle(); }
    void make_current() {}
    void release_current() {}
    void post_update() {}
};

}
}
}

#endif /* MIR_TEST_DOUBLES_NULL_DISPLAY_BUFFER_H_ */
