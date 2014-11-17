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

namespace mi = mir::input;
namespace mia = mi::android;

mia::InputDeviceProvider::InputDeviceProvider()
{
}

mi::InputDeviceProvider::Priority mia::InputDeviceProvider::probe_device(mir::udev::Device const& device) const
{
    InputDeviceDetection detector(device.devpath());

    if (detector.has_multi_touch_screen())
        return best;
    else if (detector.has_touchpad())
        return unsupported;
    else
        return supported;
}

std::shared_ptr<mi::InputDevice> mia::InputDeviceProvider::create_device(mir::udev::Device const& device) const
{
    (void)device;
    return std::shared_ptr<mi::InputDevice>();
}

mia::InputDeviceProvider::~InputDeviceProvider()
{
}
