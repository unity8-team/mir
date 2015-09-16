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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_DEFAULT_DEVICE_HANDLE_H_
#define MIR_INPUT_DEFAULT_DEVICE_HANDLE_H_

#include "mir/input/device_handle.h"
#include "mir/input/input_device_info.h"

#include <memory>

namespace mir
{
namespace input
{

class DefaultDeviceHandle : public DeviceHandle
{
public:
    DefaultDeviceHandle(InputDeviceInfo const& info);
    MirInputDeviceId id() const override;
    DeviceCapabilities get_device_classes() const override;
    std::string get_name() const override;
    std::string get_unique_id() const override;
    std::unique_ptr<DeviceSettings> get_settings() const override;
    void apply_settings(DeviceSettings const& settings) const override;

private:
    InputDeviceInfo info;
};

}
}

#endif
