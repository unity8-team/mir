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
 * Tests with simulated sensor backend
 *
 *******************************************/

struct event {
    uint64_t timestamp;
    float x, y, z;
    UASProximityDistance distance;
    void* context;
};
queue<struct event> events;


class SimBackendTest : public testing::Test
{
  protected:
    virtual void SetUp()
    {
        snprintf(data_file, sizeof(data_file), "%s", "/tmp/sensor_test.XXXXXX");
        data_fd = mkstemp(data_file);
        if (data_fd < 0) {
            perror("mkstemp");
            abort();
        }
        setenv("UBUNTU_PLATFORM_API_SENSOR_TEST", data_file, 1);
        setenv("UBUNTU_PLATFORM_API_BACKEND", "test", 1);

        // ensure the queue is clear
        while (events.size() > 0)
            events.pop();
    }

    virtual void TearDown()
    {
        unlink(data_file);
    }

    void set_data(const char* data)
    {
        write(data_fd, data, strlen(data));
        fsync(data_fd);
    }

    char data_file[100];
    int data_fd;
};

// without any data, there are no sensors defined
TESTP_F(SimBackendTest, NoData, {
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());
})

TESTP_F(SimBackendTest, CreateProximity, {
    set_data("create proximity");
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());

    UASensorsProximity *s = ua_sensors_proximity_new();
    EXPECT_TRUE(s != NULL);
})

TESTP_F(SimBackendTest, CreateAccelerometer, {
    set_data("create accel 0.5 1000 0.1");
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());

    UASensorsAccelerometer *s = ua_sensors_accelerometer_new();
    EXPECT_TRUE(s != NULL);

    float min = 0.f; ua_sensors_accelerometer_get_min_value(s, &min);
    float max = 0.f; ua_sensors_accelerometer_get_max_value(s, &max);
    float res = 0.f; ua_sensors_accelerometer_get_resolution(s, &res);

    EXPECT_FLOAT_EQ(0.5, min);
    EXPECT_FLOAT_EQ(1000.0, max);
    EXPECT_FLOAT_EQ(0.1, res);
})

TESTP_F(SimBackendTest, CreateLight, {
    set_data("create light 0 10 0.5");
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());

    UASensorsLight *s = ua_sensors_light_new();
    EXPECT_TRUE(s != NULL);

    float min = 0.f; ua_sensors_light_get_min_value(s, &min);
    float max = 0.f; ua_sensors_light_get_max_value(s, &max);
    float res = 0.f; ua_sensors_light_get_resolution(s, &res);

    EXPECT_FLOAT_EQ(0.0, min);
    EXPECT_FLOAT_EQ(10.0, max);
    EXPECT_FLOAT_EQ(0.5, res);
})

TESTP_F(SimBackendTest, ProximityEvents, {
    set_data("create proximity\n"
             "  # some comment\n"
             "  \n"
             "50 proximity near\n"
             "100 proximity far\n"
             "80 proximity unknown\n"
    );

    UASensorsProximity *s = ua_sensors_proximity_new();
    EXPECT_TRUE(s != NULL);
    ua_sensors_proximity_enable(s);
    auto start_time = chrono::system_clock::now();

    ua_sensors_proximity_set_reading_cb(s,
        [](UASProximityEvent* ev, void* ctx) {
            events.push({uas_proximity_event_get_timestamp(ev),
                         .0, .0, .0,
                         uas_proximity_event_get_distance(ev),
                         ctx});
        }, NULL);

    usleep(350000);
    EXPECT_EQ(3, events.size());

    auto e = events.front();
    events.pop();
    EXPECT_EQ(e.distance, U_PROXIMITY_NEAR);
    EXPECT_EQ(NULL, e.context);
    auto event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    auto delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_GE(delay, 30);
    EXPECT_LE(delay, 70);

    e = events.front();
    events.pop();
    EXPECT_EQ(e.distance, U_PROXIMITY_FAR);
    event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_GE(delay, 130);
    EXPECT_LE(delay, 170);

    e = events.front();
    events.pop();
    EXPECT_EQ(e.distance, (UASProximityDistance) 0);
    event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_GE(delay, 210);
    EXPECT_LE(delay, 250);
})

TESTP_F(SimBackendTest, LightEvents, {
    set_data(" create  light  0 10 1\n"
             "1 light 5\n"
             "100 light 8\n"
    );

    UASensorsLight *s = ua_sensors_light_new();
    EXPECT_TRUE(s != NULL);
    ua_sensors_light_enable(s);
    auto start_time = chrono::system_clock::now();

    ua_sensors_light_set_reading_cb(s,
        [](UASLightEvent* ev, void* ctx) {
            float light = -1.f;
             uas_light_event_get_light(ev, &light);
            events.push({uas_light_event_get_timestamp(ev),
                         light, .0, .0,
                         (UASProximityDistance) 0, ctx});
        }, NULL);

    usleep(130000);
    EXPECT_EQ(2, events.size());

    auto e = events.front();
    events.pop();
    EXPECT_FLOAT_EQ(e.x, 5);
    EXPECT_EQ(NULL, e.context);
    auto event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    auto delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_LE(delay, 10);

    e = events.front();
    events.pop();
    EXPECT_FLOAT_EQ(e.x, 8);
    event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_GE(delay, 91);
    EXPECT_LE(delay, 111);
})

TESTP_F(SimBackendTest, AccelEvents, {
    // cover the case of > 1 s, to ensure that we correctly do mod arithmetic
    set_data("create accel -1000 1000 0.1\n"
             "1100 accel 5.5 -8.5 9.9\n"
    );

    UASensorsAccelerometer *s = ua_sensors_accelerometer_new();
    EXPECT_TRUE(s != NULL);
    ua_sensors_accelerometer_enable(s);
    auto start_time = chrono::system_clock::now();

    ua_sensors_accelerometer_set_reading_cb(s,
        [](UASAccelerometerEvent* ev, void* ctx) {
            float x; uas_accelerometer_event_get_acceleration_x(ev, &x);
            float y; uas_accelerometer_event_get_acceleration_y(ev, &y);
            float z; uas_accelerometer_event_get_acceleration_z(ev, &z);

            events.push({uas_accelerometer_event_get_timestamp(ev),
                         x,
                         y,
                         z,
                         (UASProximityDistance) 0, ctx});
        }, NULL);

    usleep(1200000);
    EXPECT_EQ(1, events.size());

    auto e = events.front();
    events.pop();
    EXPECT_FLOAT_EQ(e.x, 5.5);
    EXPECT_FLOAT_EQ(e.y, -8.5);
    EXPECT_FLOAT_EQ(e.z, 9.9);
    EXPECT_EQ(NULL, e.context);
    auto event_time = time_point_system_ns(std::chrono::nanoseconds(e.timestamp));
    auto delay = chrono::duration_cast<chrono::milliseconds>(event_time - start_time).count();
    EXPECT_GE(delay, 1050);
    EXPECT_LE(delay, 1150);
})
