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

#ifndef MIR_INPUT_DEFAULT_DEVICE_SETTINGS_H_
#define MIR_INPUT_DEFAULT_DEVICE_SETTINGS_H_

#include "mir/input/device_settings.h"
#include <functional>
#include <unordered_map>

namespace std
{
template<>
struct hash<mir::input::DeviceSettings::Setting>
{
    typedef mir::input::DeviceSettings::Setting argument_type;
    typedef std::size_t result_type;

    inline result_type operator()(argument_type const& s) const
    {
        return static_cast<result_type>(s);
    }
};
}

namespace mir
{
namespace input
{

class DefaultDeviceSettings : public DeviceSettings
{
public:
    Value get(Setting setting) const override;
    void set(Setting setting, Value const& value) override;
    void for_each_setting(
        std::function<void(Setting setting, Value const& value)> const& element_calback) const override;

    void add_setting(Setting setting, Value const& value);
private:
    std::unordered_map<Setting, Value> settings;
};

}
}

#endif
