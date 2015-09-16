/*
 * Copyright © 2015 Canonical Ltd.
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

#include "default_device_settings.h"

#include <boost/exception/exception.hpp>
#include <stdexcept>

namespace mi = mir::input;


mi::DeviceSettings::Value mi::DefaultDeviceSettings::get(Setting setting) const
{
    auto it = settings.find(setting);
    if (it == end(settings))
        return {DeviceSettings::NotApplicable{}};
    return it->second;
}

void mi::DefaultDeviceSettings::set(Setting setting, Value const& value)
{
    if (value.which() == 0)
        BOOST_THROW_EXCEPTION(std::invalid_argument("Invalid value type provided"));

    auto it = settings.find(setting);

    if (it == end(settings))
        BOOST_THROW_EXCEPTION(std::invalid_argument("Setting not supported by device"));
    if (value.which() != it->second.which())
        BOOST_THROW_EXCEPTION(std::invalid_argument("Wrong value type used"));

    it->second = value;
}

void mi::DefaultDeviceSettings::for_each_setting(std::function<void(Setting, Value const&)> const& element_callback) const
{
    for( auto const& item : settings)
        element_callback(Setting{item.first}, item.second);
}

void mi::DefaultDeviceSettings::add_setting(Setting setting, Value const& value)
{
    settings[setting] = value;
}
