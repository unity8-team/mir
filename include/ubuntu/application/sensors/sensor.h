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
#ifndef UBUNTU_APPLICATION_SENSORS_SENSOR_H_
#define UBUNTU_APPLICATION_SENSORS_SENSOR_H_

#include "ubuntu/platform/shared_ptr.h"

#include "ubuntu/application/sensors/sensor_listener.h"
#include "ubuntu/application/sensors/sensor_type.h"

#include <cstdint>

namespace ubuntu
{
namespace application
{
namespace sensors
{
/** A data acquisition device that samples a device's environment. */
class Sensor : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Sensor> Ptr;

    /** Accesses the name of the sensor. Returned pointer is owned by the implementation. */
    virtual const char* name() = 0;

    /** Accesses the vendor of the sensor. Returned pointer is owned by the implementation. */
    virtual const char* vendor() = 0;

    /** Accesses the numeric id of the sensor. Should be as stable as possible. */
    virtual int32_t id() = 0;

    /** Registers the provided listener with the sensor and integrates it with the reporting of sensor readings. */
    virtual void register_listener(const SensorListener::Ptr& listener) = 0;

    /** Enables the sensor and starts the data acquisition. */
    virtual void enable() = 0;
    /** Disables the sensor and stops the data acquisition. */
    virtual void disable() = 0;

    virtual SensorType type() = 0;

    /** Returns the minimum numeric value of a sensor reading. */
    virtual float min_value() = 0;
    /** Returns the maximum numeric value of a sensor reading. */
    virtual float max_value() = 0;
    /** Returns the numerical resolution of a sensor reading. */
    virtual float resolution() = 0;

    /** Returns an estimate of the sensor's power consumption, normalized to [0,1]. */
    virtual float power_consumption() = 0;

    /** Returns the minimum delay between two consecutive sensor readings. */
    virtual int32_t min_delay() = 0;

protected:
    Sensor() {}
    virtual ~Sensor() {}

    Sensor(const Sensor&) = delete;
    Sensor& operator=(const Sensor&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_SENSORS_SENSOR_H_
