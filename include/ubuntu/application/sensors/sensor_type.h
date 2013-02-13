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
#ifndef UBUNTU_APPLICATION_SENSORS_SENSOR_TYPE_H_
#define UBUNTU_APPLICATION_SENSORS_SENSOR_TYPE_H_

namespace ubuntu
{
namespace application
{
namespace sensors
{
/** Models a special type of a sensor */
enum SensorType
{
    first_defined_sensor_type = 0,
    sensor_type_accelerometer = first_defined_sensor_type, ///< An accelerometer
    sensor_type_magnetic_field, ///< A magnetic field sensor, i.e., a compass
    sensor_type_gyroscope, ///< A gyroscope
    sensor_type_light, ///< An ambient light sensor
    sensor_type_proximity, ///< A proximity sensor, used to blank the screen when making a call
    sensor_type_orientation, ///< Virtual sensor, reports sensor fusion results regarding a device's orientation
    sensor_type_linear_acceleration, ///< Virtual sensor, reports sensor fusion results regarding a device's linear acceleration
    sensor_type_rotation_vector, ///< Virtual sensor, reports sensor fusion results regarding a device's rotation vector
    undefined_sensor_type
};
}
}
}

#endif // UBUNTU_APPLICATION_SENSORS_SENSOR_TYPE_H_
