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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/server/graphics/android/android_display_factory.h"
#include "src/server/graphics/android/hwc_factory.h"
#include "src/server/graphics/android/android_display_allocator.h"

#include "mir_test/hw_mock.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdexcept>

namespace mt=mir::test;
namespace mga=mir::graphics::android;

namespace
{
struct MockHWCFactory: public mga::HWCFactory
{
    MockHWCFactory()
    {
        using namespace testing;
        ON_CALL(*this, create_hwc_1_1(_))
            .WillByDefault(Return(std::shared_ptr<mga::HWCDevice>()));
    }
    MOCK_CONST_METHOD1(create_hwc_1_1, std::shared_ptr<mga::HWCDevice>(std::shared_ptr<hwc_composer_device_1> const&));
};

struct MockDisplayAllocator: public mga::DisplayAllocator
{
    MockDisplayAllocator()
    {
        using namespace testing;
        ON_CALL(*this, create_gpu_display())
            .WillByDefault(Return(std::shared_ptr<mga::AndroidDisplay>()));
        ON_CALL(*this, create_hwc_display(_))
            .WillByDefault(Return(std::shared_ptr<mga::HWCDisplay>()));

    }
    MOCK_CONST_METHOD0(create_gpu_display, std::shared_ptr<mga::AndroidDisplay>());
    MOCK_CONST_METHOD1(create_hwc_display, std::shared_ptr<mga::HWCDisplay>(std::shared_ptr<mga::HWCDevice> const&));
};

class AndroidDisplayFactoryTest : public ::testing::Test
{
public:
    AndroidDisplayFactoryTest()
        : mock_display_allocator(std::make_shared<MockDisplayAllocator>()),
          mock_hwc_factory(std::make_shared<MockHWCFactory>())
    {
    }

    void TearDown()
    {
        EXPECT_TRUE(hw_access_mock.open_count_matches_close());
    }

    std::shared_ptr<MockDisplayAllocator> const mock_display_allocator;
    std::shared_ptr<MockHWCFactory> const mock_hwc_factory;
    mt::HardwareAccessMock hw_access_mock;
};
}

TEST_F(AndroidDisplayFactoryTest, hwc_selection_gets_hwc_device)
{
    using namespace testing;

    EXPECT_CALL(hw_access_mock, hw_get_module(StrEq(HWC_HARDWARE_MODULE_ID), _))
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory); 
}

/* this case occurs when the system cannot find the hwc library. it is a nonfatal error because we have a backup to try */
TEST_F(AndroidDisplayFactoryTest, hwc_module_unavailble_always_creates_gpu_display)
{
    using namespace testing;

    EXPECT_CALL(hw_access_mock, hw_get_module(StrEq(HWC_HARDWARE_MODULE_ID), _))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_CALL(*mock_hwc_factory, create_hwc_1_1(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_hwc_display(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_gpu_display())
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory); 
    display_factory.create_display();
}

/* this case occurs when the hwc library doesn't work (error) */
TEST_F(AndroidDisplayFactoryTest, hwc_module_unopenable_uses_gpu)
{
    using namespace testing;

    mt::FailingHardwareModuleStub failing_hwc_module_stub;
    EXPECT_CALL(hw_access_mock, hw_get_module(StrEq(HWC_HARDWARE_MODULE_ID),_))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(&failing_hwc_module_stub), Return(0)));

    EXPECT_CALL(*mock_hwc_factory, create_hwc_1_1(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_hwc_display(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_gpu_display())
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory); 
    display_factory.create_display();
}

/* this is normal operation on hwc capable device */
TEST_F(AndroidDisplayFactoryTest, hwc_with_hwc_device_version_11_success)
{
    using namespace testing;

    hw_access_mock.mock_hwc_device->common.version = HWC_DEVICE_API_VERSION_1_1;

    EXPECT_CALL(*mock_hwc_factory, create_hwc_1_1(_))
        .Times(1);
    EXPECT_CALL(*mock_display_allocator, create_hwc_display(_))
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory);
    display_factory.create_display();
}

// TODO: kdub support v 1.0 and 1.2. for the time being, alloc a fallback gpu display
TEST_F(AndroidDisplayFactoryTest, hwc_with_hwc_device_failure_because_hwc_version10_not_supported)
{
    using namespace testing;

    hw_access_mock.mock_hwc_device->common.version = HWC_DEVICE_API_VERSION_1_0;

    EXPECT_CALL(*mock_hwc_factory, create_hwc_1_1(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_hwc_display(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_gpu_display())
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory); 
    display_factory.create_display();
}

TEST_F(AndroidDisplayFactoryTest, hwc_with_hwc_device_failure_because_hwc_version12_not_supported)
{
    using namespace testing;

    hw_access_mock.mock_hwc_device->common.version = HWC_DEVICE_API_VERSION_1_2;

    EXPECT_CALL(*mock_hwc_factory, create_hwc_1_1(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_hwc_display(_))
        .Times(0);
    EXPECT_CALL(*mock_display_allocator, create_gpu_display())
        .Times(1);

    mga::AndroidDisplayFactory display_factory(mock_display_allocator, mock_hwc_factory); 
    display_factory.create_display();
}
