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

#include "mir/graphics/android/sync_fence.h"
#include "src/platform/graphics/android/hwc_fb_device.h"
#include "src/platform/graphics/android/hwc_device.h"
#include "src/platform/graphics/android/hwc_wrapper.h"
#include "src/platform/graphics/android/hwc_layerlist.h"
#include "src/platform/graphics/android/hwc_vsync_coordinator.h"
#include "mir_test_doubles/mock_hwc_composer_device_1.h"
#include "mir_test_doubles/mock_hwc_vsync_coordinator.h"
#include "mir_test_doubles/mock_hwc_device_wrapper.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_display_device.h"
#include "mir_test_doubles/mock_fb_hal_device.h"

#include <thread>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <gtest/gtest.h>

namespace mg=mir::graphics;
namespace mga=mg::android;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

template<class T>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device(
    std::shared_ptr<mga::HwcWrapper> const& hwc_device,
    std::shared_ptr<framebuffer_device_t> const& fb_device,
    std::shared_ptr<mga::HWCVsyncCoordinator> const& coordinator);

template <>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device<mga::HwcFbDevice>(
    std::shared_ptr<mga::HwcWrapper> const& hwc_device,
    std::shared_ptr<framebuffer_device_t> const& fb_device,
    std::shared_ptr<mga::HWCVsyncCoordinator> const& coordinator)
{
    return std::make_shared<mga::HwcFbDevice>(hwc_device, fb_device, coordinator);
}

template <>
std::shared_ptr<mga::HWCCommonDevice> make_hwc_device<mga::HwcDevice>(
    std::shared_ptr<mga::HwcWrapper> const& hwc_device,
    std::shared_ptr<framebuffer_device_t> const&,
    std::shared_ptr<mga::HWCVsyncCoordinator> const& coordinator)
{
    auto file_ops = std::make_shared<mga::RealSyncFileOps>();
    return std::make_shared<mga::HwcDevice>(hwc_device, coordinator, file_ops);
}

class HWCCommonFixture : public ::testing::Test
{
protected:
    void setup_display_config()
    {
        ON_CALL(*this->mock_device, get_display_configs(primary))
            .WillByDefault(testing::Return(std::vector<uint32_t>{0}));

        ON_CALL(*this->mock_device, get_display_configs(external))
            .WillByDefault(testing::Return(std::vector<uint32_t>{1}));

        ON_CALL(*this->mock_device, get_display_config_attributes(primary, 0))
            .WillByDefault(testing::Return(
                    mga::HwcWrapper::Attributes{
                        geom::Size{280,190},
                        320,  // dpi x
                        320,  // dpi y
                        60    // hz
                        }));

        ON_CALL(*this->mock_device, get_display_config_attributes(external, 1))
            .WillByDefault(testing::Return(
                    mga::HwcWrapper::Attributes{
                        geom::Size{1920,1200},
                        120,  // dpi x
                        120,  // dpi y
                        60    // hz
                        }));
    }
    void setup_egl_configs()
    {
        using namespace testing;
        ON_CALL(mock_egl,eglChooseConfig(_,_,_,_,_))
            .WillByDefault(
                Invoke([](EGLDisplay, EGLint const *, EGLConfig * configs, EGLint config_size, EGLint * num_config)
                       {
                          if (config_size >= 3)
                             configs[1] = EGLConfig(42);
                          if (config_size >= 2)
                             configs[1] = EGLConfig(13);
                          if (config_size >= 1)
                             configs[0] = EGLConfig(21);
                          *num_config = std::min(3,config_size);
                          return EGL_TRUE;
                       })
                );

        ON_CALL(mock_egl,eglGetConfigAttrib(_,_,_,_))
            .WillByDefault(
                Invoke([](EGLDisplay, EGLConfig config, EGLint attribute, EGLint * value) -> EGLBoolean
                       {
                          if (!value) return EGL_FALSE;
                          if (attribute!=EGL_NATIVE_VISUAL_ID) return EGL_FALSE;

                          if (config == EGLConfig(42))
                              *value = 0; // place holder for format that cannot be converted to mir format
                          if (config == EGLConfig(13))
                              *value = HAL_PIXEL_FORMAT_RGBA_8888;
                          if (config == EGLConfig(21))
                              *value = HAL_PIXEL_FORMAT_RGBX_8888;
                          return EGL_TRUE;
                       })
                );
    }
    testing::NiceMock<mtd::MockEGL> mock_egl;
    std::shared_ptr<mtd::MockVsyncCoordinator> mock_vsync = std::make_shared<testing::NiceMock<mtd::MockVsyncCoordinator>>();
    std::shared_ptr<mtd::MockHWCDeviceWrapper> mock_device = std::make_shared<testing::NiceMock<mtd::MockHWCDeviceWrapper>>();
    std::shared_ptr<mtd::MockFBHalDevice> mock_fbdev = std::make_shared<mtd::MockFBHalDevice>(900, 600, HAL_PIXEL_FORMAT_RGBX_8888,
                                                                                             400.0f, 400.0f, 67.6f, 1);

    mg::DisplayConfigurationOutputId primary{HWC_DISPLAY_PRIMARY};
    mg::DisplayConfigurationOutputId external{HWC_DISPLAY_EXTERNAL};
    mg::DisplayConfigurationOutputId virtual_output{HWC_DISPLAY_VIRTUAL};
};

template<typename T>
using HWCCommon = HWCCommonFixture;

typedef ::testing::Types<mga::HwcFbDevice, mga::HwcDevice> HWCDeviceTestTypes;
TYPED_TEST_CASE(HWCCommon, HWCDeviceTestTypes);

using PostHWC10Tests = HWCCommonFixture;
using HWC10Tests = HWCCommonFixture;

TYPED_TEST(HWCCommon, test_proc_registration)
{
    using namespace testing;
    std::shared_ptr<mga::HWCCallbacks> callbacks;
    EXPECT_CALL(*(this->mock_device), register_hooks(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&callbacks));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    ASSERT_THAT(callbacks, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.invalidate, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.vsync, Ne(nullptr));
    EXPECT_THAT(callbacks->hooks.hotplug, Ne(nullptr));
}

TYPED_TEST(HWCCommon, test_device_destruction_unregisters_self_from_hooks)
{
    using namespace testing;
    std::shared_ptr<mga::HWCCallbacks> callbacks;
    EXPECT_CALL(*(this->mock_device), register_hooks(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&callbacks));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    ASSERT_THAT(callbacks, Ne(nullptr));
    EXPECT_THAT(callbacks->self, Eq(device.get()));
    device = nullptr;
    EXPECT_THAT(callbacks->self, Eq(nullptr));    
}

TYPED_TEST(HWCCommon, registerst_hooks_and_turns_on_display)
{
    using namespace testing;

    Sequence seq;
    EXPECT_CALL(*this->mock_device, register_hooks(_))
        .InSequence(seq);
    EXPECT_CALL(*this->mock_device, display_on())
        .InSequence(seq);
    EXPECT_CALL(*this->mock_device, vsync_signal_on())
        .InSequence(seq);

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    testing::Mock::VerifyAndClearExpectations(this->mock_device.get());
}

TYPED_TEST(HWCCommon, test_hwc_suspend_standby_throw)
{
    using namespace testing;
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THROW({
        device->mode(mir_power_mode_suspend);
    }, std::runtime_error);
    EXPECT_THROW({
        device->mode(mir_power_mode_standby);
    }, std::runtime_error);
}

TYPED_TEST(HWCCommon, test_hwc_deactivates_vsync_on_blank)
{
    using namespace testing;

    InSequence seq;
    EXPECT_CALL(*this->mock_device, display_on())
        .Times(1);
    EXPECT_CALL(*this->mock_device, vsync_signal_on())
        .Times(1);
    EXPECT_CALL(*this->mock_device, vsync_signal_off())
        .Times(1);
    EXPECT_CALL(*this->mock_device, display_off())
        .Times(1);

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    device->mode(mir_power_mode_off);
}

TYPED_TEST(HWCCommon, test_hwc_display_is_deactivated_on_destroy)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    testing::InSequence seq;
    EXPECT_CALL(*this->mock_device, vsync_signal_off())
        .Times(1);
    EXPECT_CALL(*this->mock_device, display_off())
        .Times(1);
    device.reset();
}

TYPED_TEST(HWCCommon, catches_exception_during_destruction)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    EXPECT_CALL(*this->mock_device, display_off())
        .WillOnce(testing::Throw(std::runtime_error("")));
    device.reset();
}

TYPED_TEST(HWCCommon, callback_calls_hwcvsync)
{
    using namespace testing;
    std::shared_ptr<mga::HWCCallbacks> callbacks;
    EXPECT_CALL(*(this->mock_device), register_hooks(_))
        .Times(1)
        .WillOnce(SaveArg<0>(&callbacks));

    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_CALL(*this->mock_vsync, notify_vsync())
        .Times(1);
    ASSERT_THAT(callbacks, Ne(nullptr));
    callbacks->hooks.vsync(&callbacks->hooks, 0, 0);

    callbacks->self = nullptr;
    callbacks->hooks.vsync(&callbacks->hooks, 0, 0);
}

TYPED_TEST(HWCCommon, set_orientation)
{
    auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    EXPECT_FALSE(device->apply_orientation(mir_orientation_left));
}

TYPED_TEST(HWCCommon, first_unblank_failure_is_not_fatal) //lp:1345533
{
    ON_CALL(*this->mock_device, display_on())
        .WillByDefault(testing::Throw(std::runtime_error("error")));
    EXPECT_NO_THROW({
        auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    });
}

TYPED_TEST(HWCCommon, first_vsync_failure_is_not_fatal) //lp:1345533
{
    ON_CALL(*this->mock_device, vsync_signal_on())
        .WillByDefault(testing::Throw(std::runtime_error("error")));
    EXPECT_NO_THROW({
        auto device = make_hwc_device<TypeParam>(this->mock_device, this->mock_fbdev, this->mock_vsync);
    });
}

TEST_F(HWC10Tests, throws_on_non_primary_display_output_configurations)
{
    using namespace testing;
    auto device = make_hwc_device<mga::HwcFbDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THROW({
        device->get_output_configuration(this->external);
    }, std::runtime_error);
    EXPECT_THROW({
        device->get_output_configuration(this->virtual_output);
    }, std::runtime_error);
}

TEST_F(PostHWC10Tests, throws_on_virtual_display_output_configurations)
{
    using namespace testing;
    this->setup_display_config();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THROW({device->get_output_configuration(this->virtual_output);}, std::runtime_error);
    EXPECT_NO_THROW({device->get_output_configuration(this->external);});
    EXPECT_NO_THROW({device->get_output_configuration(this->primary);});
}

TEST_F(PostHWC10Tests, queries_hwc_device_for_configs)
{
    using namespace testing;
    this->setup_display_config();

    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_CALL(*this->mock_device, get_display_configs(this->primary));

    device->get_output_configuration(this->primary);
}

TEST_F(PostHWC10Tests, throws_on_no_available_hwc_configurations)
{
    using namespace testing;
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    ON_CALL(*this->mock_device, get_display_configs(_))
        .WillByDefault(Return(std::vector<uint32_t>{}));
    EXPECT_THROW({
        device->get_output_configuration(this->primary);
        }, std::runtime_error);
}

TEST_F(PostHWC10Tests, queries_display_config_attributes)
{
    using namespace testing;
    this->setup_display_config();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_CALL(*this->mock_device, get_display_config_attributes(this->primary, 0));

    device->get_output_configuration(this->primary);
}

TEST_F(PostHWC10Tests, display_configuration_matches_expectations)
{
    using namespace testing;
    this->setup_display_config();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THAT(
        device->get_output_configuration(this->primary),
        Eq(
            mg::DisplayConfigurationOutput
            {
                this->primary,
                mg::DisplayConfigurationCardId{0},
                mg::DisplayConfigurationOutputType::lvds,
                {mir_pixel_format_abgr_8888},
                {{{280, 190}, 60}},
                0,
                {22, 15},
                true,
                true,
                {0, 0},
                0,
                mir_pixel_format_abgr_8888,
                mir_power_mode_on,
                mir_orientation_normal
            }
            ));
}

TEST_F(PostHWC10Tests, display_configuration_matches_expectations_on_external_display)
{
    using namespace testing;
    this->setup_display_config();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THAT(
        device->get_output_configuration(this->external),
        Eq(
            mg::DisplayConfigurationOutput
            {
                this->external,
                mg::DisplayConfigurationCardId{0},
                mg::DisplayConfigurationOutputType::hdmia,
                {mir_pixel_format_abgr_8888},
                {{{1920, 1200}, 60}},
                0,
                {406, 254},
                true,
                true,
                {0, 0},
                0,
                mir_pixel_format_abgr_8888,
                mir_power_mode_on,
                mir_orientation_normal
            }
            ));
}

TEST_F(PostHWC10Tests, queries_egl_configuration)
{
    using namespace testing;
    this->setup_display_config();
    this->setup_egl_configs();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_CALL(mock_egl,eglChooseConfig(_,_,_,_,_)).Times(1);
    EXPECT_CALL(mock_egl,eglGetConfigAttrib(_,_,EGL_NATIVE_VISUAL_ID,_)).Times(3);

    device->get_output_configuration(this->primary);
}

TEST_F(PostHWC10Tests, converts_egl_configs_to_mir_pixel_formats)
{
    using namespace testing;
    this->setup_display_config();
    this->setup_egl_configs();
    auto device = make_hwc_device<mga::HwcDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THAT(
        device->get_output_configuration(this->primary).pixel_formats,
        Eq(std::vector<MirPixelFormat>({mir_pixel_format_abgr_8888, mir_pixel_format_xbgr_8888}))
          );
}

TEST_F(HWC10Tests, display_configuration_matches_expectations)
{
    using namespace testing;
    auto device = make_hwc_device<mga::HwcFbDevice>(this->mock_device, this->mock_fbdev, this->mock_vsync);

    EXPECT_THAT(
        device->get_output_configuration(this->primary),
        Eq(
            mg::DisplayConfigurationOutput
            {
                this->primary,
                mg::DisplayConfigurationCardId{0},
                mg::DisplayConfigurationOutputType::lvds,
                {mir_pixel_format_xbgr_8888},
                {{{900, 600}, 67.6f}},
                0,
                {57, 38},
                true,
                true,
                {0, 0},
                0,
                mir_pixel_format_xbgr_8888,
                mir_power_mode_on,
                mir_orientation_normal
            })
          );
}
