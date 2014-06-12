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
 * Authored by: Martin Pitt <martin.pitti@ubuntu.com>
 */

#include <cstdlib>
#include <cstdio>
#include <queue>
#include <chrono>
#include <iostream>

#include <core/testing/fork_and_run.h>

#include "gtest/gtest.h"

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/event/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/event/proximity.h>
#include <ubuntu/application/sensors/light.h>
#include <ubuntu/application/sensors/event/light.h>

using namespace std;

typedef chrono::time_point<chrono::system_clock,chrono::nanoseconds> time_point_system_ns;

/*******************************************
 *
 * Tests with default backend
 *
 *******************************************/

class DefaultBackendTest : public testing::Test
{
    virtual void SetUp()
    {
    }
};

TESTP_F(DefaultBackendTest, CreateProximity, {
    // this can succeed or fail depending on whether the hardware we run this
    // on actually exists; but it should never crash
    UASensorsProximity *s = ua_sensors_proximity_new();
    if (s != NULL) {
        cerr << "proximity sensor present on this hardware\n";
        // calling its functions should not crash; we can't assert much about
        // their actual values, though
        ua_sensors_proximity_enable(s);
        float min; ua_sensors_proximity_get_min_value(s, &min);
        float max; ua_sensors_proximity_get_max_value(s, &max);

        EXPECT_LE(min, max);

        float res; ua_sensors_proximity_get_resolution(s, &res);
        ua_sensors_proximity_disable(s);
    } else {
        cerr << "no proximity sensor on this hardware\n";
    }
})

TESTP_F(DefaultBackendTest, CreateAccelerometer, {
    // this can succeed or fail depending on whether the hardware we run this
    // on actually exists; but it should never crash
    UASensorsAccelerometer *s = ua_sensors_accelerometer_new();
    if (s != NULL) {
        cerr << "accelerometer sensor present on this hardware\n";
        // calling its functions should not crash; we can't assert much about
        // their actual values, though
        ua_sensors_accelerometer_enable(s);
        float min; ua_sensors_accelerometer_get_min_value(s, &min);
        float max; ua_sensors_accelerometer_get_max_value(s, &max);

        EXPECT_LE(min, max);

        float res; ua_sensors_accelerometer_get_resolution(s, &res);
        ua_sensors_accelerometer_disable(s);
    } else {
        cerr << "no accelerometer sensor on this hardware\n";
    }
})

TESTP_F(DefaultBackendTest, CreateLight, {
    // this can succeed or fail depending on whether the hardware we run this
    // on actually exists; but it should never crash
    UASensorsLight *s = ua_sensors_light_new();
    if (s != NULL) {
        cerr << "light sensor present on this hardware\n";
        // calling its functions should not crash; we can't assert much about
        // their actual values, though
        ua_sensors_light_enable(s);
        float min; ua_sensors_light_get_min_value(s, &min);
        float max; ua_sensors_light_get_max_value(s, &max);

        EXPECT_LE(min, max);

        float res; ua_sensors_light_get_resolution(s, &res);
        ua_sensors_light_disable(s);
    } else {
        cerr << "no light sensor on this hardware\n";
    }
})
