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

#include "src/server/input/android/input_device_provider.h"

#include "mir/udev/wrapper.h"

#include <umockdev.h>
#include "mir_test_framework/udev_environment.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mtd = mir_test_framework;

namespace
{

struct DeviceStub : public mir::udev::Device
{
    char const* node;
    char const* path;
    DeviceStub(DeviceStub const& other)
        : mir::udev::Device(),
          node(other.node), path(other.path)
    {}
    DeviceStub(char const* node, char const* path)
        : node(node), path(path)
    {
    }

    char const* subsystem() const override { return "input"; }
    char const* devtype() const { return "evdev"; }
    char const* devpath() const { return path; }
    char const* devnode() const { return node; }
};

struct MockedDevice
{
    std::string recorded_device_sample;
    DeviceStub device_info;
    mi::InputDeviceProvider::Priority expected_priority;
};

}

std::ostream& operator<<(std::ostream& out, MockedDevice const& dev)
{
    return out << dev.recorded_device_sample;
}

struct AndroidInputDeviceProviderTest : public ::testing::TestWithParam<MockedDevice>
{
    mia::InputDeviceProvider provider;
    mtd::UdevEnvironment env;
};

TEST_P(AndroidInputDeviceProviderTest, device_probing_yields_expected_priority)
{
    using namespace ::testing;
    auto const& param = GetParam();
    env.add_standard_device(param.recorded_device_sample);
    EXPECT_THAT(provider.probe_device(param.device_info), Eq(param.expected_priority));
}

INSTANTIATE_TEST_CASE_P(DeviceTypes,
                        AndroidInputDeviceProviderTest,
                        ::testing::Values(
                            MockedDevice{
                                "touchpad-detection",
                                {"input/event5", "/dev/input/event5"},
                                mi::InputDeviceProvider::unsupported
                                },
                            MockedDevice{
                                "mt-screen-detection",
                                {"input/event4", "/dev/input/event4"},
                                mi::InputDeviceProvider::best
                                },
                            MockedDevice{
                                "joystick-detection",
                                {"input/event13", "/dev/input/event13"},
                                mi::InputDeviceProvider::supported
                                },
                            MockedDevice{
                                "keyboard-detection",
                                {"input/event3", "/dev/input/event3"},
                                mi::InputDeviceProvider::supported
                                }
                                ));

