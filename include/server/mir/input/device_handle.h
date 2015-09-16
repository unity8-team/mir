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

#ifndef MIR_INPUT_DEVICE_HANDLE_H_
#define MIR_INPUT_DEVICE_HANDLE_H_

#include "mir/input/device_capability.h"
#include "mir_toolkit/event.h"

#include <memory>

namespace mir
{
namespace input
{

class DeviceSettings;

class DeviceHandle
{
public:
    DeviceHandle() = default;
    virtual ~DeviceHandle() = default;
    virtual MirInputDeviceId id() const = 0;
    virtual DeviceCapabilities get_device_classes() const = 0;
    virtual std::string get_name() const = 0;
    virtual std::string get_unique_id() const = 0;
    /*!
     * Returns a copy of the current settings of this device
     */
    virtual std::unique_ptr<DeviceSettings> get_settings() const = 0;
    /*!
     * Update the device with the givien settings.
     */
    virtual void apply_settings(DeviceSettings const& settings) const = 0;

private:
    DeviceHandle(DeviceHandle const&) = delete;
    DeviceHandle& operator=(DeviceHandle const&) = delete;
};

}
}

#endif
