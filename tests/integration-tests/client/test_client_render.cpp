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

#include "mir_test_framework/process.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir/compositor/buffer_ipc_package.h"
#include "src/server/frontend/protobuf_socket_communicator.h"
#include "mir/frontend/resource_cache.h"
#include "src/server/graphics/android/android_buffer.h"
#include "src/server/graphics/android/android_alloc_adaptor.h"

#include "mir_test/draw/android_graphics.h"
#include "mir_test/draw/patterns.h"
#include "mir_test/stub_server_tool.h"
#include "mir_test/test_protobuf_server.h"
#include "mir_test/fake_shared.h"

#include <gmock/gmock.h>

#include <thread>
#include <GLES2/gl2.h>
#include <hardware/gralloc.h>

namespace mtf = mir_test_framework;
namespace mt=mir::test;
namespace mtd=mir::test::draw;
namespace mc=mir::compositor;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

namespace
{
static int test_width  = 300;
static int test_height = 200;
}

namespace mir
{
namespace test
{
/* client code */
static void connected_callback(MirConnection *connection, void* context)
{
    MirConnection** tmp = (MirConnection**) context;
    *tmp = connection;
}
static void create_callback(MirSurface *surface, void*context)
{
    MirSurface** surf = (MirSurface**) context;
    *surf = surface;
}

static void next_callback(MirSurface *, void*)
{
}

static uint32_t pattern0 [2][2] = {{0x12345678, 0x23456789},
                                   {0x34567890, 0x45678901}};

static uint32_t pattern1 [2][2] = {{0xFFFFFFFF, 0xFFFF0000},
                                   {0xFF00FF00, 0xFF0000FF}};
struct TestClient
{

    static void sig_handle(int)
    {
    }

    static int render_single()
    {
        if (signal(SIGCONT, sig_handle) == SIG_ERR)
            return -1;
        pause();

        MirConnection* connection = NULL;
        MirSurface* surface;
        MirSurfaceParameters surface_parameters;

         /* establish connection. wait for server to come up */
        while (connection == NULL)
        {
            mir_wait_for(mir_connect("./test_socket_surface", "test_renderer",
                                         &connected_callback, &connection));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        /* make surface */
        surface_parameters.name = "testsurface";
        surface_parameters.width = test_width;
        surface_parameters.height = test_height;
        surface_parameters.pixel_format = mir_pixel_format_abgr_8888;
        mir_wait_for(mir_surface_create( connection, &surface_parameters,
                                          &create_callback, &surface));

        auto graphics_region = std::make_shared<MirGraphicsRegion>();
        /* grab a buffer*/
        mir_surface_get_graphics_region( surface, graphics_region.get());

        /* render pattern */
        mtd::DrawPatternCheckered<2,2> draw_pattern0(mt::pattern0);
        draw_pattern0.draw(graphics_region);

        mir_wait_for(mir_surface_release(surface, &create_callback, &surface));

        /* release */
        mir_connection_release(connection);
        return 0;
    }

    static int render_double()
    {
        if (signal(SIGCONT, sig_handle) == SIG_ERR)
            return -1;
        pause();

        MirConnection* connection = NULL;
        MirSurface* surface;
        MirSurfaceParameters surface_parameters;

         /* establish connection. wait for server to come up */
        while (connection == NULL)
        {
            mir_wait_for(mir_connect("./test_socket_surface", "test_renderer",
                                         &connected_callback, &connection));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        /* make surface */
        surface_parameters.name = "testsurface";
        surface_parameters.width = test_width;
        surface_parameters.height = test_height;
        surface_parameters.pixel_format = mir_pixel_format_abgr_8888;

        mir_wait_for(mir_surface_create( connection, &surface_parameters,
                                          &create_callback, &surface));

        auto graphics_region = std::make_shared<MirGraphicsRegion>();
        mir_surface_get_graphics_region( surface, graphics_region.get());
        mtd::DrawPatternCheckered<2,2> draw_pattern0(mt::pattern0);
        draw_pattern0.draw(graphics_region);

        mir_wait_for(mir_surface_next_buffer(surface, &next_callback, (void*) NULL));
        mir_surface_get_graphics_region( surface, graphics_region.get());
        mtd::DrawPatternCheckered<2,2> draw_pattern1(mt::pattern1);
        draw_pattern1.draw(graphics_region);

        mir_wait_for(mir_surface_release(surface, &create_callback, &surface));

        /* release */
        mir_connection_release(connection);
        return 0;
    }

    static int render_accelerated()
    {
        if (signal(SIGCONT, sig_handle) == SIG_ERR)
            return -1;
        pause();

        /* only use C api */
        MirConnection* connection = NULL;
        MirSurface* surface;
        MirSurfaceParameters surface_parameters;

         /* establish connection. wait for server to come up */
        while (connection == NULL)
        {
            mir_wait_for(mir_connect("./test_socket_surface", "test_renderer",
                                         &connected_callback, &connection));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        /* make surface */
        surface_parameters.name = "testsurface";
        surface_parameters.width = test_width;
        surface_parameters.height = test_height;
        surface_parameters.pixel_format = mir_pixel_format_abgr_8888;

        mir_wait_for(mir_surface_create( connection, &surface_parameters,
                                          &create_callback, &surface));

        int major, minor, n;
        EGLDisplay disp;
        EGLContext context;
        EGLSurface egl_surface;
        EGLConfig egl_config;
        EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_GREEN_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE };
        EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

        EGLNativeDisplayType native_display = (EGLNativeDisplayType)mir_connection_get_egl_native_display(connection);
        EGLNativeWindowType native_window = (EGLNativeWindowType) mir_surface_get_egl_native_window(surface);

        disp = eglGetDisplay(native_display);
        eglInitialize(disp, &major, &minor);

        eglChooseConfig(disp, attribs, &egl_config, 1, &n);
        egl_surface = eglCreateWindowSurface(disp, egl_config, native_window, NULL);
        context = eglCreateContext(disp, egl_config, EGL_NO_CONTEXT, context_attribs);
        eglMakeCurrent(disp, egl_surface, egl_surface, context);

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(disp, egl_surface);
        mir_wait_for(mir_surface_release(surface, &create_callback, &surface));

        /* release */
        mir_connection_release(connection);
        return 0;

    }

    static int render_accelerated_double()
    {
        if (signal(SIGCONT, sig_handle) == SIG_ERR)
            return -1;
        pause();

        /* only use C api */
        MirConnection* connection = NULL;
        MirSurface* surface;
        MirSurfaceParameters surface_parameters;

         /* establish connection. wait for server to come up */
        while (connection == NULL)
        {
            mir_wait_for(mir_connect("./test_socket_surface", "test_renderer",
                                         &connected_callback, &connection));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        /* make surface */
        surface_parameters.name = "testsurface";
        surface_parameters.width = test_width;
        surface_parameters.height = test_height;
        surface_parameters.pixel_format = mir_pixel_format_abgr_8888;

        mir_wait_for(mir_surface_create( connection, &surface_parameters,
                                          &create_callback, &surface));

        int major, minor, n;
        EGLDisplay disp;
        EGLContext context;
        EGLSurface egl_surface;
        EGLConfig egl_config;
        EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_GREEN_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE };
        EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

        EGLNativeDisplayType native_display = (EGLNativeDisplayType)mir_connection_get_egl_native_display(connection);
        EGLNativeWindowType native_window = (EGLNativeWindowType)mir_surface_get_egl_native_window(surface);

        disp = eglGetDisplay(native_display);
        eglInitialize(disp, &major, &minor);

        eglChooseConfig(disp, attribs, &egl_config, 1, &n);
        egl_surface = eglCreateWindowSurface(disp, egl_config, native_window, NULL);
        context = eglCreateContext(disp, egl_config, EGL_NO_CONTEXT, context_attribs);
        eglMakeCurrent(disp, egl_surface, egl_surface, context);

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(disp, egl_surface);

        glClearColor(0.0, 1.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(disp, egl_surface);

        mir_wait_for(mir_surface_release(surface, &create_callback, &surface));

        /* release */
        mir_connection_release(connection);
        return 0;

    }

    static int exit_function()
    {
        return EXIT_SUCCESS;
    }
};

/* server code */
struct StubServerGenerator : public mt::StubServerTool
{
    StubServerGenerator(const std::shared_ptr<mc::BufferIPCPackage>& pack, int id)
     : package(pack),
        next_received(false),
        next_allowed(false),
        package_id(id)
    {

    }

    void create_surface(google::protobuf::RpcController* /*controller*/,
                 const mir::protobuf::SurfaceParameters* request,
                 mir::protobuf::Surface* response,
                 google::protobuf::Closure* done)
    {
        response->mutable_id()->set_value(13); // TODO distinct numbers & tracking
        response->set_width(test_width);
        response->set_height(test_height);
        response->set_pixel_format(request->pixel_format());
        response->mutable_buffer()->set_buffer_id(package_id);
        response->mutable_buffer()->set_stride(package->stride);

        unsigned int i;
        response->mutable_buffer()->set_fds_on_side_channel(1);
        for(i=0; i<package->ipc_fds.size(); i++)
            response->mutable_buffer()->add_fd(package->ipc_fds[i]);
        for(i=0; i<package->ipc_data.size(); i++)
            response->mutable_buffer()->add_data(package->ipc_data[i]);

        std::unique_lock<std::mutex> lock(guard);
        surface_name = request->surface_name();
        wait_condition.notify_one();

        done->Run();
    }

    virtual void next_buffer(
        ::google::protobuf::RpcController* /*controller*/,
        ::mir::protobuf::SurfaceId const* /*request*/,
        ::mir::protobuf::Buffer* response,
        ::google::protobuf::Closure* done)
    {
        {
            std::unique_lock<std::mutex> lk(next_guard);
            next_received = true;
            next_cv.notify_all();

            while (!next_allowed) {
                allow_cv.wait(lk);
            }
            next_allowed = false;
        }

        response->set_buffer_id(package_id);
        unsigned int i;
        response->set_fds_on_side_channel(1);
        for(i=0; i<package->ipc_fds.size(); i++)
            response->add_fd(package->ipc_fds[i]);
        for(i=0; i<package->ipc_data.size(); i++)
            response->add_data(package->ipc_data[i]);
        response->set_stride(package->stride);

        done->Run();
    }

    void wait_on_next_buffer()
    {
        std::unique_lock<std::mutex> lk(next_guard);
        while (!next_received)
            next_cv.wait(lk);
        next_received = false;
    }

    void allow_next_continue()
    {
        std::unique_lock<std::mutex> lk(next_guard);
        next_allowed = true;
        allow_cv.notify_all();
        lk.unlock();
    }

    void set_package(const std::shared_ptr<mc::BufferIPCPackage>& pack, int id)
    {
        package = pack;
        package_id = id;
    }

    std::shared_ptr<mc::BufferIPCPackage> package;

    std::mutex next_guard;
    std::condition_variable next_cv;
    std::condition_variable allow_cv;
    bool next_received;
    bool next_allowed;

    int package_id;
};


}
}

struct TestClientIPCRender : public testing::Test
{
    /* kdub -- some of the (less thoroughly tested) android blob drivers annoyingly keep
       static state about what process they are in. Once you fork, this info is invalid,
       yet the driver uses the info and bad things happen.
       Fork all needed processes before touching the blob! */
    static void SetUpTestCase() {
        render_single_client_process = mtf::fork_and_run_in_a_different_process(
            mt::TestClient::render_single,
            mt::TestClient::exit_function);

        render_double_client_process = mtf::fork_and_run_in_a_different_process(
            mt::TestClient::render_double,
            mt::TestClient::exit_function);

        second_render_with_same_buffer_client_process
             = mtf::fork_and_run_in_a_different_process(
                            mt::TestClient::render_double,
                            mt::TestClient::exit_function);

        render_accelerated_process
             = mtf::fork_and_run_in_a_different_process(
                            mt::TestClient::render_accelerated,
                            mt::TestClient::exit_function);

        render_accelerated_process_double
             = mtf::fork_and_run_in_a_different_process(
                            mt::TestClient::render_accelerated_double,
                            mt::TestClient::exit_function);
    }

    void SetUp() {
        ASSERT_FALSE(mtd::is_surface_flinger_running());

        size = geom::Size{geom::Width{test_width}, geom::Height{test_height}};
        pf = geom::PixelFormat::abgr_8888;

        /* allocate an android buffer */
        int err;
        const hw_module_t *hw_module;
        struct alloc_device_t *alloc_device_raw;
        err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &hw_module);
        if (err < 0)
            throw std::runtime_error("Could not open hardware module");
        gralloc_open(hw_module, &alloc_device_raw);
        alloc_device = mt::fake_shared(*alloc_device_raw);
        auto alloc_adaptor = std::make_shared<mga::AndroidAllocAdaptor>(alloc_device);
        buffer_converter = std::make_shared<mtd::TestGrallocMapper>(hw_module, alloc_device.get());


        android_buffer = std::make_shared<mga::AndroidBuffer>(alloc_adaptor, size, pf);
        second_android_buffer = std::make_shared<mga::AndroidBuffer>(alloc_adaptor, size, pf);

        package = android_buffer->get_ipc_package();
        second_package = second_android_buffer->get_ipc_package();

        /* start a server */
        mock_server = std::make_shared<mt::StubServerGenerator>(package, 14);
        test_server = std::make_shared<mt::TestProtobufServer>("./test_socket_surface", mock_server);
        test_server->comm->start();
    }

    void TearDown()
    {
        test_server.reset();
        gralloc_close(alloc_device.get());
    }

    mir::protobuf::Connection response;

    std::shared_ptr<mt::TestProtobufServer> test_server;
    std::shared_ptr<mt::StubServerGenerator> mock_server;

    geom::Size size;
    geom::PixelFormat pf;
    mc::BufferID id1;
    mc::BufferID id2;
    std::shared_ptr<mtd::TestGrallocMapper> buffer_converter;
    std::shared_ptr<mtf::Process> client_process;
    std::shared_ptr<mc::BufferIPCPackage> package;
    std::shared_ptr<mc::BufferIPCPackage> second_package;
    std::shared_ptr<mga::AndroidBuffer> android_buffer;
    std::shared_ptr<mga::AndroidBuffer> second_android_buffer;
    std::shared_ptr<struct alloc_device_t> alloc_device;

    static std::shared_ptr<mtf::Process> render_single_client_process;
    static std::shared_ptr<mtf::Process> render_double_client_process;
    static std::shared_ptr<mtf::Process> second_render_with_same_buffer_client_process;
    static std::shared_ptr<mtf::Process> render_accelerated_process;
    static std::shared_ptr<mtf::Process> render_accelerated_process_double;
};
std::shared_ptr<mtf::Process> TestClientIPCRender::render_single_client_process;
std::shared_ptr<mtf::Process> TestClientIPCRender::render_double_client_process;
std::shared_ptr<mtf::Process> TestClientIPCRender::second_render_with_same_buffer_client_process;
std::shared_ptr<mtf::Process> TestClientIPCRender::render_accelerated_process;
std::shared_ptr<mtf::Process> TestClientIPCRender::render_accelerated_process_double;

TEST_F(TestClientIPCRender, test_render_single)
{
    mtd::DrawPatternCheckered<2,2> rendered_pattern(mt::pattern0);
    /* activate client */
    render_single_client_process->cont();

    /* wait for client to finish */
    EXPECT_TRUE(render_single_client_process->wait_for_termination().succeeded());

    /* check content */
    auto region = buffer_converter->get_graphic_region_from_package(package, size);
    EXPECT_TRUE(rendered_pattern.check(region));
}

TEST_F(TestClientIPCRender, test_render_double)
{
    mtd::DrawPatternCheckered<2,2> rendered_pattern0(mt::pattern0);
    mtd::DrawPatternCheckered<2,2> rendered_pattern1(mt::pattern1);
    /* activate client */
    render_double_client_process->cont();

    /* wait for next buffer */
    mock_server->wait_on_next_buffer();
    auto region = buffer_converter->get_graphic_region_from_package(package, size);
    EXPECT_TRUE(rendered_pattern0.check(region));

    mock_server->set_package(second_package, 15);

    mock_server->allow_next_continue();
    /* wait for client to finish */
    EXPECT_TRUE(render_double_client_process->wait_for_termination().succeeded());

    auto second_region = buffer_converter->get_graphic_region_from_package(second_package, size);
    EXPECT_TRUE(rendered_pattern1.check(second_region));
}

TEST_F(TestClientIPCRender, test_second_render_with_same_buffer)
{
    mtd::DrawPatternCheckered<2,2> rendered_pattern(mt::pattern1);
    /* activate client */
    second_render_with_same_buffer_client_process->cont();

    /* wait for next buffer */
    mock_server->wait_on_next_buffer();
    mock_server->allow_next_continue();

    /* wait for client to finish */
    EXPECT_TRUE(second_render_with_same_buffer_client_process->wait_for_termination().succeeded());

    /* check content */
    auto region = buffer_converter->get_graphic_region_from_package(package, size);
    EXPECT_TRUE(rendered_pattern.check(region));
}

TEST_F(TestClientIPCRender, test_accelerated_render)
{
    mtd::DrawPatternSolid red_pattern(0xFF0000FF);

    /* activate client */
    render_accelerated_process->cont();

    /* wait for next buffer */
    mock_server->wait_on_next_buffer();
    mock_server->allow_next_continue();

    /* wait for client to finish */
    EXPECT_TRUE(render_accelerated_process->wait_for_termination().succeeded());

    /* check content */
    auto region = buffer_converter->get_graphic_region_from_package(package, size);
    EXPECT_TRUE(red_pattern.check(region));
}

TEST_F(TestClientIPCRender, test_accelerated_render_double)
{
    mtd::DrawPatternSolid red_pattern(0xFF0000FF);
    mtd::DrawPatternSolid green_pattern(0xFF00FF00);
    /* activate client */
    render_accelerated_process_double->cont();

    /* wait for next buffer */
    mock_server->wait_on_next_buffer();
    mock_server->set_package(second_package, 15);
    mock_server->allow_next_continue();

    mock_server->wait_on_next_buffer();
    mock_server->allow_next_continue();

    /* wait for client to finish */
    EXPECT_TRUE(render_accelerated_process_double->wait_for_termination().succeeded());

    /* check content */
    auto region = buffer_converter->get_graphic_region_from_package(package, size);
    EXPECT_TRUE(red_pattern.check(region));

    auto second_region = buffer_converter->get_graphic_region_from_package(second_package, size);
    EXPECT_TRUE(green_pattern.check(second_region));
}
