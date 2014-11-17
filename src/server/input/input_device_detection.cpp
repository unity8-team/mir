/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/input/input_device_detection.h"

#include "mir/fd.h"
#include "mir/raii.h"

#include <libevdev/libevdev.h>

#include <boost/exception/errinfo_errno.hpp>
#include <boost/exception/errinfo_file_name.hpp>
#include <boost/throw_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace mi = mir::input;
namespace
{
int get_num_abs_axes(libevdev * evdev, std::initializer_list<int> axes_list)
{
    int ret = 0;
    for (auto const axis : axes_list)
        if (libevdev_has_event_code( evdev, EV_ABS, axis))
            ++ret;
    return ret;
}

bool has_coordinates(libevdev* evdev)
{
    return 2 == get_num_abs_axes(evdev, {ABS_X, ABS_Y});
}

bool has_mt_coordinates(libevdev* evdev)
{
    return 2 == get_num_abs_axes(evdev, {ABS_MT_POSITION_X, ABS_MT_POSITION_Y});
}
}

mi::InputDeviceDetection::InputDeviceDetection(char const* devpath)
    : input_device(::open(devpath, O_RDONLY|O_NONBLOCK)),
    evdev_ptr(nullptr, &libevdev_free)
{
    libevdev* evdev = nullptr;
    int rc = libevdev_new_from_fd(input_device, &evdev);
    if (rc < 0)
    {
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(
                std::runtime_error("Failed to probe input device")
                )
            << boost::errinfo_errno(-rc)
            << boost::errinfo_file_name(devpath)
            );
    }

    evdev_ptr = mir::raii::deleter_for(evdev, &libevdev_free);
}

bool mi::InputDeviceDetection::has_touchpad() const
{
    bool has_keys = libevdev_has_event_type(evdev_ptr.get(), EV_KEY);
    bool indirect_input = !libevdev_has_property(evdev_ptr.get(), INPUT_PROP_DIRECT);
    bool finger_but_no_pen =
        libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_PEN);

    return has_keys && indirect_input && finger_but_no_pen &&
        (has_coordinates(evdev_ptr.get()) || has_mt_coordinates(evdev_ptr.get()));
}

bool mi::InputDeviceDetection::has_joystick() const
{
    return
        !libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_PEN) &&
        has_coordinates(evdev_ptr.get()) &&
        !has_mt_coordinates(evdev_ptr.get());
}

int mi::InputDeviceDetection::num_joystick_axes() const
{
    return get_num_abs_axes(evdev_ptr.get(),
                         {ABS_X, ABS_Y, ABS_Z,
                         ABS_RX, ABS_RY, ABS_RZ,
                         ABS_THROTTLE, ABS_RUDDER, ABS_WHEEL, ABS_GAS, ABS_BRAKE,
                         ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y, ABS_HAT2X, ABS_HAT2Y, ABS_HAT3X, ABS_HAT3Y,
                         ABS_TILT_X, ABS_TILT_Y
                         });
}

bool mi::InputDeviceDetection::has_multi_touch_screen() const
{
    return
        !libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev_ptr.get(), EV_KEY, BTN_TOOL_PEN) &&
        has_mt_coordinates(evdev_ptr.get());
}
