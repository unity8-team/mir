/*
 * Copyright © 2013 Canonical Ltd.
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
 */

#include <mir/input/input_device_factory.h>
#include <mir/udev_wrapper.h>
#include "mir_test_doubles/mock_udev_device.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mi = mir::input;
namespace mtd = mir::test::doubles;

namespace 
{

class MockInputDeviceProvider : public mi::InputDeviceProvider
{
public:
    MOCK_CONST_METHOD1(ProbeDevice, mi::InputDeviceProvider::Priority(mir::UdevDevice const&));
};

}

TEST(InputDeviceFactoryTest, ProbesAllProviders)
{
    using namespace testing;
    auto a = std::make_shared<MockInputDeviceProvider>();
    auto b = std::make_shared<MockInputDeviceProvider>();

    EXPECT_CALL(*a, ProbeDevice(_))
	.WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));
    EXPECT_CALL(*b, ProbeDevice(_))
        .WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));

    mi::InputDeviceFactory factory({a, b});
    mtd::MockUdevDevice mock_dev;

    factory.create_device(mock_dev);
}
