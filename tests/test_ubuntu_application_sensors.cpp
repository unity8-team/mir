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

#include <core/testing/fork_and_run.h>

#include "gtest/gtest.h"

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

using namespace std;

/*****
 * Test definition macro which runs a TEST_F in a forked process.
 * We need to do this as we cannot unload the platform-api dynloaded backend
 * shlib, nor reset the sensor state. Note that you can only use EXPECT_*, not
 * ASSERT_*!
 * 
 * Usage:
 * TEST_FP(FixtureName, TestName, {
 *    ... test code ...
 *    EXPECT_* ...
 *  })
 */
#define TEST_FP(test_fixture, test_name, CODE)                              \
    TEST_F(test_fixture, test_name) {                                       \
        auto noop = [](){ return core::posix::exit::Status::success; };     \
        auto test = [&]() {                                                 \
            CODE                                                            \
            return HasFailure() ? core::posix::exit::Status::failure        \
                                : core::posix::exit::Status::success;       \
        };                                                                  \
        auto result = core::testing::fork_and_run(noop, test);              \
        EXPECT_EQ(core::testing::ForkAndRunResult::empty,                   \
                  result & core::testing::ForkAndRunResult::client_failed); \
}

class APITest : public testing::Test
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
        setenv("UBUNTU_PLATFORM_API_BACKEND", "libubuntu_application_test_api.so", 1);
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
TEST_FP(APITest, NoData, {
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());
})

TEST_FP(APITest, CreateProximity, {
    set_data("create proximity");
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());

    UASensorsProximity *s = ua_sensors_proximity_new();
    EXPECT_TRUE(s != NULL);
})

TEST_FP(APITest, CreateAccelerator, {
    set_data("create accel 0.5 1000 0.1");
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());

    UASensorsAccelerometer *s = ua_sensors_accelerometer_new();
    EXPECT_TRUE(s != NULL);
    EXPECT_FLOAT_EQ(0.5, ua_sensors_accelerometer_get_min_value(s));
    EXPECT_FLOAT_EQ(1000.0, ua_sensors_accelerometer_get_max_value(s));
    EXPECT_FLOAT_EQ(0.1, ua_sensors_accelerometer_get_resolution(s));
})

TEST_FP(APITest, CreateLight, {
    set_data("create light 0 10 0.5");
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());

    UASensorsLight *s = ua_sensors_light_new();
    EXPECT_TRUE(s != NULL);
    EXPECT_FLOAT_EQ(0.0, ua_sensors_light_get_min_value(s));
    EXPECT_FLOAT_EQ(10.0, ua_sensors_light_get_max_value(s));
    EXPECT_FLOAT_EQ(0.5, ua_sensors_light_get_resolution(s));
})
