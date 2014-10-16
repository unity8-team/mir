/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mir/graphics/display_configuration.h"

#include <screen.h>

namespace mg = mir::graphics;
namespace geom = mir::geometry;

mg::DisplayConfigurationOutput const fake_output
{
    mg::DisplayConfigurationOutputId{3},
    mg::DisplayConfigurationCardId{2},
    mg::DisplayConfigurationOutputType::dvid,
    {
        mir_pixel_format_abgr_8888
    },
    {
        {geom::Size{10, 20}, 60.0},
        {geom::Size{10, 20}, 59.0},
        {geom::Size{15, 20}, 59.0}
    },
    0,
    geom::Size{10, 20},
    true,
    true,
    geom::Point(),
    2,
    mir_pixel_format_abgr_8888,
    mir_power_mode_on,
    mir_orientation_normal
};

TEST(ScreenTest, OrientationSensor)
{
    Screen::skipDBusRegistration = true;
    Screen *screen = new Screen(fake_output);

    // Default state should be active
    ASSERT_TRUE(screen->orientationSensorEnabled());

    screen->onDisplayPowerStateChanged(0,0);
    ASSERT_FALSE(screen->orientationSensorEnabled());

    screen->onDisplayPowerStateChanged(1,0);
    ASSERT_TRUE(screen->orientationSensorEnabled());
}
