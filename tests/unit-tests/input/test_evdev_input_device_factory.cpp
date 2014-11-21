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

#include "src/server/input/evdev_input_device_factory.h"
#include "src/server/input/input_device_info.h"
#include "src/server/input/input_device_provider.h"

#include "mir/input/input_device.h"

#include "mir_test_doubles/stub_input_device_info.h"

#include <mir_test/gmock_fixes.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mtd = mir::test::doubles;

namespace
{

class MockInputDeviceProvider : public mi::InputDeviceProvider
{
public:
    MOCK_CONST_METHOD1(get_support, mi::InputDeviceProvider::Priority(mi::InputDeviceInfo const&));
    MOCK_CONST_METHOD1(create_device, std::unique_ptr<mi::InputDevice>(mi::InputDeviceInfo const&));
};

}

TEST(EvdevInputDeviceFactory, probes_all_providers)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, get_support(_))
	.WillOnce(Return(mi::InputDeviceProvider::unsupported));
    EXPECT_CALL(*b, get_support(_))
        .WillOnce(Return(mi::InputDeviceProvider::unsupported));

    mi::EvdevInputDeviceFactory factory({a, b});
    mtd::StubInputDeviceInfo stub_dev;

    factory.create_device(stub_dev);
}

TEST(EvdevInputDeviceFactory, creates_device_on_supported_provider)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, get_support(_))
	.WillOnce(Return(mi::InputDeviceProvider::unsupported));
    EXPECT_CALL(*b, get_support(_))
        .WillOnce(Return(mi::InputDeviceProvider::supported));
    EXPECT_CALL(*b, create_device(_));

    mi::EvdevInputDeviceFactory factory({a, b});
    mtd::StubInputDeviceInfo stub_dev;

    factory.create_device(stub_dev);
}

TEST(EvdevInputDeviceFactory, preferes_creating_on_better_provider)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, get_support(_))
	.Times(2)
	.WillRepeatedly(Return(mi::InputDeviceProvider::best));
    EXPECT_CALL(*b, get_support(_))
	.Times(2)
        .WillRepeatedly(Return(mi::InputDeviceProvider::supported));
    EXPECT_CALL(*a, create_device(_))
	.Times(2);

    mi::EvdevInputDeviceFactory factory_one({a, b});
    mi::EvdevInputDeviceFactory factory_two({b, a});
    mtd::StubInputDeviceInfo stub_dev;

    factory_one.create_device(stub_dev);
    factory_two.create_device(stub_dev);
}

