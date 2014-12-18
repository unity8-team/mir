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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "libinput_device.h"
#include "libinput_wrapper.h"

#include "mir/input/input_event_handler_register.h"
#include "mir/input/event_sink.h"
#include "mir/input/cursor_filter.h"
#include <libinput.h>

#include <cstring>
#include <iostream>

namespace mie = mir::input::evdev;


struct mie::LibInputDevice::KeyboardState
{
public:
    KeyboardState();
    void convert_event(MirEvent& mir_event, libinput_event_keyboard* keyboard);
private:
    std::vector<std::pair<uint32_t,uint32_t>> down_times;
    MirKeyModifier modifier_state;
};

mie::LibInputDevice::KeyboardState::KeyboardState()
    : modifier_state{mir_key_modifier_none}
{}

void mie::LibInputDevice::KeyboardState::convert_event(MirEvent& mir_event, libinput_event_keyboard* keyboard)
{
    mir_event.type = mir_event_type_key;
    mir_event.key.action =
        libinput_event_keyboard_get_key_state(keyboard) == LIBINPUT_KEY_STATE_PRESSED ?
        mir_key_action_down :
        mir_key_action_up;
    mir_event.key.scan_code = libinput_event_keyboard_get_key(keyboard);
    mir_event.key.event_time = libinput_event_keyboard_get_time(keyboard);

    // TODO scan code to key code converstion.
    // TODO? is_system key and flags canot be dervied from libinput
    // device and source ids will be filled by other layers
    // TODO track event for down time
    // TODO track event for modifier - do we send modifier events?
}

struct mie::LibInputDevice::PointerState
{
public:
    PointerState();
    void set_filter(std::shared_ptr<CursorFilter> filter);
    void apply_filter(float& x, float& y, float& delta_x, float& delta_y);
    void apply_filter(float& hscroll, float& vscroll);
    void convert_event(MirEvent& mir_event, libinput_event_type ev_type, libinput_event_pointer* pointer);
private:
    MirMotionButton button_state;
    std::shared_ptr<CursorFilter> cursor_filter;
    float current_x, current_y;
    uint32_t down_time;
};

mie::LibInputDevice::PointerState::PointerState()
    : button_state(MirMotionButton(0)), current_x(0), current_y(0)
{
}

void mie::LibInputDevice::PointerState::apply_filter(float& x, float& y, float& delta_x, float& delta_y)
{
    if (cursor_filter)
        cursor_filter->filter_cursor_movement(x, y, delta_x, delta_y);
    else
    {
        x += delta_x;
        y += delta_y;
    }
}

void mie::LibInputDevice::PointerState::apply_filter(float& hscroll, float& vscroll)
{
    if (cursor_filter)
        cursor_filter->filter_scroll(hscroll, vscroll);
}

void mie::LibInputDevice::PointerState::convert_event(MirEvent& mir_event, libinput_event_type ev_type, libinput_event_pointer* pointer)
{
    bool no_button_pressed = button_state == 0;
    float relative_x = 0;
    float relative_y = 0;
    switch(ev_type)
    {
    case LIBINPUT_EVENT_POINTER_MOTION:
        mir_event.motion.action =
            no_button_pressed ?
            mir_motion_action_hover_move :
            mir_motion_action_move;
        relative_x = float(libinput_event_pointer_get_dx(pointer));
        relative_y = float(libinput_event_pointer_get_dy(pointer));

        apply_filter(current_x, current_y, relative_x, relative_y);
        break;
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
        mir_event.motion.action =
            no_button_pressed ?
            mir_motion_action_hover_move :
            mir_motion_action_move;
        current_x = float(libinput_event_pointer_get_absolute_x(pointer));
        current_y = float(libinput_event_pointer_get_absolute_y(pointer));
        apply_filter(current_x, current_y, relative_x, relative_y);
        break;
    case LIBINPUT_EVENT_POINTER_BUTTON:
        {
            auto button = uint32_t(1 << (libinput_event_pointer_get_button(pointer) - 1));

            if (libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED)
            {
                mir_event.motion.action =  mir_motion_action_down;
                button_state = MirMotionButton(uint32_t(button_state) | button);
            }
            else
            {
                mir_event.motion.action = mir_motion_action_up;
                button_state = MirMotionButton(uint32_t(button_state) & ~button);
            }
            break;
        }
    case LIBINPUT_EVENT_POINTER_AXIS:
        mir_event.motion.action = mir_motion_action_scroll;
        if (libinput_event_pointer_get_axis(pointer) == LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)
            mir_event.motion.pointer_coordinates[0].vscroll = libinput_event_pointer_get_axis_value(pointer);
        else
            mir_event.motion.pointer_coordinates[0].hscroll = libinput_event_pointer_get_axis_value(pointer);
        apply_filter(mir_event.motion.pointer_coordinates[0].hscroll, mir_event.motion.pointer_coordinates[0].vscroll);
        break;
    default:
        break;
    }

    mir_event.motion.event_time = libinput_event_pointer_get_time(pointer);
    if (button_state == 0)
        down_time = mir_event.motion.event_time;
    mir_event.motion.down_time = down_time;

    mir_event.type = mir_event_type_motion;
    mir_event.motion.button_state = button_state;
    mir_event.motion.pointer_count = 1;
    mir_event.motion.pointer_coordinates[0].tool_type = mir_motion_tool_type_mouse;
    mir_event.motion.pointer_coordinates[0].x = current_x;
    mir_event.motion.pointer_coordinates[0].y = current_y;
    mir_event.motion.pointer_coordinates[0].raw_x = current_x;
    mir_event.motion.pointer_coordinates[0].raw_y = current_y;
}

void mie::LibInputDevice::PointerState::set_filter(std::shared_ptr<CursorFilter> filter)
{
    cursor_filter = std::move(filter);
}

mie::LibInputDevice::LibInputDevice(std::shared_ptr<mie::LibInputWrapper> const& lib, char const* path)
    : path(path), dev(nullptr,&libinput_device_unref), lib(lib)
{
}

mie::LibInputDevice::~LibInputDevice() = default;

void mie::LibInputDevice::start(InputEventHandlerRegister& registry, EventSink& sink)
{
    dev = lib->add_device(path);
    lib->start_device(registry, this);
    this->sink = &sink;
}

void mie::LibInputDevice::stop(InputEventHandlerRegister& registry)
{
    sink = nullptr;
    lib->stop_device(registry, this);
    dev.reset();
}

void mie::LibInputDevice::process_event(libinput_event* event)
{
    if (!sink)
        return;

    MirEvent mir_event;
    std::memset(&mir_event, 0, sizeof mir_event);

    auto ev_type = libinput_event_get_type(event);

    switch(ev_type)
    {
    case LIBINPUT_EVENT_KEYBOARD_KEY:
        get_keyboard().convert_event(mir_event, libinput_event_get_keyboard_event(event));
        sink->handle_input(mir_event);
        break;
    case LIBINPUT_EVENT_POINTER_MOTION:
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
    case LIBINPUT_EVENT_POINTER_BUTTON:
    case LIBINPUT_EVENT_POINTER_AXIS:
        get_pointer().convert_event(mir_event, ev_type, libinput_event_get_pointer_event(event));
        sink->handle_input(mir_event);
        break;
    // Touch device support is not used:
    case LIBINPUT_EVENT_TOUCH_DOWN:
    case LIBINPUT_EVENT_TOUCH_UP:
    case LIBINPUT_EVENT_TOUCH_MOTION:
    case LIBINPUT_EVENT_TOUCH_CANCEL:
    case LIBINPUT_EVENT_TOUCH_FRAME:
    default:
        break;
    }
}

libinput_device* mie::LibInputDevice::device() const
{
    return dev.get();
}

auto mie::LibInputDevice::get_pointer() -> PointerState&
{
    if (!pointer)
        pointer.reset(new PointerState);
    return *pointer.get();
}

auto mie::LibInputDevice::get_keyboard() -> KeyboardState&
{
    if (!keyboard)
        keyboard.reset(new KeyboardState);
    return *keyboard.get();
}

void mie::LibInputDevice::set_cursor_filter(std::shared_ptr<CursorFilter> filter)
{
    get_pointer().set_filter(std::move(filter));
}
