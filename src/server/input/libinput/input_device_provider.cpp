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

#include "mir/input/input_device_detection.h"

#include <libinput.h>

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
    InputDeviceDetection detector(device.devpath());

    const int joystick_axes_exposed_by_libinput = 2;

    if (detector.has_touchpad())
        return best;
    else if (detector.has_joystick() &&
             detector.num_joystick_axes() > joystick_axes_exposed_by_libinput)
        return unsupported;
    else if (detector.has_multi_touch_screen())
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
