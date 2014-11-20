/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/server/input/evdev_device_info.h"
#include "mir_test_framework/udev_environment.h"

#include <gtest/gtest.h>

#include <tuple>

namespace mtf = mir_test_framework;
namespace mi = mir::input;

struct EvdevDeviceInfoTest : public ::testing::TestWithParam<std::tuple<char const*, char const*, uint32_t>>
{
    mtf::UdevEnvironment env;
};

TEST_P(EvdevDeviceInfoTest, evaluates_expected_input_class)
{
    using namespace testing;
    auto const& param = GetParam();
    env.add_standard_device(std::get<0>(param));
    mi::EvdevDeviceInfo info(std::get<1>(param));
    EXPECT_THAT(info.device_classes(),Eq(std::get<2>(param)));
}

INSTANTIATE_TEST_CASE_P(InputDeviceClassDetection,
                        EvdevDeviceInfoTest,
                        ::testing::Values(
                            std::make_tuple(
                                "synaptics-touchpad",
                                "/dev/input/event12",
                                mi::InputDeviceInfo::touchpad
                                ),
                            std::make_tuple(
                                "laptop-keyboard",
                                "/dev/input/event4",
                                mi::InputDeviceInfo::keyboard
                                ),
                            std::make_tuple(
                                "usb-keyboard",
                                "/dev/input/event14",
                                mi::InputDeviceInfo::keyboard
                                ),
                            std::make_tuple(
                                "usb-mouse",
                                "/dev/input/event13",
                                mi::InputDeviceInfo::cursor
                                ),
                            std::make_tuple(
                                "bluetooth-magic-trackpad",
                                "/dev/input/event13",
                                mi::InputDeviceInfo::touchpad
                                ),
                            std::make_tuple(
                                "mt-screen-detection", // device also reports available keys..
                                "/dev/input/event4",
                                mi::InputDeviceInfo::touchscreen|mi::InputDeviceInfo::keyboard
                                ),
                            std::make_tuple(
                                "joystick-detection",
                                "/dev/input/event13",
                                mi::InputDeviceInfo::joystick|mi::InputDeviceInfo::gamepad|mi::InputDeviceInfo::keyboard
                                )
                            ));


