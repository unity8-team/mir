/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_CURSOR_FILTER_H_
#define MIR_INPUT_CURSOR_FILTER_H_

namespace mir
{
namespace input
{
class CursorFilter
{
public:
    CursorFilter() = default;
    virtual ~CursorFilter() = default;
    /**
     * Shall be called by InputDevices to update an absolute position based on relative movement.
     * All values will be updated to reflect the new position and movement delta after the call.
     */
    virtual void filter_cursor_movement(float& x, float& y, float& delta_x, float& delta_y) = 0;
    virtual void filter_scroll(float& hscroll, float& vscroll) = 0;
private:
    CursorFilter(CursorFilter const&) = delete;
    CursorFilter& operator=(CursorFilter const&) = delete;
};
}
}

#endif
