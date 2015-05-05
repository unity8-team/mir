/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/headless_in_process_server.h"
#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_framework/stub_platform_helpers.h"
#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_framework/any_surface.h"
#include "mir_test/validity_matchers.h"
#include "mir_test/fd_utils.h"
#include "mir_test_framework/udev_environment.h"
#include "mir_test/signal.h"
#include "mir_test/auto_unblock_thread.h"

#include "src/include/client/mir/client_buffer.h"

#include "mir_protobuf.pb.h"

#ifdef ANDROID
/*
 * MirNativeBuffer for Android is defined opaquely, but we now depend on
 * it having width and height fields, for all platforms. So need definition...
 */
#include <system/window.h>  // for ANativeWindowBuffer AKA MirNativeBuffer
#endif

#include <boost/throw_exception.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

namespace mf = mir::frontend;
namespace mc = mir::compositor;
namespace mcl = mir::client;
namespace mtf = mir_test_framework;
namespace mt = mir::test;

namespace
{
struct ClientLibrary : mtf::HeadlessInProcessServer
{
    std::set<MirSurface*> surfaces;
    MirConnection* connection = nullptr;
    MirSurface* surface  = nullptr;
    int buffers = 0;
    mtf::UdevEnvironment mock_devices;

    ClientLibrary()
    {
        mock_devices.add_standard_device("laptop-keyboard");
    }

    static void connection_callback(MirConnection* connection, void* context)
    {
        ClientLibrary* config = reinterpret_cast<ClientLibrary*>(context);
        config->connected(connection);
    }

    static void create_surface_callback(MirSurface* surface, void* context)
    {
        ClientLibrary* config = reinterpret_cast<ClientLibrary*>(context);
        config->surface_created(surface);
    }

    static void next_buffer_callback(MirBufferStream* bs, void* context)
    {
        ClientLibrary* config = reinterpret_cast<ClientLibrary*>(context);
        config->next_buffer(bs);
    }

    static void release_surface_callback(MirSurface* surface, void* context)
    {
        ClientLibrary* config = reinterpret_cast<ClientLibrary*>(context);
        config->surface_released(surface);
    }

    virtual void connected(MirConnection* new_connection)
    {
        connection = new_connection;
    }

    virtual void surface_created(MirSurface* new_surface)
    {
        surfaces.insert(new_surface);
        surface = new_surface;
    }

    virtual void next_buffer(MirBufferStream*)
    {
        ++buffers;
    }

    void surface_released(MirSurface* old_surface)
    {
        surfaces.erase(old_surface);
        surface = NULL;
    }

    MirSurface* any_surface()
    {
        return *surfaces.begin();
    }

    size_t current_surface_count()
    {
        return surfaces.size();
    }

    static void nosey_thread(MirSurface *surf)
    {
        for (int i = 0; i < 10; i++)
        {
            mir_wait_for_one(mir_surface_set_state(surf,
                                            mir_surface_state_maximized));
            mir_wait_for_one(mir_surface_set_state(surf,
                                            mir_surface_state_restored));
            mir_wait_for_one(mir_surface_set_state(surf,
                                            mir_surface_state_fullscreen));
            mir_wait_for_one(mir_surface_set_state(surf,
                                            mir_surface_state_minimized));
        }
    }
    
    mtf::UsingStubClientPlatform using_stub_client_platform;
};
}

using namespace testing;

TEST_F(ClientLibrary, client_library_connects_and_disconnects)
{
    MirWaitHandle* wh = mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this);
    EXPECT_THAT(wh, NotNull());
    mir_wait_for(wh);

    ASSERT_THAT(connection, NotNull());
    EXPECT_TRUE(mir_connection_is_valid(connection));
    EXPECT_THAT(mir_connection_get_error_message(connection), StrEq(""));

    mir_connection_release(connection);
}

TEST_F(ClientLibrary, synchronous_connection)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    EXPECT_THAT(connection, NotNull());
    EXPECT_TRUE(mir_connection_is_valid(connection));
    EXPECT_THAT(mir_connection_get_error_message(connection), StrEq(""));

    mir_connection_release(connection);
}

TEST_F(ClientLibrary, creates_surface)
{
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    int request_width = 640, request_height = 480;
    MirPixelFormat request_format = mir_pixel_format_abgr_8888;
    MirBufferUsage request_buffer_usage = mir_buffer_usage_hardware;

    auto spec = mir_connection_create_spec_for_normal_surface(connection, request_width,
                                                              request_height, request_format);
    mir_surface_spec_set_buffer_usage(spec, request_buffer_usage);
    surface = mir_surface_create_sync(spec);
    mir_surface_spec_release(spec);

    ASSERT_THAT(surface, NotNull());
    EXPECT_TRUE(mir_surface_is_valid(surface));
    EXPECT_THAT(mir_surface_get_error_message(surface), StrEq(""));

    MirSurfaceParameters response_params;
    mir_surface_get_parameters(surface, &response_params);
    EXPECT_EQ(request_width, response_params.width);
    EXPECT_EQ(request_height, response_params.height);
    EXPECT_EQ(request_format, response_params.pixel_format);
    EXPECT_EQ(request_buffer_usage, response_params.buffer_usage);

    mir_wait_for(mir_surface_release( surface, release_surface_callback, this));
    mir_connection_release(connection);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

TEST_F(ClientLibrary, can_set_surface_types)
{
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 640, 480, mir_pixel_format_abgr_8888);

    mir_wait_for(mir_surface_create(spec, create_surface_callback, this));
    mir_surface_spec_release(spec);

    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_normal));

    mir_wait_for(mir_surface_set_type(surface, mir_surface_type_freestyle));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_freestyle));

    mir_wait_for(mir_surface_set_type(surface, static_cast<MirSurfaceType>(999)));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_freestyle));

    mir_wait_for(mir_surface_set_type(surface, mir_surface_type_dialog));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_dialog));

    mir_wait_for(mir_surface_set_type(surface, static_cast<MirSurfaceType>(888)));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_dialog));

    // Stress-test synchronization logic with some flooding
    for (int i = 0; i < 100; i++)
    {
        mir_surface_set_type(surface, mir_surface_type_normal);
        mir_surface_set_type(surface, mir_surface_type_utility);
        mir_surface_set_type(surface, mir_surface_type_dialog);
        mir_surface_set_type(surface, mir_surface_type_gloss);
        mir_surface_set_type(surface, mir_surface_type_freestyle);
        mir_wait_for(mir_surface_set_type(surface, mir_surface_type_menu));
        ASSERT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_menu));
    }

    mir_wait_for(mir_surface_release(surface, release_surface_callback, this));
    mir_connection_release(connection);
}
#pragma GCC diagnostic pop

TEST_F(ClientLibrary, can_set_surface_state)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 640, 480, mir_pixel_format_abgr_8888);

    mir_wait_for(mir_surface_create(spec, create_surface_callback, this));
    mir_surface_spec_release(spec);

    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_restored));

    mir_wait_for(mir_surface_set_state(surface, mir_surface_state_fullscreen));
    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_fullscreen));

    mir_wait_for(mir_surface_set_state(surface, static_cast<MirSurfaceState>(999)));
    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_fullscreen));

    mir_wait_for(mir_surface_set_state(surface, mir_surface_state_horizmaximized));
    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_horizmaximized));

    mir_wait_for(mir_surface_set_state(surface, static_cast<MirSurfaceState>(888)));
    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_horizmaximized));

    // Stress-test synchronization logic with some flooding
    for (int i = 0; i < 100; i++)
    {
        mir_surface_set_state(surface, mir_surface_state_maximized);
        mir_surface_set_state(surface, mir_surface_state_restored);
        mir_wait_for(mir_surface_set_state(surface, mir_surface_state_fullscreen));
        ASSERT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_fullscreen));
    }

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, receives_surface_dpi_value)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 640, 480, mir_pixel_format_abgr_8888);

    surface = mir_surface_create_sync(spec);
    mir_surface_spec_release(spec);

    // Expect zero (not wired up to detect the physical display yet)
    EXPECT_THAT(mir_surface_get_dpi(surface), Eq(0));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

#ifndef ANDROID
TEST_F(ClientLibrary, surface_scanout_flag_toggles)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 1280, 1024, mir_pixel_format_abgr_8888);
    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_hardware);

    surface = mir_surface_create_sync(spec);

    MirNativeBuffer *native;
    auto bs = mir_surface_get_buffer_stream(surface);
    mir_buffer_stream_get_current_buffer(bs, &native);
    EXPECT_TRUE(native->flags & mir_buffer_flag_can_scanout);
    mir_buffer_stream_swap_buffers_sync(bs);
    EXPECT_TRUE(native->flags & mir_buffer_flag_can_scanout);
    mir_surface_release_sync(surface);

    mir_surface_spec_set_width(spec, 100);
    mir_surface_spec_set_height(spec, 100);

    surface = mir_surface_create_sync(spec);
    bs = mir_surface_get_buffer_stream(surface);
    mir_buffer_stream_get_current_buffer(bs, &native);
    EXPECT_FALSE(native->flags & mir_buffer_flag_can_scanout);
    mir_buffer_stream_swap_buffers_sync(bs);
    EXPECT_FALSE(native->flags & mir_buffer_flag_can_scanout);
    mir_surface_release_sync(surface);


    mir_surface_spec_set_width(spec, 800);
    mir_surface_spec_set_height(spec, 600);
    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_software);

    surface = mir_surface_create_sync(spec);
    bs = mir_surface_get_buffer_stream(surface);
    mir_buffer_stream_get_current_buffer(bs, &native);
    EXPECT_FALSE(native->flags & mir_buffer_flag_can_scanout);
    mir_buffer_stream_swap_buffers_sync(bs);
    EXPECT_FALSE(native->flags & mir_buffer_flag_can_scanout);
    mir_surface_release_sync(surface);

    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_hardware);

    surface = mir_surface_create_sync(spec);
    bs = mir_surface_get_buffer_stream(surface);
    mir_buffer_stream_get_current_buffer(bs, &native);
    EXPECT_TRUE(native->flags & mir_buffer_flag_can_scanout);
    mir_buffer_stream_swap_buffers_sync(bs);
    EXPECT_TRUE(native->flags & mir_buffer_flag_can_scanout);
    mir_surface_release_sync(surface);

    mir_surface_spec_release(spec);
    mir_connection_release(connection);
}
#endif

#ifdef ANDROID
// Mir's Android test infrastructure isn't quite ready for this yet.
TEST_F(ClientLibrary, DISABLED_gets_buffer_dimensions)
#else
TEST_F(ClientLibrary, gets_buffer_dimensions)
#endif
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 0, 0, mir_pixel_format_abgr_8888);
    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_hardware);

    struct {int width, height;} const sizes[] =
    {
        {12, 34},
        {56, 78},
        {90, 21},
    };

    for (auto const& size : sizes)
    {
        mir_surface_spec_set_width(spec, size.width);
        mir_surface_spec_set_height(spec, size.height);

        surface = mir_surface_create_sync(spec);
        auto bs = mir_surface_get_buffer_stream(surface);

        MirNativeBuffer *native = NULL;
        mir_buffer_stream_get_current_buffer(bs, &native);
        ASSERT_THAT(native, NotNull());
        EXPECT_THAT(native->width, Eq(size.width));
        ASSERT_THAT(native->height, Eq(size.height));

        mir_buffer_stream_swap_buffers_sync(bs);
        mir_buffer_stream_get_current_buffer(bs, &native);
        ASSERT_THAT(native, NotNull());
        EXPECT_THAT(native->width, Eq(size.width));
        ASSERT_THAT(native->height, Eq(size.height));

        mir_surface_release_sync(surface);
    }

    mir_surface_spec_release(spec);
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, creates_multiple_surfaces)
{
    int const n_surfaces = 13;
    size_t old_surface_count = 0;

    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    auto const spec =
        mir_connection_create_spec_for_normal_surface(
            connection, 640, 480, mir_pixel_format_abgr_8888);

    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_hardware);
    for (int i = 0; i != n_surfaces; ++i)
    {
        old_surface_count = current_surface_count();

        mir_wait_for(mir_surface_create(spec, create_surface_callback, this));

        ASSERT_THAT(current_surface_count(), Eq(old_surface_count + 1));
    }
    for (int i = 0; i != n_surfaces; ++i)
    {
        old_surface_count = current_surface_count();

        ASSERT_THAT(old_surface_count, Ne(0u));

        MirSurface * surface = any_surface();
        mir_wait_for(mir_surface_release( surface, release_surface_callback, this));

        ASSERT_THAT(current_surface_count(), Eq(old_surface_count - 1));
    }

    mir_surface_spec_release(spec);
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, client_library_accesses_and_advances_buffers)
{
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    surface = mtf::make_any_surface(connection);

    buffers = 0;
    mir_wait_for(mir_buffer_stream_swap_buffers(mir_surface_get_buffer_stream(surface), next_buffer_callback, this));
    EXPECT_THAT(buffers, Eq(1));

    mir_wait_for(mir_surface_release(surface, release_surface_callback, this));

    ASSERT_THAT(surface, IsNull());

    mir_connection_release(connection);
}

TEST_F(ClientLibrary, fully_synchronous_client)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    surface = mtf::make_any_surface(connection);

    mir_buffer_stream_swap_buffers_sync(mir_surface_get_buffer_stream(surface));
    EXPECT_TRUE(mir_surface_is_valid(surface));
    EXPECT_STREQ(mir_surface_get_error_message(surface), "");

    mir_surface_release_sync(surface);

    EXPECT_TRUE(mir_connection_is_valid(connection));
    EXPECT_STREQ("", mir_connection_get_error_message(connection));
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, highly_threaded_client)
{
    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    surface = mtf::make_any_surface(connection);

    std::thread a(nosey_thread, surface);
    std::thread b(nosey_thread, surface);
    std::thread c(nosey_thread, surface);

    a.join();
    b.join();
    c.join();

    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_fullscreen));

    mir_surface_release_sync(surface);

    EXPECT_TRUE(mir_connection_is_valid(connection));
    EXPECT_THAT(mir_connection_get_error_message(connection), StrEq(""));
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, accesses_platform_package)
{
    using namespace testing;
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    MirPlatformPackage platform_package;
    ::memset(&platform_package, -1, sizeof(platform_package));

    mir_connection_get_platform(connection, &platform_package);
    EXPECT_THAT(platform_package, mtf::IsStubPlatformPackage());

    mir_connection_release(connection);
}

TEST_F(ClientLibrary, accesses_display_info)
{
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    auto configuration = mir_connection_create_display_config(connection);
    ASSERT_THAT(configuration, NotNull());
    ASSERT_GT(configuration->num_outputs, 0u);
    ASSERT_THAT(configuration->outputs, NotNull());
    for (auto i=0u; i < configuration->num_outputs; i++)
    {
        MirDisplayOutput* disp = &configuration->outputs[i];
        ASSERT_THAT(disp, NotNull());
        EXPECT_GE(disp->num_modes, disp->current_mode);
        EXPECT_GE(disp->num_output_formats, disp->current_format);
    }

    mir_display_config_destroy(configuration);
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, MultiSurfaceClientTracksBufferFdsCorrectly)
{
    mir_wait_for(mir_connect(new_connection().c_str(), __PRETTY_FUNCTION__, connection_callback, this));

    auto const surf_one = mtf::make_any_surface(connection);
    auto const surf_two = mtf::make_any_surface(connection);

    ASSERT_THAT(surf_one, NotNull());
    ASSERT_THAT(surf_two, NotNull());

    buffers = 0;

    while (buffers < 1024)
    {
        mir_buffer_stream_swap_buffers_sync(
            mir_surface_get_buffer_stream(surf_one));
        mir_buffer_stream_swap_buffers_sync(
            mir_surface_get_buffer_stream(surf_two));

        buffers++;
    }

    /* We should not have any stray fds hanging around.
       Test this by trying to open a new one */
    int canary_fd;
    canary_fd = open("/dev/null", O_RDONLY);

    ASSERT_THAT(canary_fd, Gt(0)) << "Failed to open canary file descriptor: "<< strerror(errno);
    EXPECT_THAT(canary_fd, Lt(1024));

    close(canary_fd);

    mir_wait_for(mir_surface_release(surf_one, release_surface_callback, this));
    mir_wait_for(mir_surface_release(surf_two, release_surface_callback, this));

    ASSERT_THAT(current_surface_count(), testing::Eq(0));

    mir_connection_release(connection);
}

/* TODO: Our stub platform support is a bit terrible.
 *
 * These acceptance tests accidentally work on mesa because the mesa client
 * platform doesn't validate any of its input and we don't touch anything that requires
 * syscalls.
 *
 * The Android client platform *does* care about its input, and so the fact that it's
 * trying to marshall stub buffers causes crashes.
 */

#ifndef ANDROID
TEST_F(ClientLibrary, create_simple_normal_surface_from_spec)
#else
TEST_F(ClientLibrary, DISABLED_create_simple_normal_surface_from_spec)
#endif
{
    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    int const width{800}, height{600};
    MirPixelFormat const format{mir_pixel_format_bgr_888};
    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      width, height,
                                                                      format);

    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    MirNativeBuffer* native_buffer;
    mir_buffer_stream_get_current_buffer(
        mir_surface_get_buffer_stream(surface), &native_buffer);

    EXPECT_THAT(native_buffer->width, Eq(width));
    EXPECT_THAT(native_buffer->height, Eq(height));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_normal));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

#ifndef ANDROID
TEST_F(ClientLibrary, create_simple_normal_surface_from_spec_async)
#else
TEST_F(ClientLibrary, DISABLED_create_simple_normal_surface_from_spec_async)
#endif
{
    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    int const width{800}, height{600};
    MirPixelFormat const format{mir_pixel_format_xbgr_8888};
    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      width, height,
                                                                      format);

    mir_wait_for(mir_surface_create(surface_spec, create_surface_callback, this));
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    MirNativeBuffer* native_buffer;
    mir_buffer_stream_get_current_buffer(
        mir_surface_get_buffer_stream(surface), &native_buffer);

    EXPECT_THAT(native_buffer->width, Eq(width));
    EXPECT_THAT(native_buffer->height, Eq(height));
    EXPECT_THAT(mir_surface_get_type(surface), Eq(mir_surface_type_normal));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

#ifndef ANDROID
TEST_F(ClientLibrary, can_specify_all_normal_surface_parameters_from_spec)
#else
TEST_F(ClientLibrary, DISABLED_can_specify_all_normal_surface_parameters_from_spec)
#endif
{
    using namespace testing;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      800, 600,
                                                                      mir_pixel_format_bgr_888);

    char const* name = "The magnificent Dandy Warhols";
    EXPECT_TRUE(mir_surface_spec_set_name(surface_spec, name));

    int const width{999}, height{555};
    EXPECT_TRUE(mir_surface_spec_set_width(surface_spec, width));
    EXPECT_TRUE(mir_surface_spec_set_height(surface_spec, height));

    MirPixelFormat const pixel_format{mir_pixel_format_argb_8888};
    EXPECT_TRUE(mir_surface_spec_set_pixel_format(surface_spec, pixel_format));

    MirBufferUsage const buffer_usage{mir_buffer_usage_hardware};
    EXPECT_TRUE(mir_surface_spec_set_buffer_usage(surface_spec, buffer_usage));

    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

#ifndef ANDROID
TEST_F(ClientLibrary, set_fullscreen_on_output_makes_fullscreen_surface)
#else
TEST_F(ClientLibrary, DISABLED_set_fullscreen_on_output_makes_fullscreen_surface)
#endif
{
    using namespace testing;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      780, 555,
                                                                      mir_pixel_format_xbgr_8888);

    // We need to specify a valid output id, so we need to find which ones are valid...
    auto configuration = mir_connection_create_display_config(connection);
    ASSERT_THAT(configuration->num_outputs, Ge(1));

    auto const requested_output = configuration->outputs[0];

    mir_surface_spec_set_fullscreen_on_output(surface_spec, requested_output.output_id);

    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    MirNativeBuffer* native_buffer;
    mir_buffer_stream_get_current_buffer(
        mir_surface_get_buffer_stream(surface), &native_buffer);

    EXPECT_THAT(native_buffer->width,
                Eq(requested_output.modes[requested_output.current_mode].horizontal_resolution));
    EXPECT_THAT(native_buffer->height,
                Eq(requested_output.modes[requested_output.current_mode].vertical_resolution));

// TODO: This is racy. Fix in subsequent "send all the things on construction" branch
//    EXPECT_THAT(mir_surface_get_state(surface), Eq(mir_surface_state_fullscreen));

    mir_surface_release_sync(surface);
    mir_display_config_destroy(configuration);
    mir_connection_release(connection);
}

/*
 * We don't (yet) use a stub client platform, so can't rely on its behaviour
 * in these tests.
 *
 * At the moment, enabling them will either spuriously pass (hardware buffer, mesa)
 * or crash (everything else).
 */
TEST_F(ClientLibrary, DISABLED_can_create_buffer_usage_hardware_surface)
{
    using namespace testing;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      800, 600,
                                                                      mir_pixel_format_bgr_888);

    MirBufferUsage const buffer_usage{mir_buffer_usage_hardware};
    EXPECT_TRUE(mir_surface_spec_set_buffer_usage(surface_spec, buffer_usage));

    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    MirNativeBuffer* native_buffer;
    // We use the fact that our stub client platform returns NULL if asked for a native
    // buffer on a surface with mir_buffer_usage_software set.
    mir_buffer_stream_get_current_buffer(
        mir_surface_get_buffer_stream(surface), &native_buffer);

    EXPECT_THAT(native_buffer, Not(Eq(nullptr)));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

TEST_F(ClientLibrary, DISABLED_can_create_buffer_usage_software_surface)
{
    using namespace testing;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      800, 600,
                                                                      mir_pixel_format_bgr_888);

    MirBufferUsage const buffer_usage{mir_buffer_usage_software};
    EXPECT_TRUE(mir_surface_spec_set_buffer_usage(surface_spec, buffer_usage));

    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surface, IsValid());

    MirGraphicsRegion graphics_region;
    // We use the fact that our stub client platform returns a NULL vaddr if
    // asked to map a hardware buffer.
    mir_buffer_stream_get_graphics_region(
        mir_surface_get_buffer_stream(surface), &graphics_region);

    EXPECT_THAT(graphics_region.vaddr, Not(Eq(nullptr)));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

namespace
{
void dummy_event_handler_one(MirSurface*, MirEvent const*, void*)
{
}

void dummy_event_handler_two(MirSurface*, MirEvent const*, void*)
{
}
}

/*
 * Regression test for LP: 1438160
 */
TEST_F(ClientLibrary, can_change_event_delegate)
{
    using namespace testing;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      800, 600,
                                                                      mir_pixel_format_argb_8888);
    auto surface = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    ASSERT_THAT(surface, IsValid());

    /* TODO: When provide-event-fd lands, change this into a better test that actually
     * tests that the correct event handler is called.
     *
     * Without manual dispatch, it's racy to try and test that.
     */
    mir_surface_set_event_handler(surface, &dummy_event_handler_one, nullptr);
    mir_surface_set_event_handler(surface, &dummy_event_handler_two, nullptr);

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

namespace
{
struct ThreadTrackingCallbacks
{
    ThreadTrackingCallbacks()
    {
    }

    void current_thread_is_event_thread()
    {
        client_thread = pthread_self();
    }

    static void connection_ready(MirConnection* /*connection*/, void* ctx)
    {
        auto data = reinterpret_cast<ThreadTrackingCallbacks*>(ctx);
        EXPECT_EQ(pthread_self(), data->client_thread);
        data->connection_ready_called.raise();
    }

    static void event_delegate(MirSurface* /*surf*/, MirEvent const* event, void* ctx)
    {
        auto data = reinterpret_cast<ThreadTrackingCallbacks*>(ctx);

        EXPECT_THAT(pthread_self(), Eq(data->client_thread));
        data->event_received.raise();
        if (mir_event_get_type(event) == mir_event_type_input)
        {
            data->input_event_received.raise();
        }
    }

    static void surface_created(MirSurface* surf, void* ctx)
    {
        auto data = reinterpret_cast<ThreadTrackingCallbacks*>(ctx);
        EXPECT_THAT(pthread_self(), Eq(data->client_thread));
        data->surf = surf;

        mir_surface_set_event_handler(data->surf, &ThreadTrackingCallbacks::event_delegate, data);
    }

    static void swap_buffers_complete(MirBufferStream* /*stream*/, void* ctx)
    {
        auto data = reinterpret_cast<ThreadTrackingCallbacks*>(ctx);
        EXPECT_EQ(pthread_self(), data->client_thread);
        data->buffers_swapped.raise();
    }

    pthread_t client_thread{0};
    MirSurface* surf{nullptr};
    mt::Signal buffers_swapped;
    mt::Signal connection_ready_called;
    mt::Signal event_received;
    mt::Signal input_event_received;
};

void pump_eventloop_until(MirConnection* connection, std::function<bool()> predicate, std::chrono::steady_clock::time_point timeout)
{
    using namespace std::literals::chrono_literals;

    auto fd = mir::Fd{mir::IntOwnedFd{mir_connection_get_event_fd(connection)}};

    while (!predicate() && (std::chrono::steady_clock::now() < timeout))
    {
        if (mt::fd_becomes_readable(fd, 10ms))
        {
            mir_connection_dispatch(connection);
        }
    }
    if (!predicate())
    {
        BOOST_THROW_EXCEPTION((std::runtime_error{"Timeout waiting for state change"}));
    }
}

class EventDispatchThread
{
public:
    template<typename Period, typename Rep>
    EventDispatchThread(MirConnection* connection,
                        ThreadTrackingCallbacks& data,
                        std::chrono::duration<Period, Rep> timeout)
        : runner{[this]() { shutdown.raise(); },
                 [this, connection, &data, timeout]()
                 {
                     using namespace std::literals::chrono_literals;

                     data.current_thread_is_event_thread();

                     auto const end_time = std::chrono::steady_clock::now() + timeout;

                     pump_eventloop_until(connection, [this]() { return shutdown.raised(); }, end_time);
                     mir_connection_release(connection);
                 }
                }
    {
    }

private:
    mt::Signal shutdown;
    mt::AutoUnblockThread runner;
};

}

TEST_F(ClientLibrary, manual_dispatch_handles_callbacks_in_parent_thread)
{
    using namespace std::literals::chrono_literals;

    ThreadTrackingCallbacks data;

    auto connection = mir_connect_with_manual_dispatch(new_connection().c_str(), __PRETTY_FUNCTION__, &ThreadTrackingCallbacks::connection_ready, &data);

    ASSERT_THAT(connection, Ne(nullptr));
    EventDispatchThread event_thread{connection, data, 10min};

    EXPECT_TRUE(data.connection_ready_called.wait_for(5min));
    ASSERT_THAT(connection, IsValid());


    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      233, 355,
                                                                      mir_pixel_format_argb_8888);
    auto surf_wh = mir_surface_create(surface_spec,
                                      &ThreadTrackingCallbacks::surface_created,
                                      &data);
    mir_surface_spec_release(surface_spec);

    mir_wait_for(surf_wh);
    EXPECT_THAT(data.surf, IsValid());

    auto buffer_stream = mir_surface_get_buffer_stream(data.surf);
    auto swap_wh = mir_buffer_stream_swap_buffers(buffer_stream, ThreadTrackingCallbacks::swap_buffers_complete, &data);

    mir_wait_for(swap_wh);
    EXPECT_TRUE(data.buffers_swapped.raised());

    mir_surface_release_sync(data.surf);
    // EventDispatchThread releases the connection for us.
}

TEST_F(ClientLibrary, manual_dispatch_handles_events_in_parent_thread)
{
    using namespace testing;
    using namespace std::literals::chrono_literals;

    ThreadTrackingCallbacks data;

    connection = mir_connect_with_manual_dispatch(new_connection().c_str(), __PRETTY_FUNCTION__, &ThreadTrackingCallbacks::connection_ready, &data);

    ASSERT_THAT(connection, Ne(nullptr));
    EventDispatchThread event_thread{connection, data, 10min};

    EXPECT_TRUE(data.connection_ready_called.wait_for(5min));
    ASSERT_THAT(connection, IsValid());

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      233, 355,
                                                                      mir_pixel_format_argb_8888);
    auto surf_wh = mir_surface_create(surface_spec,
                                      &ThreadTrackingCallbacks::surface_created,
                                      &data);
    mir_surface_spec_release(surface_spec);


    mir_wait_for(surf_wh);
    EXPECT_THAT(data.surf, IsValid());

    // We need to swap buffers so that the surface is fully realised and
    // will be a valid focus target.
    //
    // The shell will not focus a surface with no content.
    auto buffer_stream = mir_surface_get_buffer_stream(data.surf);
    mir_buffer_stream_swap_buffers_sync(buffer_stream);

    mir_surface_set_state(data.surf, mir_surface_state_fullscreen);

    EXPECT_TRUE(data.event_received.wait_for(5min));

    ASSERT_THAT(mir_surface_get_focus(data.surf), Eq(mir_surface_focused));

    mock_devices.load_device_evemu("laptop-keyboard-hello");

    EXPECT_TRUE(data.input_event_received.wait_for(5min));

    mir_surface_release_sync(data.surf);
    // EventDispatchThread releases the connection for us.
}

namespace
{
struct SignalPair
{
    mir::test::Signal now_blocking;
    mir::test::Signal event_received;
};

void notifying_event_handler(MirSurface*, MirEvent const* ev, void* ctx)
{
    auto signal_pair = *reinterpret_cast<std::shared_ptr<SignalPair>*>(ctx);
    // We trigger an input event once we've noticed the surface callback is blocking
    // so we need to only raise the flag on an input event; otherwise we may spuriously
    // fail if we receive a surface event (like the focus event) before we hit
    // the wait in blocking_surface_callback()
    if (mir_event_get_type(ev) == mir_event_type_input)
    {
        signal_pair->event_received.raise();
    }
}

void blocking_buffer_stream_callback(MirBufferStream*, void* ctx)
{
    auto signal_pair = *reinterpret_cast<std::shared_ptr<SignalPair>*>(ctx);
    signal_pair->now_blocking.raise();
    EXPECT_TRUE(signal_pair->event_received.wait_for(std::chrono::seconds{5}));
}
}

TEST_F(ClientLibrary, rpc_blocking_doesnt_block_event_delivery_with_auto_dispatch)
{
    using namespace testing;

    connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);

    ASSERT_THAT(connection, IsValid());

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      233, 355,
                                                                      mir_pixel_format_argb_8888);

    auto surf = mir_surface_create_sync(surface_spec);
    mir_surface_spec_release(surface_spec);

    EXPECT_THAT(surf, IsValid());

    auto signal_pair = std::make_shared<SignalPair>();
    mir_surface_set_event_handler(surf, &notifying_event_handler, &signal_pair);

    auto buffer_stream = mir_surface_get_buffer_stream(surf);
    auto wh = mir_buffer_stream_swap_buffers(buffer_stream, &blocking_buffer_stream_callback, &signal_pair);

    EXPECT_TRUE(signal_pair->now_blocking.wait_for(std::chrono::seconds{5}));
    EXPECT_FALSE(signal_pair->event_received.raised());

    mock_devices.load_device_evemu("laptop-keyboard-hello");

    EXPECT_TRUE(signal_pair->event_received.wait_for(std::chrono::seconds{5}));

    mir_wait_for(wh);
    mir_surface_release_sync(surf);
    mir_connection_release(connection);
}

namespace
{
void async_creation_completed(MirSurface* surf, void* ctx)
{
    auto called = reinterpret_cast<bool*>(ctx);
    mir_surface_release_sync(surf);

    *called = true;
}
}

TEST_F(ClientLibrary, sync_call_completes_before_previous_undispatched_call)
{
    using namespace std::literals::chrono_literals;
    using namespace testing;

    auto timeout = std::chrono::steady_clock::now() + 60s;

    ThreadTrackingCallbacks data;

    auto connection = mir_connect_with_manual_dispatch(new_connection().c_str(), __PRETTY_FUNCTION__, &ThreadTrackingCallbacks::connection_ready, &data);
    ASSERT_THAT(connection, Ne(nullptr));

    pump_eventloop_until(connection, [connection]() { return mir_connection_is_valid(connection); }, timeout);

    bool async_call_completed{false};

    auto surface_spec = mir_connection_create_spec_for_normal_surface(connection,
                                                                      233, 355,
                                                                      mir_pixel_format_argb_8888);
    mir_surface_create(surface_spec, &async_creation_completed, &async_call_completed);

    EXPECT_FALSE(async_call_completed);
    auto surf = mir_surface_create_sync(surface_spec);

    EXPECT_THAT(surf, IsValid());
    EXPECT_FALSE(async_call_completed);

    mir_surface_release_sync(surf);
    EXPECT_FALSE(async_call_completed);

    pump_eventloop_until(connection, [&async_call_completed]() { return async_call_completed;}, timeout);

    mir_connection_release(connection);
}
