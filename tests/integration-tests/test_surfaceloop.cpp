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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/graphics/buffer_properties.h"
#include "mir/graphics/buffer_id.h"
#include "mir/graphics/buffer_basic.h"
#include "mir/graphics/display.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/stub_buffer_allocator.h"
#include "mir_test_doubles/null_platform.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/stub_display_buffer.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mir_test/gmock_fixes.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;

namespace
{
char const* const mir_test_socket = mtf::test_socket_file().c_str();

geom::Size const size{640, 480};
MirPixelFormat const format{mir_pixel_format_abgr_8888};
mg::BufferUsage const usage{mg::BufferUsage::hardware};
mg::BufferProperties const buffer_properties{size, format, usage};


class MockGraphicBufferAllocator : public mtd::StubBufferAllocator
{
 public:
    MockGraphicBufferAllocator()
    {
        using testing::_;
        ON_CALL(*this, alloc_buffer(_))
            .WillByDefault(testing::Invoke(this, &MockGraphicBufferAllocator::on_create_swapper));
    }

    MOCK_METHOD1(
        alloc_buffer,
        std::shared_ptr<mg::Buffer> (mg::BufferProperties const&));


    std::shared_ptr<mg::Buffer> on_create_swapper(mg::BufferProperties const&)
    {
        return std::make_shared<mtd::StubBuffer>(::buffer_properties);
    }

    ~MockGraphicBufferAllocator() noexcept {}
};

}

namespace mir
{
namespace
{

class StubDisplay : public mtd::NullDisplay
{
public:
    StubDisplay()
        : display_buffer{geom::Rectangle{geom::Point{0,0}, geom::Size{1600,1600}}}
    {
    }

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f) override
    {
        f(display_buffer);
    }

private:
    mtd::StubDisplayBuffer display_buffer;
};

struct SurfaceSync
{
    SurfaceSync() :
        surface(0)
    {
    }

    void surface_created(MirSurface * new_surface)
    {
        std::unique_lock<std::mutex> lock(guard);
        surface = new_surface;
        wait_condition.notify_all();
    }

    void surface_released(MirSurface * /*released_surface*/)
    {
        std::unique_lock<std::mutex> lock(guard);
        surface = NULL;
        wait_condition.notify_all();
    }

    void wait_for_surface_create()
    {
        std::unique_lock<std::mutex> lock(guard);
        while (!surface)
            wait_condition.wait(lock);
    }

    void wait_for_surface_release()
    {
        std::unique_lock<std::mutex> lock(guard);
        while (surface)
            wait_condition.wait(lock);
    }


    std::mutex guard;
    std::condition_variable wait_condition;
    MirSurface * surface;
};

struct ClientConfigCommon : TestingClientConfiguration
{
    static const int max_surface_count = 5;
    SurfaceSync ssync[max_surface_count];
};
const int ClientConfigCommon::max_surface_count;
}
}

using SurfaceLoop = BespokeDisplayServerTestFixture;
using SurfaceLoopDefault = DefaultDisplayServerTestFixture;
using mir::SurfaceSync;
using mir::ClientConfigCommon;
using mir::StubDisplay;

namespace
{
void create_surface_callback(MirSurface* surface, void * context)
{
    SurfaceSync* config = reinterpret_cast<SurfaceSync*>(context);
    config->surface_created(surface);
}

void release_surface_callback(MirSurface* surface, void * context)
{
    SurfaceSync* config = reinterpret_cast<SurfaceSync*>(context);
    config->surface_released(surface);
}

void wait_for_surface_create(SurfaceSync* context)
{
    context->wait_for_surface_create();
}

void wait_for_surface_release(SurfaceSync* context)
{
    context->wait_for_surface_release();
}
}

TEST_F(SurfaceLoopDefault, creating_a_client_surface_gets_surface_of_requested_size)
{
    struct ClientConfig : TestingClientConfiguration
    {
        void exec()
        {
            auto const connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            ASSERT_TRUE(connection != NULL);
            EXPECT_TRUE(mir_connection_is_valid(connection));
            EXPECT_STREQ(mir_connection_get_error_message(connection), "");

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };

            auto const surface = mir_connection_create_surface_sync(connection, &request_params);

            ASSERT_TRUE(surface != NULL);
            EXPECT_TRUE(mir_surface_is_valid(surface));
            EXPECT_STREQ(mir_surface_get_error_message(surface), "");

            MirSurfaceParameters response_params;
            mir_surface_get_parameters(surface, &response_params);
            EXPECT_EQ(request_params.width, response_params.width);
            EXPECT_EQ(request_params.height, response_params.height);
            EXPECT_EQ(request_params.pixel_format, response_params.pixel_format);
            EXPECT_EQ(request_params.buffer_usage, response_params.buffer_usage);

            mir_surface_release_sync(surface);
            mir_connection_release(connection);
        }
    } client_config;

    launch_client_process(client_config);
}

namespace
{

/*
 * Need to declare outside method, because g++ 4.4 doesn't support local types
 * as template parameters (in std::make_shared<StubPlatform>()).
 */
struct ServerConfigAllocatesBuffersOnServer : TestingServerConfiguration
{
    class StubPlatform : public mtd::NullPlatform
    {
     public:
        std::shared_ptr<mg::GraphicBufferAllocator> create_buffer_allocator(
            const std::shared_ptr<mg::BufferInitializer>& /*buffer_initializer*/) override
        {
            using testing::AtMost;

            auto buffer_allocator = std::make_shared<testing::NiceMock<MockGraphicBufferAllocator>>();
            EXPECT_CALL(*buffer_allocator, alloc_buffer(buffer_properties))
                .Times(AtMost(3));
            return buffer_allocator;
        }

        std::shared_ptr<mg::Display> create_display(
            std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
            std::shared_ptr<mg::GLProgramFactory> const&,
            std::shared_ptr<mg::GLConfig> const&) override
        {
            return std::make_shared<StubDisplay>();
        }
    };

    std::shared_ptr<mg::Platform> the_graphics_platform()
    {
        if (!platform)
            platform = std::make_shared<StubPlatform>();

        return platform;
    }

    std::shared_ptr<mg::Platform> platform;
};

}

TEST_F(SurfaceLoop, creating_a_client_surface_allocates_buffers_on_server)
{

    ServerConfigAllocatesBuffersOnServer server_config;

    launch_server_process(server_config);

    struct ClientConfig : ClientConfigCommon
    {
        void exec()
        {
            auto connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            ASSERT_TRUE(connection != NULL);
            EXPECT_TRUE(mir_connection_is_valid(connection));
            EXPECT_STREQ(mir_connection_get_error_message(connection), "");

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };
            mir_connection_create_surface(connection, &request_params, create_surface_callback, ssync);

            wait_for_surface_create(ssync);

            ASSERT_TRUE(ssync->surface != NULL);
            EXPECT_TRUE(mir_surface_is_valid(ssync->surface));
            EXPECT_STREQ(mir_surface_get_error_message(ssync->surface), "");

            MirSurfaceParameters response_params;
            mir_surface_get_parameters(ssync->surface, &response_params);
            EXPECT_EQ(request_params.width, response_params.width);
            EXPECT_EQ(request_params.height, response_params.height);
            EXPECT_EQ(request_params.pixel_format, response_params.pixel_format);
            EXPECT_EQ(request_params.buffer_usage, response_params.buffer_usage);


            mir_surface_release(ssync->surface, release_surface_callback, ssync);

            wait_for_surface_release(ssync);

            ASSERT_TRUE(ssync->surface == NULL);

            mir_connection_release(connection);
        }
    } client_config;

    launch_client_process(client_config);
}

namespace
{
struct BufferCounterConfig : TestingServerConfiguration
{
    class CountingStubBuffer : public mtd::StubBuffer
    {
    public:

        CountingStubBuffer()
        {
            int created = buffers_created.load();
            while (!buffers_created.compare_exchange_weak(created, created + 1)) std::this_thread::yield();
        }
        ~CountingStubBuffer()
        {
            int destroyed = buffers_destroyed.load();
            while (!buffers_destroyed.compare_exchange_weak(destroyed, destroyed + 1)) std::this_thread::yield();
        }

        static std::atomic<int> buffers_created;
        static std::atomic<int> buffers_destroyed;
    };

    class StubGraphicBufferAllocator : public mtd::StubBufferAllocator
    {
     public:
        std::shared_ptr<mg::Buffer> alloc_buffer(mg::BufferProperties const&) override
        {
            return std::make_shared<CountingStubBuffer>();
        }
    };

    class StubPlatform : public mtd::NullPlatform
    {
    public:
        std::shared_ptr<mg::GraphicBufferAllocator> create_buffer_allocator(
            const std::shared_ptr<mg::BufferInitializer>& /*buffer_initializer*/) override
        {
            return std::make_shared<StubGraphicBufferAllocator>();
        }

        std::shared_ptr<mg::Display> create_display(
            std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
            std::shared_ptr<mg::GLProgramFactory> const&,
            std::shared_ptr<mg::GLConfig> const&) override
        {
            return std::make_shared<StubDisplay>();
        }
    };

    std::shared_ptr<mg::Platform> the_graphics_platform()
    {
        if (!platform)
            platform = std::make_shared<StubPlatform>();

        return platform;
    }

    std::shared_ptr<mg::Platform> platform;
};

std::atomic<int> BufferCounterConfig::CountingStubBuffer::buffers_created;
std::atomic<int> BufferCounterConfig::CountingStubBuffer::buffers_destroyed;
}


TEST_F(SurfaceLoop, all_created_buffers_are_destoyed)
{
    struct ServerConfig : BufferCounterConfig
    {
        void on_exit() override
        {
            EXPECT_EQ(CountingStubBuffer::buffers_created.load(),
                      CountingStubBuffer::buffers_destroyed.load());
        }

    } server_config;

    launch_server_process(server_config);

    struct Client : ClientConfigCommon
    {
        void exec() override
        {
            auto connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };

            for (int i = 0; i != max_surface_count; ++i)
                mir_connection_create_surface(connection, &request_params, create_surface_callback, ssync+i);

            for (int i = 0; i != max_surface_count; ++i)
                wait_for_surface_create(ssync+i);

            for (int i = 0; i != max_surface_count; ++i)
                mir_surface_release(ssync[i].surface, release_surface_callback, ssync+i);

            for (int i = 0; i != max_surface_count; ++i)
                wait_for_surface_release(ssync+i);

            mir_connection_release(connection);
        }
    } client_creates_surfaces;

    launch_client_process(client_creates_surfaces);
}

TEST_F(SurfaceLoop, all_created_buffers_are_destoyed_if_client_disconnects_without_releasing_surfaces)
{
    struct ServerConfig : BufferCounterConfig
    {
        void on_exit() override
        {
            EXPECT_EQ(CountingStubBuffer::buffers_created.load(),
                      CountingStubBuffer::buffers_destroyed.load());
        }

    } server_config;

    launch_server_process(server_config);

    struct Client : ClientConfigCommon
    {
        void exec() override
        {
            auto connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };

            for (int i = 0; i != max_surface_count; ++i)
                mir_connection_create_surface(connection, &request_params, create_surface_callback, ssync+i);

            for (int i = 0; i != max_surface_count; ++i)
                wait_for_surface_create(ssync+i);

            mir_connection_release(connection);
        }
    } client_creates_surfaces;

    launch_client_process(client_creates_surfaces);
}
