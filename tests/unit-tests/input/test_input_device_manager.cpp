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

    std::unique_ptr<mi::InputDevice> create_device(mir::udev::Device const& dev) const override
    {
        return std::unique_ptr<mi::InputDevice>(mock_create_device(dev));
    }
    // Needs this thunk because GMock doesn't understand move semantics
    MOCK_CONST_METHOD1(mock_create_device, mi::InputDevice*(mir::udev::Device const&));
};

class InputDeviceFactoryTest : public testing::Test
{
public:
    InputDeviceFactoryTest()
        : provider_a{std::unique_ptr<NiceMock<MockInputDeviceProvider>> (new NiceMock<MockInputDeviceProvider>{})},
          provider_b{std::unique_ptr<NiceMock<MockInputDeviceProvider>> (new NiceMock<MockInputDeviceProvider>{})}
    {
        ON_CALL(*provider_a, ProbeDevice(_))
            .WillByDefault(Return(mi::InputDeviceProvider::UNSUPPORTED));
        ON_CALL(*provider_b, ProbeDevice(_))
            .WillByDefault(Return(mi::InputDeviceProvider::UNSUPPORTED));
        ON_CALL(*provider_a, mock_create_device(_))
            .WillByDefault(Return(nullptr));
        ON_CALL(*provider_b, mock_create_device(_))
            .WillByDefault(Return(nullptr));
    }

    std::unique_ptr<NiceMock<MockInputDeviceProvider>> provider_a, provider_b;
    mtd::MockUdevDevice mock_dev;
};

}

TEST_F(InputDeviceFactoryTest, ProbesAllProviders)
{
    EXPECT_CALL(*provider_a, ProbeDevice(_))
        .WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));
    EXPECT_CALL(*provider_b, ProbeDevice(_))
        .WillOnce(Return(mi::InputDeviceProvider::UNSUPPORTED));

    mi::InputDeviceFactory factory({std::move(provider_a), std::move(provider_b)});

    // TODO: What should this return?
    factory.create_device(mock_dev);
}

TEST_F(InputDeviceFactoryTest, CreatesDeviceOnSupportedProvider)
{
    auto* dummy_device = new mi::InputDevice(mock_dev);
    ON_CALL(*provider_b, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::SUPPORTED));
    EXPECT_CALL(*provider_b, mock_create_device(_))
        .WillOnce(Return(dummy_device));

    mi::InputDeviceFactory factory({std::move(provider_a), std::move(provider_b)});

    EXPECT_EQ(dummy_device, factory.create_device(mock_dev).get());
}

TEST_F(InputDeviceFactoryTest, PrefersCreatingDeviceOnBetterProvider)
{
    auto* dummy_device = new mi::InputDevice(mock_dev);
    ON_CALL(*provider_a, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::BEST));
    ON_CALL(*provider_b, ProbeDevice(_))
        .WillByDefault(Return(mi::InputDeviceProvider::SUPPORTED));
    ON_CALL(*provider_a, mock_create_device(_))
        .WillByDefault(Return(dummy_device));

    mi::InputDeviceFactory factory({std::move(provider_a), std::move(provider_b)});

    EXPECT_EQ(dummy_device, factory.create_device(mock_dev).get());
}
