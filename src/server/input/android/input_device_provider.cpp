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

namespace mi = mir::input;
namespace mia = mi::android;

mi::InputDeviceProvider::Priority mia::InputDeviceProvider::get_support(mi::InputDeviceInfo const& device) const
{
    auto device_classes = device.device_classes();
    if (device_classes & InputDeviceInfo::touchscreen)
        return best;

    if (device_classes & InputDeviceInfo::touchpad)
        return unsupported;

    return supported;
}

std::shared_ptr<mi::InputDevice> mia::InputDeviceProvider::create_device(mi::InputDeviceInfo const& /*device*/) const
{
    return std::shared_ptr<mi::InputDevice>();
}

