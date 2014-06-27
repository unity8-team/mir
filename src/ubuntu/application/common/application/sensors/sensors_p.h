/*
 * Copyright (C) 2014 Canonical Ltd
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
 *      Author: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

#include <core/dbus/dbus.h>
#include <core/dbus/fixture.h>
#include <core/dbus/object.h>
#include <core/dbus/property.h>
#include <core/dbus/service.h>
#include <core/dbus/interfaces/properties.h>
#include <core/dbus/types/stl/tuple.h>
#include <core/dbus/types/stl/vector.h>

#include <core/dbus/asio/executor.h>

#include <vector>
#include <memory>

namespace dbus = core::dbus;

struct UbuntuApplicationSensorsHaptic
{
    UbuntuApplicationSensorsHaptic(std::shared_ptr<dbus::Object> session)
        : enabled(false), 
          session(session)
    {
    }

    ~UbuntuApplicationSensorsHaptic()
    {
        bus->stop();

        if (bus_thread.joinable())
            bus_thread.join();
    }

    bool enabled;
    std::shared_ptr<dbus::Object> session;
    std::thread bus_thread;
    std::shared_ptr<dbus::Bus> bus;
};
