/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_SENSORS_SENSOR_SERVICE_H_
#define UBUNTU_APPLICATION_SENSORS_SENSOR_SERVICE_H_

#include "ubuntu/platform/shared_ptr.h"

#include "ubuntu/application/sensors/sensor.h"

namespace ubuntu
{
namespace application
{
namespace sensors
{
/** Local stub that allows for instantiating sensor instances given a type. */
class SensorService : public ubuntu::platform::ReferenceCountedBase
{
public:
    /** Returns a sensor instance for the provided type or NULL. */
    static Sensor::Ptr sensor_for_type(SensorType type);
protected:
    SensorService() {}
    virtual ~SensorService() {}

    SensorService(const SensorService&) = delete;
    SensorService& operator=(const SensorService&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_SENSORS_SENSOR_SERVICE_H_
