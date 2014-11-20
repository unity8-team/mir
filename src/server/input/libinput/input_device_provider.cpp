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

#include "../input_device_info.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace mi = mir::input;
namespace mili = mi::libinput;

mi::InputDeviceProvider::Priority mili::InputDeviceProvider::get_support(mi::InputDeviceInfo const& device) const
{
    auto device_classes = device.device_classes();

    if (device_classes & mi::InputDeviceInfo::joystick)
        return unsupported;

    if (device_classes & mi::InputDeviceInfo::gamepad)
        return unsupported;

    if (device_classes & mi::InputDeviceInfo::touchscreen)
        return unsupported;

    if (device_classes & mi::InputDeviceInfo::touchpad)
        return best;

    return supported;
}

std::shared_ptr<mi::InputDevice> mili::InputDeviceProvider::create_device(mi::InputDeviceInfo const& device) const
{
    (void)device;
    return std::shared_ptr<mi::InputDevice>();
}

