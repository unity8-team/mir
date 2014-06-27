/*
 * Copyright (C) 2013 Canonical Ltd
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

#include "ubuntu/application/sensors/haptic.h"
#include "usensord_service.h"
#include "sensors_p.h"

#include <stdlib.h>

template<typename T>
struct Holder
{
    Holder(const T&value = T()) : value(value)
    {
    }

    T value;
};

template<typename T>
Holder<T>* make_holder(const T& value)
{
    return new Holder<T>(value);
}

namespace dbus = core::dbus;
namespace uas = ubuntu::application::sensors;

UASensorsHaptic*
ua_sensors_haptic_new()
{
    auto bus = std::shared_ptr<dbus::Bus>(new dbus::Bus(core::dbus::WellKnownBus::session));
    bus->install_executor(dbus::asio::make_executor(bus));

    auto stub_service = dbus::Service::use_service(bus, dbus::traits::Service<uas::USensorD>::interface_name());
    auto stub = stub_service->object_for_path(dbus::types::ObjectPath("/com/canonical/usensord/haptic"));

    auto holder = make_holder(new UbuntuApplicationSensorsHaptic(stub));
    holder->value->bus_thread = std::thread{[bus](){ bus->run(); }};
    holder->value->bus = bus;

    return make_holder(new UbuntuApplicationSensorsHaptic(stub));
}

UStatus
ua_sensors_haptic_enable(UASensorsHaptic* sensor)
{
    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);
    s->value->enabled = true;

    return U_STATUS_SUCCESS;
}

UStatus
ua_sensors_haptic_disable(UASensorsHaptic* sensor)
{
    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);
    s->value->enabled = false;

    return U_STATUS_SUCCESS;
}

UStatus
ua_sensors_haptic_vibrate_once(
    UASensorsHaptic* sensor,
    uint32_t duration)
{
    if (sensor == nullptr)
        return U_STATUS_ERROR;

    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);

    if (s->value->enabled == false)
        return U_STATUS_ERROR;

    try
    {
        s->value->session->invoke_method_synchronously<uas::USensorD::Haptic::Vibrate, void>(duration);
    }
    catch (const std::runtime_error& e)
    {
        std::cout << e.what() << std::endl;
        return U_STATUS_ERROR;
    }

    return U_STATUS_SUCCESS;
}

UStatus
ua_sensors_haptic_vibrate_with_pattern(
    UASensorsHaptic* sensor,
    uint32_t pattern[MAX_PATTERN_SIZE],
    uint32_t repeat)
{
    if (sensor == nullptr)
        return U_STATUS_ERROR;

    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);

    if (s->value->enabled == false)
        return U_STATUS_ERROR;

    std::vector<uint32_t> p_arg (pattern, pattern + MAX_PATTERN_SIZE);

    try
    {
        s->value->session->invoke_method_synchronously<uas::USensorD::Haptic::VibratePattern, void>(p_arg, repeat);
    }
    catch (const std::runtime_error& e)
    {
        std::cout << e.what() << std::endl;
        return U_STATUS_ERROR;
    }
    
    return U_STATUS_SUCCESS;
}
