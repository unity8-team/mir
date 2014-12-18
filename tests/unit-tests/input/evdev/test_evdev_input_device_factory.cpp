/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/platforms/evdev/evdev_input_device_factory.h"
#include "src/platforms/evdev/input_device_provider.h"

#include "mir/input/input_device.h"

#include <mir_test/gmock_fixes.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace mi = mir::input;
namespace mie = mi::evdev;

namespace
{

class MockInputDeviceProvider : public mie::InputDeviceProvider
{
public:
    MOCK_CONST_METHOD1(probe_device, mie::Priority(const char* node));
    MOCK_CONST_METHOD1(create_device, std::unique_ptr<mi::InputDevice>(const char* node));
};

}

TEST(EvdevInputDeviceFactory, probes_all_providers)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, probe_device(_))
	.WillOnce(Return(mie::Priority::unsupported));
    EXPECT_CALL(*b, probe_device(_))
        .WillOnce(Return(mie::Priority::unsupported));

    mie::EvdevInputDeviceFactory factory({a, b});

    EXPECT_THROW(
        {
            factory.create_device("stub_dev");
        }, std::runtime_error);
}

TEST(EvdevInputDeviceFactory, creates_device_on_supported_provider)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, probe_device(_))
	.WillOnce(Return(mie::Priority::unsupported));
    EXPECT_CALL(*b, probe_device(_))
        .WillOnce(Return(mie::Priority::supported));
    EXPECT_CALL(*b, create_device(_));
    EXPECT_CALL(*a, create_device(_))
        .Times(0);

    mie::EvdevInputDeviceFactory factory({a, b});

    factory.create_device("stub_dev");
}

TEST(EvdevInputDeviceFactory, preferes_creating_on_better_provider)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, probe_device(_))
	.Times(2)
	.WillRepeatedly(Return(mie::Priority::best));
    EXPECT_CALL(*b, probe_device(_))
	.Times(2)
        .WillRepeatedly(Return(mie::Priority::supported));
    EXPECT_CALL(*a, create_device(_))
	.Times(2);
    EXPECT_CALL(*b, create_device(_))
        .Times(0);

    mie::EvdevInputDeviceFactory factory_one({a, b});
    mie::EvdevInputDeviceFactory factory_two({b, a});

    factory_one.create_device("stub_dev1");
    factory_two.create_device("stub_dev2");
}

