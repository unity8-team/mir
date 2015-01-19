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

#include "libinput_device_provider.h"
#include "evdev_device_detection.h"

#include "mir/input/input_device.h"
#include "mir/input/device_capability.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace mi = mir::input;
namespace mie = mi::evdev;

mie::Priority mie::LibInputDeviceProvider::probe_device(char const* device) const
{
    auto device_caps = detect_device_capabilities(device);

    if (contains(device_caps, DeviceCapability::joystick))
        return Priority::unsupported;

    if (contains(device_caps, DeviceCapability::gamepad))
        return Priority::unsupported;

    if (contains(device_caps, DeviceCapability::touchscreen))
        return Priority::unsupported;

    if (contains(device_caps, DeviceCapability::touchpad))
        return Priority::best;

    if (device_caps == DeviceCapability::unknown)
        return Priority::unsupported;

    if (contains(device_caps, DeviceCapability::pointer))
        return Priority::supported;

    if (contains(device_caps, DeviceCapability::keyboard))
        return Priority::unsupported;

    return Priority::unsupported;
}

std::unique_ptr<mi::InputDevice> mie::LibInputDeviceProvider::create_device(char const* device) const
{
    (void)device;
    return std::unique_ptr<mi::InputDevice>();
}

