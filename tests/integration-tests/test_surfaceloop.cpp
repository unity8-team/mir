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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/compositor/graphic_buffer_allocator.h"
#include "mir/compositor/swapper_factory.h"
#include "mir/compositor/buffer_swapper.h"
#include "mir/compositor/buffer_swapper_multi.h"
#include "mir/compositor/buffer_ipc_package.h"
#include "mir/compositor/buffer_properties.h"
#include "mir/compositor/buffer_id.h"
#include "mir/compositor/buffer_basic.h"
#include "mir/graphics/display.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_doubles/stub_buffer.h"

#include <thread>
#include <atomic>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mir_test/gmock_fixes.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;

namespace
{
char const* const mir_test_socket = mtf::test_socket_file().c_str();

geom::Size const size{geom::Width{640}, geom::Height{480}};
geom::PixelFormat const format{geom::PixelFormat::abgr_8888};
mc::BufferUsage const usage{mc::BufferUsage::hardware};
mc::BufferProperties const buffer_properties{size, format, usage};

geom::Rectangle const default_view_area = geom::Rectangle{geom::Point(),
                                                          geom::Size{geom::Width(1600),
                                                                     geom::Height(1600)}};

struct MockBufferAllocationStrategy : public mc::BufferAllocationStrategy
{
    MockBufferAllocationStrategy()
    {
        using testing::_;
        ON_CALL(*this, create_swapper(_, _))
            .WillByDefault(testing::Invoke(this, &MockBufferAllocationStrategy::on_create_swapper));
    }

    MOCK_METHOD2(
        create_swapper,
        std::unique_ptr<mc::BufferSwapper>(mc::BufferProperties&, mc::BufferProperties const&));

    std::unique_ptr<mc::BufferSwapper> on_create_swapper(mc::BufferProperties& actual,
                                                         mc::BufferProperties const& requested)
    {
        actual = requested;
        auto stub_buffer_a = std::make_shared<mtd::StubBuffer>(::buffer_properties);
        auto stub_buffer_b = std::make_shared<mtd::StubBuffer>(::buffer_properties);
        std::initializer_list<std::shared_ptr<mc::Buffer>> list = {stub_buffer_a, stub_buffer_b};
        return std::unique_ptr<mc::BufferSwapper>(
            new mc::BufferSwapperMulti(list));
    }
};

class MockGraphicBufferAllocator : public mc::GraphicBufferAllocator
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
        std::shared_ptr<mc::Buffer> (mc::BufferProperties const&));


    std::vector<geom::PixelFormat> supported_pixel_formats()
    {
        return std::vector<geom::PixelFormat>();
    }

    std::unique_ptr<mc::Buffer> on_create_swapper(mc::BufferProperties const&)
    {
        return std::unique_ptr<mc::Buffer>(new mtd::StubBuffer(::buffer_properties));
    }
};

}

namespace mir
{
namespace
{

class StubDisplay : public mg::Display
{
public:
    geom::Rectangle view_area() const
    {
        return default_view_area;
    }
    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f)
    {
        (void)f;
        std::this_thread::yield();
    }
    std::shared_ptr<mg::DisplayConfiguration> configuration()
    {
        auto null_configuration = std::shared_ptr<mg::DisplayConfiguration>();
        return null_configuration;
    }
    void register_pause_resume_handlers(mir::MainLoop&,
                                        mg::DisplayPauseHandler const&,
                                        mg::DisplayResumeHandler const&)
    {
    }
    void pause() {}
    void resume() {}
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
    ClientConfigCommon()
        : connection(NULL)
    {
    }

    static void connection_callback(MirConnection * connection, void * context)
    {
        ClientConfigCommon * config = reinterpret_cast<ClientConfigCommon *>(context);
        config->connected(connection);
    }

    void connected(MirConnection * new_connection)
    {
        std::unique_lock<std::mutex> lock(guard);
        connection = new_connection;
        wait_condition.notify_all();
    }

    void wait_for_connect()
    {
        std::unique_lock<std::mutex> lock(guard);
        while (!connection)
            wait_condition.wait(lock);
    }

    std::mutex guard;
    std::condition_variable wait_condition;
    MirConnection * connection;
    static const int max_surface_count = 5;
    SurfaceSync ssync[max_surface_count];
};
const int ClientConfigCommon::max_surface_count;
}
}

using SurfaceLoop = BespokeDisplayServerTestFixture;
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

TEST_F(SurfaceLoop,
       creating_a_client_surface_allocates_buffer_swapper_on_server)
{

    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<mc::BufferAllocationStrategy> the_buffer_allocation_strategy()
        {
            using namespace testing;

            if (!buffer_allocation_strategy)
                buffer_allocation_strategy = std::make_shared<MockBufferAllocationStrategy>();

            EXPECT_CALL(*buffer_allocation_strategy, create_swapper(_, buffer_properties)).Times(1);

            return buffer_allocation_strategy;
        }

        std::shared_ptr<MockBufferAllocationStrategy> buffer_allocation_strategy;
    } server_config;

    launch_server_process(server_config);

    struct ClientConfig : ClientConfigCommon
    {
        void exec()
        {
            mir_connect(mir_test_socket, __PRETTY_FUNCTION__, connection_callback, this);

            wait_for_connect();

            ASSERT_TRUE(connection != NULL);
            EXPECT_TRUE(mir_connection_is_valid(connection));
            EXPECT_STREQ(mir_connection_get_error_message(connection), "");

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware
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

/*
 * Need to declare outside method, because g++ 4.4 doesn't support local types
 * as template parameters (in std::make_shared<StubPlatform>()).
 */
struct ServerConfigAllocatesBuffersOnServer : TestingServerConfiguration
{
    class StubPlatform : public mg::Platform
    {
     public:
        std::shared_ptr<mc::GraphicBufferAllocator> create_buffer_allocator(
                const std::shared_ptr<mg::BufferInitializer>& /*buffer_initializer*/)
        {
            using testing::AtLeast;

            auto buffer_allocator = std::make_shared<testing::NiceMock<MockGraphicBufferAllocator>>();
            EXPECT_CALL(*buffer_allocator, alloc_buffer(buffer_properties)).Times(AtLeast(2));
            return buffer_allocator;
        }

        std::shared_ptr<mg::Display> create_display()
        {
            return std::make_shared<StubDisplay>();
        }

        std::shared_ptr<mg::PlatformIPCPackage> get_ipc_package()
        {
            return std::make_shared<mg::PlatformIPCPackage>();
        }

        EGLNativeDisplayType shell_egl_display()
        {
            return static_cast<EGLNativeDisplayType>(0);
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

TEST_F(SurfaceLoop,
       creating_a_client_surface_allocates_buffers_on_server)
{

    ServerConfigAllocatesBuffersOnServer server_config;

    launch_server_process(server_config);

    struct ClientConfig : ClientConfigCommon
    {
        void exec()
        {
            mir_connect(mir_test_socket, __PRETTY_FUNCTION__, connection_callback, this);

            wait_for_connect();

            ASSERT_TRUE(connection != NULL);
            EXPECT_TRUE(mir_connection_is_valid(connection));
            EXPECT_STREQ(mir_connection_get_error_message(connection), "");

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware
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

    class StubGraphicBufferAllocator : public mc::GraphicBufferAllocator
    {
     public:
        virtual std::shared_ptr<mc::Buffer> alloc_buffer(
            mc::BufferProperties const&)
        {
            return std::unique_ptr<mc::Buffer>(new CountingStubBuffer());
        }

        std::vector<geom::PixelFormat> supported_pixel_formats()
        {
            return std::vector<geom::PixelFormat>();
        }
    };

    class StubPlatform : public mg::Platform
    {
    public:
        std::shared_ptr<mc::GraphicBufferAllocator> create_buffer_allocator(
                const std::shared_ptr<mg::BufferInitializer>& /*buffer_initializer*/)
        {
            return std::make_shared<StubGraphicBufferAllocator>();
        }

        std::shared_ptr<mg::Display> create_display()
        {
            return std::make_shared<StubDisplay>();
        }

        std::shared_ptr<mg::PlatformIPCPackage> get_ipc_package()
        {
            return std::make_shared<mg::PlatformIPCPackage>();
        }

        EGLNativeDisplayType shell_egl_display()
        {
            return (EGLNativeDisplayType) 0;
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
            EXPECT_EQ(2*ClientConfigCommon::max_surface_count, CountingStubBuffer::buffers_created.load());
            EXPECT_EQ(2*ClientConfigCommon::max_surface_count, CountingStubBuffer::buffers_destroyed.load());
        }

    } server_config;

    launch_server_process(server_config);

    struct Client : ClientConfigCommon
    {
        void exec() override
        {
            mir_connect(mir_test_socket, __PRETTY_FUNCTION__, connection_callback, this);

            wait_for_connect();

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware
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
            EXPECT_EQ(2*ClientConfigCommon::max_surface_count, CountingStubBuffer::buffers_created.load());
            EXPECT_EQ(2*ClientConfigCommon::max_surface_count, CountingStubBuffer::buffers_destroyed.load());
        }

    } server_config;

    launch_server_process(server_config);

    struct Client : ClientConfigCommon
    {
        void exec() override
        {
            mir_connect(mir_test_socket, __PRETTY_FUNCTION__, connection_callback, this);

            wait_for_connect();

            MirSurfaceParameters const request_params =
            {
                __PRETTY_FUNCTION__,
                640, 480,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware
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
