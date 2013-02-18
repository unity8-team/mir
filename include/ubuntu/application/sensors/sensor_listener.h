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
#ifndef UBUNTU_APPLICATION_SENSORS_SENSOR_LISTENER_H_
#define UBUNTU_APPLICATION_SENSORS_SENSOR_LISTENER_H_

#include "ubuntu/platform/shared_ptr.h"

#include "ubuntu/application/sensors/sensor_reading.h"

namespace ubuntu
{
namespace application
{
namespace sensors
{
/** Extension point to allow for observing a sensor's readings. */
class SensorListener : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<SensorListener> Ptr;

    /** Invoked whenever a new reading is available from the sensor.
     * \param [in] reading The new reading.
     */
    virtual void on_new_reading(const SensorReading::Ptr& reading) = 0;

protected:
    SensorListener() {}
    virtual ~SensorListener() {}

    SensorListener(const SensorListener&) = delete;
    SensorListener& operator=(const SensorListener&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_SENSORS_SENSOR_LISTENER_H_
