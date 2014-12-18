/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "evdev_input_device_factory.h"
#include "input_device_provider.h"

#include "mir/find_best.h"
#include "mir/input/input_device.h"

#include <stdexcept>

namespace mi = mir::input;
namespace mie = mi::evdev;

mie::EvdevInputDeviceFactory::EvdevInputDeviceFactory(std::initializer_list<std::shared_ptr<mie::InputDeviceProvider>> providers)
    : providers(providers)
{
}

std::unique_ptr<mi::InputDevice> mie::EvdevInputDeviceFactory::create_device(char const* device)
{
    auto best_provider = find_best(
        providers,
        [device](std::shared_ptr<InputDeviceProvider> const& provider) -> Priority
        {
            try
            {
                return provider->probe_device(device);
            }
            catch(...)
            {
                return Priority::unsupported;
            }
        },
        Priority::unsupported
        );

    if (best_provider != end(providers))
	return std::move((*best_provider)->create_device(device));

    throw std::runtime_error("Failed to open input device");
}
