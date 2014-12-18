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

#include "src/platforms/evdev/libinput_device_provider.h"
#include "mir_test_framework/udev_environment.h"
#include "mir_test_doubles/mock_libinput.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mie = mi::evdev;
namespace mtf = mir_test_framework;

struct LibInput : public ::testing::TestWithParam<std::tuple<char const*, char const*, mie::Priority>>
{
    mtf::UdevEnvironment env;
    mir::test::doubles::MockLibInput input;
};

TEST_P(LibInput, device_probing_yields_expected_priority)
{
    using namespace testing;
    auto const& param = GetParam();
    env.add_standard_device(std::get<0>(param));
    mie::LibInputDeviceProvider provider;

    EXPECT_THAT(provider.probe_device(std::get<1>(param)), Eq(std::get<2>(param)));
}

INSTANTIATE_TEST_CASE_P(InputDeviceProviderTest,
                        LibInput,
                        ::testing::Values(
                            std::make_tuple(
                                "synaptics-touchpad",
                                "/dev/input/event12",
                                mie::Priority::best
                                ),
                            std::make_tuple(
                                "bluetooth-magic-trackpad",
                                "/dev/input/event13",
                                mie::Priority::best
                                ),
                            std::make_tuple(
                                "mt-screen-detection",
                                "/dev/input/event4",
                                mie::Priority::unsupported
                                ),
                            std::make_tuple(
                                "joystick-detection",
                                "/dev/input/event13",
                                mie::Priority::unsupported
                                ),
                            std::make_tuple(
                                "usb-mouse",
                                "/dev/input/event13",
                                mie::Priority::supported
                                ),
                            std::make_tuple(
                                "usb-keyboard",
                                "/dev/input/event14",
                                mie::Priority::supported
                                ),
                            std::make_tuple(
                                "laptop-keyboard",
                                "/dev/input/event4",
                                mie::Priority::supported
                                )
                            ));
