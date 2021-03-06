/*
 * Copyright © 2012 Canonical Ltd.
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

#include "src/platforms/android/server/hal_component_factory.h"
#include "android_format_conversion-inl.h"
#include "src/platforms/android/server/resource_factory.h"
#include "src/platforms/android/server/hwc_loggers.h"
#include "src/platforms/android/server/hwc_configuration.h"
#include "src/platforms/android/server/fb_device.h"
#include "src/platforms/android/server/device_quirks.h"
#include "src/platforms/android/server/hwc_layerlist.h"
#include "mir/test/doubles/mock_buffer.h"
#include "mir/test/doubles/mock_display_report.h"
#include "mir/test/fake_shared.h"
#include "mir/test/doubles/mock_android_hw.h"
#include "mir/test/doubles/mock_fb_hal_device.h"
#include "mir/test/doubles/mock_egl.h"
#include "mir/test/doubles/mock_gl.h"
#include "mir/test/doubles/mock_android_native_buffer.h"
#include "mir/test/doubles/mock_hwc_report.h"
#include "mir/test/doubles/mock_hwc_device_wrapper.h"
#include "mir/test/doubles/stub_gl_config.h"
#include "mir/test/doubles/stub_gl_program_factory.h"
#include "mir/test/doubles/stub_display_configuration.h"
#include <system/window.h>
#include <gtest/gtest.h>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mt=mir::test;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

namespace
{
struct MockResourceFactory: public mga::DisplayResourceFactory
{
    ~MockResourceFactory() noexcept {}
    MockResourceFactory()
    {
        using namespace testing;
        ON_CALL(*this, create_hwc_wrapper(_))
            .WillByDefault(Return(std::make_tuple(nullptr, mga::HwcVersion::hwc10)));
        ON_CALL(*this, create_fb_native_device()).WillByDefault(Return(nullptr));
    }

    MOCK_CONST_METHOD1(create_hwc_wrapper,
        std::tuple<std::shared_ptr<mga::HwcWrapper>, mga::HwcVersion>(std::shared_ptr<mga::HwcReport> const&));
    MOCK_CONST_METHOD0(create_fb_native_device, std::shared_ptr<framebuffer_device_t>());
};

class HalComponentFactory : public ::testing::Test
{
public:
    void SetUp()
    {
        using namespace testing;
        quirks = std::make_shared<mga::DeviceQuirks>(mga::PropertiesOps{});
        mock_resource_factory = std::make_shared<testing::NiceMock<MockResourceFactory>>();
        mock_wrapper = std::make_shared<testing::NiceMock<mtd::MockHWCDeviceWrapper>>();
        ON_CALL(*mock_resource_factory, create_hwc_wrapper(_))
            .WillByDefault(Return(std::make_tuple(mock_wrapper, mga::HwcVersion::hwc11)));
        ON_CALL(*mock_resource_factory, create_fb_native_device())
            .WillByDefault(Return(mt::fake_shared(fb_hal_mock)));
    }

    testing::NiceMock<mtd::MockEGL> mock_egl;
    testing::NiceMock<mtd::MockGL> mock_gl;
    testing::NiceMock<mtd::HardwareAccessMock> hw_access_mock;
    testing::NiceMock<mtd::MockFBHalDevice> fb_hal_mock;
    std::shared_ptr<MockResourceFactory> mock_resource_factory;
    testing::NiceMock<mtd::MockDisplayReport> mock_display_report;
    std::shared_ptr<mtd::MockHwcReport> mock_hwc_report{
        std::make_shared<testing::NiceMock<mtd::MockHwcReport>>()};
    std::shared_ptr<mtd::MockHWCDeviceWrapper> mock_wrapper;
    std::shared_ptr<mga::DeviceQuirks> quirks;
};
}

TEST_F(HalComponentFactory, builds_hwc_version_10)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Return(std::make_tuple(mock_wrapper, mga::HwcVersion::hwc10)));
    EXPECT_CALL(*mock_resource_factory, create_fb_native_device());
    EXPECT_CALL(*mock_hwc_report, report_hwc_version(mga::HwcVersion::hwc10));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    factory.create_display_device();
}

TEST_F(HalComponentFactory, builds_hwc_version_11_and_later)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Return(std::make_tuple(mock_wrapper, mga::HwcVersion::hwc11)));
    EXPECT_CALL(*mock_hwc_report, report_hwc_version(mga::HwcVersion::hwc11));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    factory.create_display_device();
}

TEST_F(HalComponentFactory, allocates_correct_hwc_configuration_for_backup_display)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_fb_native_device());
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Throw(std::runtime_error("")));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    auto hwc_config = factory.create_hwc_configuration();
    EXPECT_THAT(dynamic_cast<mga::FbControl*>(hwc_config.get()), Ne(nullptr));
}

TEST_F(HalComponentFactory, allocates_correct_hwc_configuration_for_hwc_version_10_to_13)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Return(std::make_tuple(mock_wrapper, mga::HwcVersion::hwc10)));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    auto hwc_config = factory.create_hwc_configuration();
    EXPECT_THAT(dynamic_cast<mga::HwcBlankingControl*>(hwc_config.get()), Ne(nullptr));
}

TEST_F(HalComponentFactory, allocates_correct_hwc_configuration_for_hwc_version_14_and_later)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Return(std::make_tuple(mock_wrapper, mga::HwcVersion::hwc14)));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    auto hwc_config = factory.create_hwc_configuration();
    EXPECT_THAT(dynamic_cast<mga::HwcPowerModeControl*>(hwc_config.get()), Ne(nullptr));
}

TEST_F(HalComponentFactory, hwc_failure_falls_back_to_fb)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Throw(std::runtime_error("")));
    EXPECT_CALL(*mock_resource_factory, create_fb_native_device());
    EXPECT_CALL(*mock_hwc_report, report_legacy_fb_module());

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    factory.create_display_device();
}

TEST_F(HalComponentFactory, hwc_and_fb_failure_fatal)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Throw(std::runtime_error("")));
    EXPECT_CALL(*mock_resource_factory, create_fb_native_device())
        .WillOnce(Throw(std::runtime_error("")));

    EXPECT_THROW({
        mga::HalComponentFactory factory(
            mock_resource_factory,
            mock_hwc_report,
            quirks);
    }, std::runtime_error);
}

//some drivers incorrectly report 0 buffers available. request 2 fbs in this case.
TEST_F(HalComponentFactory, determine_fbnum_always_reports_2_minimum)
{
    using namespace testing;
    EXPECT_CALL(*mock_resource_factory, create_hwc_wrapper(_))
        .WillOnce(Throw(std::runtime_error("")));
    EXPECT_CALL(*mock_resource_factory, create_fb_native_device())
        .WillOnce(Return(std::make_shared<mtd::MockFBHalDevice>(
            0, 0, mir_pixel_format_abgr_8888, 0, 0.0, 0.0)));

    mga::HalComponentFactory factory(
        mock_resource_factory,
        mock_hwc_report,
        quirks);
    auto fbs = factory.create_framebuffers(mtd::StubDisplayConfig(1).outputs[0]);
    std::vector<mg::BufferID> buffer_list;
    for(auto i = 0u; i < 10u; i++)
        buffer_list.push_back(fbs->buffer_for_render()->id());
    std::sort(buffer_list.begin(), buffer_list.end());
    buffer_list.erase(std::unique(buffer_list.begin(), buffer_list.end()), buffer_list.end());
    EXPECT_THAT(buffer_list.size(), Eq(2u));
}

TEST_F(HalComponentFactory, doesnt_complain_if_version_is_supported)
{
    using namespace testing;
    auto supported_versions = {
        mga::HwcVersion::hwc10, 
        mga::HwcVersion::hwc11, 
        mga::HwcVersion::hwc12, 
        mga::HwcVersion::hwc13, 
        mga::HwcVersion::hwc14, 
        mga::HwcVersion::hwc15 };
    for (auto supported_version : supported_versions)
    {
        ON_CALL(*mock_resource_factory, create_hwc_wrapper(_))
            .WillByDefault(Return(std::make_tuple(mock_wrapper, supported_version)));
        mga::HalComponentFactory factory(
            mock_resource_factory,
            mock_hwc_report,
            quirks);
        EXPECT_THAT(factory.create_display_device(), Ne(nullptr));
        EXPECT_THAT(factory.create_layer_list(), Ne(nullptr));
    }
}
