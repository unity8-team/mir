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

    auto stub_service = dbus::Service::use_service(bus, dbus::traits::Service<uas::HapticService>::interface_name());
    auto stub = stub_service->object_for_path(dbus::types::ObjectPath("/com/canonical/usensord/haptic"));

    return make_holder(new UbuntuApplicationSensorsHaptic(stub));
}

UStatus
ua_sensors_haptic_vibrate_once(
    UASensorsHaptic* sensor,
    uint32_t duration)
{
    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);

    s->value->session->invoke_method_synchronously<uas::HapticService::Vibrate, void>(duration);
}

UStatus
ua_sensors_haptic_vibrate_with_pattern(
    UASensorsHaptic* sensor,
    uint32_t pattern[MAX_PATTERN_SIZE],
    uint32_t repeat)
{
    auto s = static_cast<Holder<UbuntuApplicationSensorsHaptic*>*>(sensor);

    std::vector<uint32_t> args (pattern, pattern + sizeof(pattern) / sizeof(uint32_t) );

    //s->value->session->invoke_method_synchronously<uas::HapticService::Vibrate, void>(args, repeat);
    s->value->session->invoke_method_synchronously<uas::HapticService::VibratePattern, void>(args);
}
