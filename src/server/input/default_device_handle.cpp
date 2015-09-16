/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "default_device_handle.h"
#include "default_device_settings.h"

namespace mi = mir::input;

mi::DefaultDeviceHandle::DefaultDeviceHandle(mi::InputDeviceInfo const& info)
    : info{info}
{
}

mi::DeviceCapabilities mi::DefaultDeviceHandle::get_device_classes() const
{
    return info.capabilities;
}

std::string mi::DefaultDeviceHandle::get_name() const
{
    return info.name;
}

std::string mi::DefaultDeviceHandle::get_unique_id() const
{
    return info.unique_id;
}

std::unique_ptr<mi::DeviceSettings> mi::DefaultDeviceHandle::get_settings() const
{
    return std::make_unique<mi::DefaultDeviceSettings>();
}

void mi::DefaultDeviceHandle::apply_settings(DeviceSettings const& /*settings*/) const
{
}

MirInputDeviceId mi::DefaultDeviceHandle::id() const
{
    return info.id;
}

