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

#include "mir/graphics/buffer_initializer.h"
#include "mir/graphics/display_buffer.h"
#include "src/server/surfaces/gl_pixel_buffer.h"
#include "src/server/graphics/android/android_display.h"
#include "src/server/graphics/android/resource_factory.h"
#include "src/server/graphics/android/android_graphic_buffer_allocator.h"
#include "src/server/graphics/android/display_buffer_factory.h"

#include "examples/graphics.h"
#include "mir_test/draw/android_graphics.h"
#include "mir_test_doubles/mock_display_report.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace mga=mir::graphics::android;
namespace mg=mir::graphics;
namespace md=mir::draw;
namespace mtd=mir::test::doubles;

namespace
{
class AndroidGPUDisplay : public ::testing::Test
{
protected:
    static void SetUpTestCase()
    {
        /* note: exynos5 hwc driver can sends sigterm to vsync thread when closing hwc.
           the server can handle this, but we need the test to as well */
        original_sigterm_handler = signal(SIGTERM, [](int){});

        /* note about fb_device: OMAP4 drivers seem to only be able to open fb once
           per process (repeated framebuffer_{open,close}() doesn't seem to work). once we
           figure out why, we can remove fb_device in the test fixture */
        auto buffer_initializer = std::make_shared<mg::NullBufferInitializer>();
        buffer_allocator = std::make_shared<mga::AndroidGraphicBufferAllocator>(buffer_initializer);
        display_resource_factory = std::make_shared<mga::ResourceFactory>(buffer_allocator);
    }

    static void TearDownTestCase()
    {
        signal(SIGTERM, original_sigterm_handler);
        display_resource_factory.reset();
    }

    md::glAnimationBasic gl_animation;

    static std::shared_ptr<mga::ResourceFactory> display_resource_factory;
    static void (*original_sigterm_handler)(int);
    static std::shared_ptr<mga::AndroidGraphicBufferAllocator> buffer_allocator;
};

void (*AndroidGPUDisplay::original_sigterm_handler)(int);
std::shared_ptr<mga::ResourceFactory> AndroidGPUDisplay::display_resource_factory;
std::shared_ptr<mga::AndroidGraphicBufferAllocator> AndroidGPUDisplay::buffer_allocator;
}

TEST_F(AndroidGPUDisplay, gpu_display_ok_with_gles)
{
    auto should_use_fb_fallback = true;
    auto mock_display_report = std::make_shared<testing::NiceMock<mtd::MockDisplayReport>>();
    auto display_buffer_factory = std::make_shared<mga::DisplayBufferFactory>(
        display_resource_factory, mock_display_report, should_use_fb_fallback);

    mga::AndroidDisplay display(display_buffer_factory, mock_display_report);
    display.for_each_display_buffer([this](mg::DisplayBuffer& buffer)
    {
        buffer.make_current();
        gl_animation.init_gl();

        gl_animation.render_gl();
        buffer.post_update();

        gl_animation.render_gl();
        buffer.post_update();
    });
}

TEST_F(AndroidGPUDisplay, hwc_display_ok_with_gles)
{
    auto should_use_fb_fallback = false;
    auto mock_display_report = std::make_shared<testing::NiceMock<mtd::MockDisplayReport>>();
    auto display_buffer_factory = std::make_shared<mga::DisplayBufferFactory>(
        display_resource_factory, mock_display_report, should_use_fb_fallback);

    mga::AndroidDisplay display(display_buffer_factory, mock_display_report);
    display.for_each_display_buffer([this](mg::DisplayBuffer& buffer)
    {
        buffer.make_current();
        gl_animation.init_gl();

        gl_animation.render_gl();
        buffer.post_update();

        gl_animation.render_gl();
        buffer.post_update();
    });
}

TEST_F(AndroidGPUDisplay, screenshot)
{
    auto should_use_fb_fallback = false;
    auto mock_display_report = std::make_shared<testing::NiceMock<mtd::MockDisplayReport>>();
    auto display_buffer_factory = std::make_shared<mga::DisplayBufferFactory>(
        display_resource_factory, mock_display_report, should_use_fb_fallback);

    mga::AndroidDisplay display(display_buffer_factory, mock_display_report);
    auto context = display.create_gl_context();
    mir::surfaces::GLPixelBuffer pixel_buffer(std::move(context));

    auto buffer = buffer_allocator->alloc_buffer_platform(
        mir::geometry::Size{512, 512},
        mir::geometry::PixelFormat::abgr_8888,
        mga::BufferUsage::use_hardware);

    pixel_buffer.fill_from(*buffer); 
}
