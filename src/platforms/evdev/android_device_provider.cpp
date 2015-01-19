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

#include "android_device_provider.h"
#include "evdev_device_detection.h"

#include "mir/input/device_capability.h"
#include "mir/input/input_device.h"

namespace mi = mir::input;
namespace mie = mi::evdev;

mie::Priority mie::AndroidDeviceProvider::probe_device(char const* device) const
{
    auto device_caps = detect_device_capabilities(device);

    if (contains(device_caps, DeviceCapability::touchscreen))
        return Priority::best;

    if (contains(device_caps, DeviceCapability::touchpad))
        return Priority::unsupported;

    if (device_caps == DeviceCapability::unknown)
        return Priority::unsupported;

    return Priority::supported;
}

std::unique_ptr<mi::InputDevice> mie::AndroidDeviceProvider::create_device(char const* /*device*/) const
{
    return std::unique_ptr<mi::InputDevice>();
}

