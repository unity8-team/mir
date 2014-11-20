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

#include "src/server/input/libinput/input_device_provider.h"

#include "mir_test_doubles/stub_input_device_info.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mili = mi::libinput;
namespace mtd = mir::test::doubles;

using LibInput = ::testing::TestWithParam<std::pair<uint32_t, mi::InputDeviceProvider::Priority>>;

TEST_P(LibInput, device_probing_yields_expected_priority)
{
    using namespace ::testing;
    mili::InputDeviceProvider provider;

    auto const& param = GetParam();

    EXPECT_THAT(provider.get_support(
            mtd::StubInputDeviceInfo(param.first)
            ), Eq(param.second));
}

INSTANTIATE_TEST_CASE_P(InputDeviceProviderTest,
                        LibInput,
                        ::testing::Values(
                            std::make_pair(
                                mi::InputDeviceInfo::touchpad,
                                mi::InputDeviceProvider::best
                                ),
                            std::make_pair(
                                mi::InputDeviceInfo::touchscreen,
                                mi::InputDeviceProvider::unsupported
                                ),
                            std::make_pair(
                                mi::InputDeviceInfo::joystick,
                                mi::InputDeviceProvider::unsupported
                                ),
                            std::make_pair(
                                mi::InputDeviceInfo::gamepad,
                                mi::InputDeviceProvider::unsupported
                                ),
                            std::make_pair(
                                mi::InputDeviceInfo::cursor,
                                mi::InputDeviceProvider::supported
                                ),
                            std::make_pair(
                                mi::InputDeviceInfo::keyboard,
                                mi::InputDeviceProvider::supported
                                )
                            ));
