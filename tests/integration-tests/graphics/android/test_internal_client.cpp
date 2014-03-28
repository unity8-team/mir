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

#include "src/platform/graphics/android/android_graphic_buffer_allocator.h"
#include "src/platform/graphics/android/internal_client_window.h"
#include "src/platform/graphics/android/interpreter_cache.h"
#include "src/platform/graphics/android/internal_client.h"
#include "src/server/compositor/buffer_stream_factory.h"
#include "src/server/report/null_report_factory.h"
#include "mir/graphics/buffer_initializer.h"
#include "src/server/report/null_report_factory.h"
#include "mir/graphics/android/mir_native_window.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/internal_client.h"
#include "mir/graphics/internal_surface.h"
#include "src/server/scene/surface_stack.h"
#include "src/server/scene/surface_controller.h"
#include "mir/scene/scene_report.h"
#include "src/server/scene/surface_allocator.h"
#include "src/server/scene/surface_source.h"
#include "mir/shell/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/frontend/surface_id.h"
#include "mir/input/input_channel_factory.h"
#include "mir/options/program_option.h"

#include "mir_test_doubles/stub_input_registrar.h"
#include "mir_test_doubles/null_surface_configurator.h"

#include <EGL/egl.h>
#include <gtest/gtest.h>

#include <GLES2/gl2.h>


namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mc=mir::compositor;
namespace geom=mir::geometry;
namespace ms=mir::scene;
namespace msh=mir::shell;
namespace mf=mir::frontend;
namespace mi=mir::input;
namespace mtd=mir::test::doubles;
namespace mr = mir::report;
namespace mo=mir::options;

namespace
{
class AndroidInternalClient : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
    }
};

struct StubInputFactory : public mi::InputChannelFactory
{
    std::shared_ptr<mi::InputChannel> make_input_channel()
    {
        return std::shared_ptr<mi::InputChannel>();
    }
};
}

TEST_F(AndroidInternalClient, internal_client_creation_and_use)
{
    auto size = geom::Size{334, 122};
    auto pf  = mir_pixel_format_abgr_8888;
    msh::SurfaceCreationParameters params;
    params.name = std::string("test");
    params.size = size;
    params.pixel_format = pf;
    params.buffer_usage = mg::BufferUsage::hardware;
    auto id = mf::SurfaceId{4458};

    auto stub_input_factory = std::make_shared<StubInputFactory>();
    auto stub_input_registrar = std::make_shared<mtd::StubInputRegistrar>();
    auto null_buffer_initializer = std::make_shared<mg::NullBufferInitializer>();
    auto allocator = std::make_shared<mga::AndroidGraphicBufferAllocator>(null_buffer_initializer);
    auto buffer_stream_factory = std::make_shared<mc::BufferStreamFactory>(allocator);
    auto scene_report = mr::null_scene_report();
    auto surface_allocator = std::make_shared<ms::SurfaceAllocator>(buffer_stream_factory, stub_input_factory, std::make_shared<mtd::NullSurfaceConfigurator>(), scene_report);
    auto ss = std::make_shared<ms::SurfaceStack>(stub_input_registrar, scene_report);
    auto surface_controller = std::make_shared<ms::SurfaceController>(surface_allocator, ss);
    auto surface_source = std::make_shared<ms::SurfaceSource>(surface_controller);
    auto surface = surface_source->create_surface(nullptr, params, id, std::shared_ptr<mf::EventSink>());
    surface->allow_framedropping(true);
    auto mir_surface = as_internal_surface(surface);

    auto options = std::shared_ptr<mo::ProgramOption>();
    auto report = mr::null_display_report();
    auto internal_client = std::make_shared<mga::InternalClient>();

    int major, minor, n;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE };
    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    auto egl_display = eglGetDisplay(internal_client->egl_native_display());
    int rc = eglInitialize(egl_display, &major, &minor);
    EXPECT_EQ(EGL_TRUE, rc);

    rc = eglChooseConfig(egl_display, attribs, &egl_config, 1, &n);
    EXPECT_EQ(EGL_TRUE, rc);

    egl_surface = eglCreateWindowSurface(egl_display, egl_config, internal_client->egl_native_window(mir_surface), NULL);
    EXPECT_NE(EGL_NO_SURFACE, egl_surface);

    egl_context = eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attribs);
    EXPECT_NE(EGL_NO_CONTEXT, egl_context);

    rc = eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
    EXPECT_EQ(EGL_TRUE, rc);

    glClearColor(1.0f, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    rc = eglSwapBuffers(egl_display, egl_surface);
    EXPECT_EQ(EGL_TRUE, rc);

    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(egl_display, egl_surface);
    eglDestroyContext(egl_display, egl_context);
    eglTerminate(egl_display);
}
