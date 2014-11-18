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

#include "../input_device_detection.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace mi = mir::input;
namespace mili = mi::libinput;

mi::InputDeviceProvider::Priority mili::InputDeviceProvider::probe_device(mir::udev::Device const& device) const
{
    InputDeviceDetection detector(device.devpath());

    const int joystick_axes_exposed_by_libinput = 2;

    if (detector.has_touchpad())
        return best;

    if (detector.has_joystick() &&
        detector.num_joystick_axes() > joystick_axes_exposed_by_libinput)
        return unsupported;

    if (detector.has_multi_touch_screen())
        return unsupported;

    return supported;
}

std::shared_ptr<mi::InputDevice> mili::InputDeviceProvider::create_device(mir::udev::Device const& device) const
{
    (void)device;
    return std::shared_ptr<mi::InputDevice>();
}

