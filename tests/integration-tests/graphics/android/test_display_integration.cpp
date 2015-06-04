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

#include "mir/graphics/display_buffer.h"
#include "src/platforms/android/server/display.h"
#include "src/platforms/android/server/hwc_loggers.h"
#include "src/platforms/android/server/resource_factory.h"
#include "src/platforms/android/server/android_graphic_buffer_allocator.h"
#include "src/platforms/android/server/hal_component_factory.h"
#include "src/platforms/android/server/device_quirks.h"
#include "src/server/graphics/program_factory.h"
#include "src/server/report/null_report_factory.h"

#include "examples/graphics.h"
#include "mir_test_doubles/mock_display_report.h"
#include "mir_test_doubles/stub_gl_config.h"
#include "mir_test_doubles/stub_renderable.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace mga=mir::graphics::android;
namespace mg=mir::graphics;
namespace geom=mir::geometry;
namespace md=mir::draw;
namespace mtd=mir::test::doubles;

namespace
{
void (*original_sigterm_handler)(int);
std::shared_ptr<mg::Display> display;
std::shared_ptr<mga::GraphicBufferAllocator> buffer_allocator;

class AndroidDisplay : public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        /* note: exynos5 hwc driver can sends sigterm to vsync thread when closing hwc.
           the server can handle this, but we need the test to as well */
        original_sigterm_handler = signal(SIGTERM, [](int){});

        auto quirks = std::make_shared<mga::DeviceQuirks>(mga::PropertiesOps{});
        buffer_allocator = std::make_shared<mga::AndroidGraphicBufferAllocator>(quirks);

        /* note about fb_device: OMAP4 drivers seem to only be able to open fb once
           per process (repeated framebuffer_{open,close}() doesn't seem to work). once we
           figure out why, we can remove fb_device in the test fixture */
        auto report = std::make_shared<mga::NullHwcReport>();
        auto display_resource_factory = std::make_shared<mga::ResourceFactory>();
        auto null_display_report = mir::report::null_display_report();
        auto stub_gl_config = std::make_shared<mtd::StubGLConfig>();
        auto display_buffer_factory = std::make_shared<mga::HalComponentFactory>(
            buffer_allocator, display_resource_factory, report, quirks);
        auto program_factory = std::make_shared<mg::ProgramFactory>();
        display = std::make_shared<mga::Display>(
            display_buffer_factory, program_factory, stub_gl_config, null_display_report, mga::OverlayOptimization::enabled);
    }

    static void TearDownTestCase()
    {
        signal(SIGTERM, original_sigterm_handler);
        display.reset();
        buffer_allocator.reset();
    }
};
}

TEST_F(AndroidDisplay, display_can_post)
{
    display->for_each_display_sync_group([](mg::DisplaySyncGroup& group) {
        group.for_each_display_buffer([](mg::DisplayBuffer& buffer)
        {
            buffer.make_current();
            md::glAnimationBasic gl_animation;
            gl_animation.init_gl();

            gl_animation.render_gl();
            buffer.gl_swap_buffers();

            gl_animation.render_gl();
            buffer.gl_swap_buffers();
        });
        group.post();
    });
}

TEST_F(AndroidDisplay, display_can_post_overlay)
{
    display->for_each_display_sync_group([](mg::DisplaySyncGroup& group) {
        group.for_each_display_buffer([](mg::DisplayBuffer& db)
        {
            db.make_current();
            auto area = db.view_area();
            auto buffer = buffer_allocator->alloc_buffer_platform(
                area.size, mir_pixel_format_abgr_8888, mga::BufferUsage::use_hardware);
            mg::RenderableList list{
                std::make_shared<mtd::StubRenderable>(buffer, area)
            };

            db.post_renderables_if_optimizable(list);
        });
        group.post();
    });
}
