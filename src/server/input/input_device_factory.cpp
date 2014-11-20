/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/input/input_device_factory.h"

#include "evdev_device_info.h"

namespace mi = mir::input;

mi::InputDeviceFactory::InputDeviceFactory(std::initializer_list<std::shared_ptr<mi::InputDeviceProvider>> providers)
    : providers(providers)
{
}

std::shared_ptr<mi::InputDevice> mir::input::InputDeviceFactory::create_device(mir::input::InputDeviceInfo const& info)
{
    InputDeviceProvider::Priority best_prio = mi::InputDeviceProvider::unsupported;
    InputDeviceProvider* best_provider = nullptr;

    for (auto& provider : providers)
    {
        auto prio = provider->get_support(info);
        if (prio > best_prio)
        {
            best_prio = prio;
            best_provider = provider.get();
        }
    }

    if (best_provider != nullptr)
	return best_provider->create_device(info);

    return std::shared_ptr<mi::InputDevice>();
}
