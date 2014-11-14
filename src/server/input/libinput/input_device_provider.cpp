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

#include "input_device_provider.h"

#include "mir/fd.h"
#include "mir/raii.h"

#include <libinput.h>
#include <libevdev/libevdev.h>

#include <boost/exception/errinfo_errno.hpp>
#include <boost/exception/errinfo_file_name.hpp>
#include <boost/throw_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace mi = mir::input;
namespace mili = mi::libinput;

namespace
{
int libinput_open_restricted(const char *path, int flags, void *user_data)
{
    (void)user_data;
    return ::open(path, flags);
}

void libinput_close_restricted(int fd, void *user_data)
{
    (void)user_data;
    ::close(fd);
}

int get_num_abs_axis(libevdev * evdev, std::initializer_list<int> axis_list)
{
    int ret = 0;
    for (auto const axis : axis_list)
        if (libevdev_has_event_code( evdev, EV_ABS, axis))
            ++ret;
    return ret;
}

bool has_abs(libevdev* evdev)
{
    return 2 == get_num_abs_axis(evdev, {ABS_X, ABS_Y});
}

bool has_mt(libevdev* evdev)
{
    return 2 == get_num_abs_axis(evdev, {ABS_MT_POSITION_X, ABS_MT_POSITION_Y});
}

bool is_touchpad(libevdev * evdev)
{
    return libevdev_has_event_type(evdev, EV_KEY) &&
        !libevdev_has_property(evdev, INPUT_PROP_DIRECT) &&
        libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN) &&
        (has_abs(evdev) || has_mt(evdev));
}

bool is_joystick_with_three_axis(libevdev * evdev)
{
    return
        !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN) &&
        has_abs(evdev) &&
        !has_mt(evdev) &&
        get_num_abs_axis(evdev,
                         {ABS_X, ABS_Y, ABS_Z,
                         ABS_RX, ABS_RY, ABS_RZ,
                         ABS_THROTTLE, ABS_RUDDER, ABS_WHEEL, ABS_GAS, ABS_BRAKE,
                         ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y, ABS_HAT2X, ABS_HAT2Y, ABS_HAT3X, ABS_HAT3Y,
                         ABS_TILT_X, ABS_TILT_Y
                         }) > 2;
}

bool is_multi_touch_screen(libevdev * evdev)
{
    return
        !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_FINGER) &&
        !libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN) &&
        has_mt(evdev);
}
}

static const libinput_interface open_close_interface =
{
    libinput_open_restricted, libinput_close_restricted
};

mili::InputDeviceProvider::InputDeviceProvider()
    : libinput_handle(
        libinput_path_create_context(&open_close_interface, nullptr),
        &libinput_destroy
        )
{
}

mi::InputDeviceProvider::Priority mili::InputDeviceProvider::probe_device(mir::udev::Device const& device) const
{
    mir::Fd dev(
        ::open(device.devpath(), O_RDONLY|O_NONBLOCK)
        );

    libevdev* evdev = nullptr;
    int rc = libevdev_new_from_fd(dev, &evdev);

    if (rc < 0)
    {
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(
                std::runtime_error("Failed to probe input device")
                )
            << boost::errinfo_errno(-rc)
            << boost::errinfo_file_name(device.devpath())
            );
    }

    auto on_exit = mir::raii::deleter_for(evdev, &libevdev_free);

    if (is_touchpad(evdev))
        return best;
    else if (is_joystick_with_three_axis(evdev))
        return unsupported;
    else if (is_multi_touch_screen(evdev))
        return unsupported;
    else
        return supported;
}

std::shared_ptr<mi::InputDevice> mili::InputDeviceProvider::create_device(mir::udev::Device const& device) const
{
    (void)device;
    return std::shared_ptr<mi::InputDevice>();
}

mili::InputDeviceProvider::~InputDeviceProvider()
{
}
