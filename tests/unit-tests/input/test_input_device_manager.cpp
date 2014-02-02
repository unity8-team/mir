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

#include "mir/input/input_device_factory.h"
#include "mir/udev/wrapper.h"
#include "mir_test_doubles/mock_udev_device.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mi = mir::input;
namespace mtd = mir::test::doubles;

using namespace testing;

namespace
{

class MockInputDeviceProvider : public mi::InputDeviceProvider
{
public:
    MOCK_CONST_METHOD1(ProbeDevice, mi::InputDeviceProvider::Priority(mir::udev::Device const&));
    MOCK_CONST_METHOD1(create_device, std::shared_ptr<mi::InputDevice>(mir::udev::Device const&));
};

class InputDeviceFactoryTest : public testing::Test
{
public:
    InputDeviceFactoryTest()
        : provider_a{std::make_shared<NiceMock<MockInputDeviceProvider>> ()},
          provider_b{std::make_shared<NiceMock<MockInputDeviceProvider>> ()},
          dummy_device{std::make_shared<mi::InputDevice> (mock_dev)}
    {
        ON_CALL(*provider_a, ProbeDevice(_))
            .WillByDefault(Return(mi::InputDeviceProvider::UNSUPPORTED));
        ON_CALL(*provider_b, ProbeDevice(_))
            .WillByDefault(Return(mi::InputDeviceProvider::UNSUPPORTED));
        ON_CALL(*provider_a, create_device(_))
            .WillByDefault(Return(std::shared_ptr<mi::InputDevice>()));
        ON_CALL(*provider_b, create_device(_))
            .WillByDefault(Return(std::shared_ptr<mi::InputDevice>()));
    }

    std::shared_ptr<NiceMock<MockInputDeviceProvider>> provider_a, provider_b;
    mtd::MockUdevDevice mock_dev;
    std::shared_ptr<mi::InputDevice> dummy_device;
};

}

TEST_F(InputDeviceFactoryTest, ProbesAllProviders)
{
    EXPECT_CALL(*provider_a, ProbeDevice(_))
        .WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));
    EXPECT_CALL(*provider_b, ProbeDevice(_))
        .WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));

    mi::InputDeviceFactory factory({provider_a, provider_b});

    // TODO: What should this return?
    factory.create_device(mock_dev);
}

TEST_F(InputDeviceFactoryTest, CreatesDeviceOnSupportedProvider)
{
    ON_CALL(*provider_b, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::SUPPORTED));
    EXPECT_CALL(*provider_b, create_device(_))
        .WillOnce(Return(dummy_device));

    mi::InputDeviceFactory factory({provider_a, provider_b});

    EXPECT_EQ(dummy_device, factory.create_device(mock_dev));
}

TEST_F(InputDeviceFactoryTest, PrefersCreatingDeviceOnBetterProvider)
{
    ON_CALL(*provider_a, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::BEST));
    ON_CALL(*provider_b, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::SUPPORTED));
    ON_CALL(*provider_a, create_device(_))
        .WillByDefault(Return(dummy_device));

    mi::InputDeviceFactory factory_one({provider_a, provider_b});
    mi::InputDeviceFactory factory_two({provider_b, provider_a});

    EXPECT_EQ(dummy_device, factory_one.create_device(mock_dev));
    EXPECT_EQ(dummy_device, factory_two.create_device(mock_dev));
}
