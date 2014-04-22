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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */


#include "default_input_registrar.h"

namespace ms = mir::scene;

void ms::DefaultInputRegistrar::add_observer(std::shared_ptr<InputRegistrarObserver> const& observer)
{
    std::unique_lock<std::mutex> lock(observers_mutex);
    observers.push_back(observer);
}

void ms::DefaultInputRegistrar::remove_observer(std::shared_ptr<InputRegistrarObserver> const& /*observer*/)
{
}

void ms::DefaultInputRegistrar::input_channel_opened(std::shared_ptr<input::InputChannel> const& channel,
                                                     std::shared_ptr<input::Surface> const& surface,
                                                     input::InputReceptionMode mode)
{
    decltype(observers) current_observers;
    {
        std::unique_lock<std::mutex> lock(observers_mutex);
        current_observers = observers;
    }

    for( auto const& observer : current_observers )
    {
        observer->input_channel_opened(channel, surface, mode);
    }
}

void ms::DefaultInputRegistrar::input_channel_closed(std::shared_ptr<input::InputChannel> const& channel)
{
    decltype(observers) current_observers;
    {
        std::unique_lock<std::mutex> lock(observers_mutex);
        current_observers = observers;
    }

    for( auto const& observer : current_observers )
    {
        observer->input_channel_closed(channel);
    }
}

