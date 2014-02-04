/*
 * Copyright © 2013 Canonical Ltd.
 *
b * This program is free software: you can redistribute it and/or modify
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
 */

#include <boost/throw_exception.hpp>

#include "mir/input/input_device_factory.h"

namespace mi = mir::input;

mi::InputDeviceFactory::InputDeviceFactory(std::initializer_list<std::unique_ptr<InputDeviceProvider>> providers)
{
    for (auto& provider : providers)
    {
        if (!provider)
        {
            BOOST_THROW_EXCEPTION(
                std::logic_error("Attempt to construct InputDeviceFactory from null InputDeviceProvider"));
        }
        // Did you know that std::initializer_list really, really doesn't like MoveAssignable?
        // This is safe; there's no valid way to construct a std::unique_ptr that's compile-time constant,
        // so the compiler is guaranteed not to put the initializer_list in RO memory.
        //
        // Should someone manage to construct a compile-time constant ptr, crashing here will be
        // friendlier than possibly crashing later when the unique_ptr tries to destroy the
        // object.
        this->providers.push_back(std::move(const_cast<std::unique_ptr<InputDeviceProvider>&>(provider)));
    }
}

std::unique_ptr<mi::InputDevice> mir::input::InputDeviceFactory::create_device(mir::udev::Device const& device)
{
    mi::InputDeviceProvider::Priority best_prio = mi::InputDeviceProvider::UNSUPPORTED;
    mi::InputDeviceProvider* best_provider = nullptr;
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
        return best_provider->create_device(device);

    // TODO: What do we do here?
    return std::unique_ptr<mi::InputDevice>();
}
