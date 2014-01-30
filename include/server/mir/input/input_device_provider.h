/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_INPUT_INPUT_DEVICE_PROVIDER_H_
#define MIR_INPUT_INPUT_DEVICE_PROVIDER_H_

#include "mir/udev/wrapper.h"
#include <mir/input/input_device.h>

#include <memory>

namespace mir
{
namespace input
{

class InputDeviceProvider
{
public:
    enum Priority {
	UNSUPPORTED = 0,
	SUPPORTED = 100,
	BEST = 255,
    };

    virtual ~InputDeviceProvider() noexcept {}

    virtual Priority ProbeDevice(mir::udev::Device const& device) const = 0;
    virtual std::shared_ptr<InputDevice> create_device(mir::udev::Device const& device) const = 0;
};

}
} // namespace mir

#endif // MIR_INPUT_INPUT_DEVICE_PROVIDER_H_
