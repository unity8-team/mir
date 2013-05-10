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

#include "src/server/graphics/android/hwc10_device.h"
#include "src/server/graphics/android/hwc11_device.h"
#include "src/server/graphics/android/hwc_layerlist.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"
#include "mir_test_doubles/mock_hwc_organizer.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_display_support_provider.h"

#include <thread>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <gtest/gtest.h>

namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

template<class T>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device(std::shared_ptr<hwc_composer_device_1> const& hwc_device,
                                                std::shared_ptr<mga::HWCLayerOrganizer> const& organizer,
                                                std::shared_ptr<mga::DisplaySupportProvider> const& fbdev);

template <>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device<mga::HWC10Device>(
                                                std::shared_ptr<hwc_composer_device_1> const& hwc_device,
                                                std::shared_ptr<mga::HWCLayerOrganizer> const&, 
                                                std::shared_ptr<mga::DisplaySupportProvider> const& fbdev)
{
    return std::make_shared<mga::HWC10Device>(hwc_device, fbdev);
}

template <>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device<mga::HWC11Device>(
                                                std::shared_ptr<hwc_composer_device_1> const& hwc_device,
                                                std::shared_ptr<mga::HWCLayerOrganizer> const& organizer,
                                                std::shared_ptr<mga::DisplaySupportProvider> const& fbdev)
{
    return std::make_shared<mga::HWC11Device>(hwc_device, organizer, fbdev);
}

namespace
{
struct HWCDummyLayer : public mga::HWCDefaultLayer
{
    HWCDummyLayer()
     : HWCDefaultLayer({})
    {
    }
};
}

template<typename T>
class HWCCommon : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        using namespace testing;

        mock_device = std::make_shared<testing::NiceMock<mtd::MockHWCComposerDevice1>>();
        mock_organizer = std::make_shared<testing::NiceMock<mtd::MockHWCOrganizer>>();
        mock_fbdev = std::make_shared<testing::NiceMock<mtd::MockDisplaySupportProvider>>();
        ON_CALL(*mock_fbdev, number_of_framebuffers_available())
            .WillByDefault(Return(2u));
        ON_CALL(*mock_fbdev, display_format())
            .WillByDefault(Return(geom::PixelFormat::abgr_8888));
    }

    std::shared_ptr<mtd::MockHWCOrganizer> mock_organizer;
    std::shared_ptr<mtd::MockHWCComposerDevice1> mock_device;
    std::shared_ptr<mtd::MockDisplaySupportProvider> mock_fbdev;
};

typedef ::testing::Types<mga::HWC10Device, mga::HWC11Device> HWCDeviceTestTypes;
TYPED_TEST_CASE(HWCCommon, HWCDeviceTestTypes);

TYPED_TEST(HWCCommon, test_proc_registration)
{
    using namespace testing;

    hwc_procs_t const* procs;
    EXPECT_CALL(*(this->mock_device), registerProcs_interface(this->mock_device.get(), _))
        .Times(1)
        .WillOnce(SaveArg<1>(&procs));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);

    EXPECT_NE(nullptr, procs->invalidate);
    EXPECT_NE(nullptr, procs->vsync);
    EXPECT_NE(nullptr, procs->hotplug);
}

TYPED_TEST(HWCCommon, test_vsync_activation_comes_after_proc_registration)
{
    using namespace testing;

    InSequence sequence_enforcer;
    EXPECT_CALL(*this->mock_device, registerProcs_interface(this->mock_device.get(),_))
        .Times(1);
    EXPECT_CALL(*this->mock_device, eventControl_interface(this->mock_device.get(), 0, HWC_EVENT_VSYNC, 1))
        .Times(1)
        .WillOnce(Return(0));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    testing::Mock::VerifyAndClearExpectations(this->mock_device.get());
}

TYPED_TEST(HWCCommon, test_vsync_activation_failure_throws)
{
    using namespace testing;

    EXPECT_CALL(*this->mock_device, eventControl_interface(this->mock_device.get(), 0, HWC_EVENT_VSYNC, 1))
        .Times(1)
        .WillOnce(Return(-EINVAL));

    EXPECT_THROW({
        auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    }, std::runtime_error);
}

namespace
{
static mga::HWCDevice *global_device;
void* waiting_device(void*)
{
    global_device->wait_for_vsync();
    return NULL;
}
}

TYPED_TEST(HWCCommon, test_vsync_hook_waits)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    global_device = device.get();

    pthread_t thread;
    pthread_create(&thread, NULL, waiting_device, NULL);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    void* retval;
    auto error = pthread_tryjoin_np(thread, &retval);
    ASSERT_EQ(EBUSY, error);

    device->notify_vsync();
    error = pthread_join(thread, &retval);
    ASSERT_EQ(0, error);

}

TYPED_TEST(HWCCommon, test_vsync_hook_from_hwc_unblocks_wait)
{
    using namespace testing;

    hwc_procs_t const* procs;
    EXPECT_CALL(*this->mock_device, registerProcs_interface(this->mock_device.get(), _))
        .Times(1)
        .WillOnce(SaveArg<1>(&procs));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    global_device = device.get();

    pthread_t thread;
    pthread_create(&thread, NULL, waiting_device, NULL);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    void* retval;
    auto error = pthread_tryjoin_np(thread, &retval);
    ASSERT_EQ(EBUSY, error);

    procs->vsync(procs, 0, 0);
    error = pthread_join(thread, &retval);
    ASSERT_EQ(0, error);
}

TYPED_TEST(HWCCommon, test_hwc_turns_on_display_after_proc_registration)
{
    using namespace testing;
    InSequence sequence_enforcer;
    EXPECT_CALL(*this->mock_device, registerProcs_interface(this->mock_device.get(),_))
        .Times(1);
    EXPECT_CALL(*this->mock_device, blank_interface(this->mock_device.get(), HWC_DISPLAY_PRIMARY, 0))
        .Times(1);

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    testing::Mock::VerifyAndClearExpectations(this->mock_device.get());
}

TYPED_TEST(HWCCommon, test_hwc_throws_on_blank_error)
{
    using namespace testing;

    EXPECT_CALL(*this->mock_device, blank_interface(this->mock_device.get(), HWC_DISPLAY_PRIMARY, 0))
        .Times(1)
        .WillOnce(Return(-1));

    EXPECT_THROW({
        auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    }, std::runtime_error);
}

TYPED_TEST(HWCCommon, test_hwc_display_is_deactivated_on_destroy)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);

    EXPECT_CALL(*this->mock_device, blank_interface(this->mock_device.get(), HWC_DISPLAY_PRIMARY, 1))
        .Times(1);
    EXPECT_CALL(*this->mock_device, eventControl_interface(this->mock_device.get(), HWC_DISPLAY_PRIMARY, HWC_EVENT_VSYNC, 0))
        .Times(1);
    device.reset();
}

TYPED_TEST(HWCCommon, hwc_device_reports_2_fbs_available_by_default)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    EXPECT_EQ(2u, device->number_of_framebuffers_available());
}

TYPED_TEST(HWCCommon, hwc_device_reports_abgr_8888_by_default)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_organizer, this->mock_fbdev);
    EXPECT_EQ(geom::PixelFormat::abgr_8888, device->display_format());
}
