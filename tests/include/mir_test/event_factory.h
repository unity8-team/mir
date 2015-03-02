/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_EVENT_FACTORY_H
#define MIR_TEST_EVENT_FACTORY_H

#include "mir/geometry/point.h"

namespace mir
{
namespace input
{
namespace synthesis
{

enum class EventAction
{
    Down, Up
};

class KeyParameters
{
public:
    KeyParameters();

    KeyParameters& from_device(int device_id);
    KeyParameters& of_scancode(int scancode);
    KeyParameters& with_action(EventAction action);

    int device_id;
    int scancode;
    EventAction action;
};
KeyParameters a_key_down_event();
KeyParameters a_key_up_event();

class ButtonParameters
{
public:
    ButtonParameters();
    ButtonParameters& from_device(int device_id);
    ButtonParameters& of_button(int scancode);
    ButtonParameters& with_action(EventAction action);

    int device_id;
    int button;
    EventAction action;
};
ButtonParameters a_button_down_event();
ButtonParameters a_button_up_event();

class MotionParameters
{
public:
    MotionParameters();
    MotionParameters& from_device(int device_id);
    MotionParameters& with_movement(int rel_x, int rel_y);

    int device_id;
    int rel_x;
    int rel_y;
};
MotionParameters a_pointer_event();

class TouchParameters
{
public:
    enum class Action
    {
        Tap = 0,
        Move,
        Release
    };

    TouchParameters();
    TouchParameters& from_device(int device_id);
    TouchParameters& at_position(geometry::Point abs_pos);
    TouchParameters& with_action(Action touch_action);
    
    int device_id;
    int abs_x;
    int abs_y;
    Action action;
};
TouchParameters a_touch_event();

}
}
}

#endif /* MIR_TEST_EVENT_FACTORY_H */
