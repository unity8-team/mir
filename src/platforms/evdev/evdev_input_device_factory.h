/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_EVDEV_EVDEV_INPUT_DEVICE_FACTORY_H_
#define MIR_INPUT_EVDEV_EVDEV_INPUT_DEVICE_FACTORY_H_

#include "input_device_factory.h"

#include <memory>
#include <vector>
#include <initializer_list>

namespace mir
{
namespace input
{
namespace evdev
{
class InputDeviceProvider;

class EvdevInputDeviceFactory : public InputDeviceFactory
{
public:
    EvdevInputDeviceFactory(std::initializer_list<std::shared_ptr<InputDeviceProvider>> providers);

    std::unique_ptr<InputDevice> create_device(char const* devcie_node) override;

private:
    std::vector<std::shared_ptr<InputDeviceProvider>> providers;
};

}
}
} // namespace mir

#endif // MIR_INPUT_EVDEV_INPUT_DEVICE_FACTORY_H_
