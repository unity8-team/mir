/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/server/input/default_device_settings.h"

#include <boost/variant/get.hpp>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace mi = mir::input;
using namespace ::testing;

TEST(DefaultDeviceSettings, rejects_unkown_settings)
{
    mi::DefaultDeviceSettings settings;
    EXPECT_THROW(
        {settings.set(mi::DeviceSettings::primary_button, 2);},
        std::invalid_argument);

    EXPECT_TRUE(settings.get(mi::DeviceSettings::primary_button).type() ==
                typeid(mir::input::DeviceSettings::NotApplicable));
}

TEST(DefaultDeviceSettings, accepts_known_settings)
{
    mi::DefaultDeviceSettings settings;
    settings.add_setting(mi::DeviceSettings::primary_button, 2);

    EXPECT_THAT(boost::get<int>(settings.get(mi::DeviceSettings::primary_button)), Eq(2));
    EXPECT_NO_THROW(
        {
            settings.set(mi::DeviceSettings::primary_button, mi::DeviceSettings::Value{1});
        });
    EXPECT_THAT(boost::get<int>(settings.get(mi::DeviceSettings::primary_button)), Eq(1));
}

TEST(DefaultDeviceSettings, rejects_wrong_value_type)
{
    mi::DefaultDeviceSettings settings;
    settings.add_setting(mi::DeviceSettings::primary_button, 1);
    EXPECT_THROW(
        {
            settings.set(mi::DeviceSettings::primary_button, 10.0);
        },
        std::invalid_argument);
}

TEST(DefaultDeviceSettings, executes_callback_on_each_setting)
{
    mi::DefaultDeviceSettings settings;
    int times_called = 0;
    int called_for_primary_button = 0;
    int called_for_calibration = 0;
    settings.add_setting(mi::DeviceSettings::primary_button, 1);
    settings.add_setting(mi::DeviceSettings::primary_button, 2);
    settings.add_setting(mi::DeviceSettings::coordinate_calibration,
                         glm::mat3x3{1.0f,0.0f,0.0f,
                                     0.0f,1.0f,0.0f,
                                     0.0f,0.0f,1.0f}
                        );
    settings.for_each_setting([&](mi::DeviceSettings::Setting setting, mi::DeviceSettings::Value const&)
                              {
                                 ++times_called;
                                 if (setting == mi::DeviceSettings::primary_button)
                                        ++called_for_primary_button;
                                 if (setting == mi::DeviceSettings::coordinate_calibration)
                                        ++called_for_calibration;
                              });

    EXPECT_THAT(times_called, Eq(2));
    EXPECT_THAT(called_for_calibration, Eq(1));
    EXPECT_THAT(called_for_primary_button, Eq(1));
}
