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
#ifndef UBUNTU_APPLICATION_SENSORS_SENSOR_READING_H_
#define UBUNTU_APPLICATION_SENSORS_SENSOR_READING_H_

#include "ubuntu/platform/shared_ptr.h"

#include <cstddef>
#include <cstdint>

namespace ubuntu
{
namespace application
{
namespace sensors
{

/** A vector of static size. */
template<size_t size, typename NumericType = float>
struct Vector
{
    NumericType v[size];

    /** Accesses the element at index index. */
    NumericType& operator[](size_t index)
    {
        return v[index];
    }

    /** Accesses the element at index index. */
    const NumericType& operator[](size_t index) const
    {
        return v[index];
    }
};

/** A timestamped reading from a sensor. */
struct SensorReading : public ubuntu::platform::ReferenceCountedBase
{
    typedef ubuntu::platform::shared_ptr<SensorReading> Ptr;

    SensorReading() : timestamp(-1)
    {
    }

    int64_t timestamp; ///< The timestamp of the reading in [ns], CLOCK_MONOTONIC.
    /** A union of different possible sensor readings. */
    union
    {
        Vector<3> vector; ///< Arbitrary vector, orientation and linear acceleration readings are reported here.
        Vector<3> acceleration; ///< Acceleration vector containing acceleration readings for the three axis.
        Vector<3> magnetic; ///< Readings from magnetometer, in three dimensions.
        float temperature; ///< Ambient temperature.
        float distance; ///< Discrete distance, everything > 5 is considered far, everything < 5 is considered near.
        float light; ///< Ambient light conditions.
        float pressure; ///< Ambient pressure.
    };
};
}
}
}

#endif // UBUNTU_APPLICATION_SENSORS_SENSOR_READING_H_
