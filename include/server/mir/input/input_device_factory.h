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

#ifndef MIR_INPUT_INPUT_DEVICE_FACTORY_H_
#define MIR_INPUT_INPUT_DEVICE_FACTORY_H_

#include "mir/udev/wrapper.h"
#include "mir/input/input_device_provider.h"
#include "mir/input/input_device.h"

#include <memory>
#include <vector>
#include <initializer_list>

namespace mir
{
namespace input
{

class InputDeviceFactory
{
public:
    InputDeviceFactory(std::initializer_list<std::shared_ptr<InputDeviceProvider>> providers);

    std::shared_ptr<InputDevice> create_device(mir::udev::Device const& device);

private:
    std::vector<std::shared_ptr<InputDeviceProvider>> providers;
};

}
} // namespace mir

#endif // MIR_INPUT_INPUT_DEVICE_FACTORY_H_
