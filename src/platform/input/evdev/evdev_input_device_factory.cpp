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

#include "mir/input/input_device.h"

namespace mi = mir::input;
namespace mie = mi::evdev;

mie::EvdevInputDeviceFactory::EvdevInputDeviceFactory(std::initializer_list<std::shared_ptr<mie::InputDeviceProvider>> providers)
    : providers(providers)
{
}

std::unique_ptr<mi::InputDevice> mie::EvdevInputDeviceFactory::create_device(char const* device)
{
    auto best_prio = Priority::unsupported;
    InputDeviceProvider* best_provider = nullptr;

    for (auto& provider : providers)
    {
        auto prio = provider->probe_device(device);
        if (prio > best_prio)
        {
            best_prio = prio;
            best_provider = provider.get();
        }
    }

    if (best_provider != nullptr)
	return std::move(best_provider->create_device(device));

    return std::unique_ptr<mi::InputDevice>();
}
