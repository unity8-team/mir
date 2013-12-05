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

#include "gtest/gtest.h"

#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

using namespace std;

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
    }

    char data_file[100];
    int data_fd;
};

TEST_F(APITest, NoData) {
    // without any data, there are no sensors defined
    EXPECT_EQ(NULL, ua_sensors_accelerometer_new());
    EXPECT_EQ(NULL, ua_sensors_proximity_new());
    EXPECT_EQ(NULL, ua_sensors_light_new());
}
