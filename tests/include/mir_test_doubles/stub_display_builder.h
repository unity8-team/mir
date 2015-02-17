/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_DISPLAY_BUILDER_H_
#define MIR_TEST_DOUBLES_STUB_DISPLAY_BUILDER_H_

#include "src/platforms/android/server/framebuffer_bundle.h"
#include "src/platforms/android/server/display_component_factory.h"
#include "src/platforms/android/server/configurable_display_buffer.h"
#include "src/platforms/android/server/hwc_configuration.h"
#include "mock_display_device.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct StubFramebufferBundle : public graphics::android::FramebufferBundle
{
    MirPixelFormat fb_format() override { return mir_pixel_format_abgr_8888; }
    geometry::Size fb_size() override { return {33, 34}; }
    double fb_refresh_rate() override { return 53.45; };
    std::shared_ptr<graphics::Buffer> buffer_for_render() { return nullptr; }
    std::shared_ptr<graphics::Buffer> last_rendered_buffer() { return nullptr; }
};

struct MockHwcConfiguration : public graphics::android::HwcConfiguration
{
    MockHwcConfiguration()
    {
        using namespace testing;
        ON_CALL(*this, subscribe_to_config_changes(_)).WillByDefault(Return(nullptr));
        ON_CALL(*this, active_attribs_for(graphics::android::DisplayName::primary))
            .WillByDefault(testing::Return(graphics::android::DisplayAttribs{
                {0,0},{0,0}, 0.0, true, mir_pixel_format_abgr_8888, 2}));
        ON_CALL(*this, active_attribs_for(graphics::android::DisplayName::external))
            .WillByDefault(testing::Return(graphics::android::DisplayAttribs{
                {0,0},{0,0}, 0.0, false, mir_pixel_format_abgr_8888, 2}));
    }
    MOCK_METHOD2(power_mode, void(graphics::android::DisplayName, MirPowerMode));
    MOCK_METHOD1(active_attribs_for, graphics::android::DisplayAttribs(graphics::android::DisplayName));
    MOCK_METHOD1(subscribe_to_config_changes,
        graphics::android::ConfigChangeSubscription(std::function<void()> const&));
};

struct StubHwcConfiguration : public graphics::android::HwcConfiguration
{
    void power_mode(graphics::android::DisplayName, MirPowerMode) override
    {
    }

    graphics::android::DisplayAttribs active_attribs_for(graphics::android::DisplayName name) override
    {
        if (name == graphics::android::DisplayName::external)
            return graphics::android::DisplayAttribs{{20,20}, {4,4}, 50.0f, false, mir_pixel_format_abgr_8888, 2};
        else
            return graphics::android::DisplayAttribs{{20,20}, {4,4}, 350.0f, true, mir_pixel_format_abgr_8888, 2};
    }

    
    graphics::android::ConfigChangeSubscription subscribe_to_config_changes(
        std::function<void()> const&) override
    {
        return nullptr;
    }
};

struct StubDisplayBuilder : public graphics::android::DisplayComponentFactory
{
    StubDisplayBuilder(geometry::Size sz)
        : sz(sz),
          config{new StubHwcConfiguration},
          device{new testing::NiceMock<MockDisplayDevice>()}
    {
    }

    StubDisplayBuilder()
        : StubDisplayBuilder(geometry::Size{0,0})
    {
    }

    std::unique_ptr<graphics::android::LayerList> create_layer_list()
    {
        return std::unique_ptr<graphics::android::LayerList>(
            new graphics::android::LayerList(std::make_shared<graphics::android::IntegerSourceCrop>(), {}));
    }

    std::unique_ptr<graphics::android::FramebufferBundle> create_framebuffers(graphics::android::DisplayAttribs const&) override
    {
        return std::unique_ptr<graphics::android::FramebufferBundle>(new StubFramebufferBundle());
    }

    std::unique_ptr<graphics::android::DisplayDevice> create_display_device() override
    {
        std::unique_ptr<graphics::android::DisplayDevice> mock_device(new testing::NiceMock<MockDisplayDevice>());
        std::swap(mock_device, device);
        return mock_device;
    }

    std::unique_ptr<graphics::android::HwcConfiguration> create_hwc_configuration() override
    {
        auto c = std::unique_ptr<graphics::android::HwcConfiguration>(new StubHwcConfiguration);
        std::swap(config, c);
        return std::move(c);
    }

    void with_next_device(std::function<void(MockDisplayDevice&)> const& fn)
    {
        std::unique_ptr<MockDisplayDevice> mock_device(new testing::NiceMock<MockDisplayDevice>());
        fn(*mock_device);
        device = std::move(mock_device);
    }
 
    void with_next_config(std::function<void(MockHwcConfiguration& mock_config)> const& fn)
    {
        std::unique_ptr<MockHwcConfiguration> mock_config{
            new testing::NiceMock<MockHwcConfiguration>()};
        fn(*mock_config);
        config = std::move(mock_config); 
    }

    geometry::Size sz;
    std::unique_ptr<graphics::android::HwcConfiguration> config;
    std::unique_ptr<graphics::android::DisplayDevice> device;
};
}
}
} // namespace mir

#endif /* MIR_TEST_DOUBLES_STUB_DISPLAY_BUILDER_H_ */
