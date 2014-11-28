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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_MOCK_INPUT_DEVICE_REGISTRY_H_
#define MIR_INPUT_MOCK_INPUT_DEVICE_REGISTRY_H_

#include "mir/input/input_device_registry.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockInputDeviceRegistry : public input::InputDeviceRegistry
{
public:
    MOCK_METHOD1(add_device, void(std::shared_ptr<input::InputDevice> const& device));
    MOCK_METHOD1(remove_device, void(std::shared_ptr<input::InputDevice> const& device));
};

}
}
}

#endif

