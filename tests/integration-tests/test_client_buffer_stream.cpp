/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/scene/buffer_stream_factory.h"

#include "mir_test/fake_shared.h"
#include "mir_test/wait_condition.h"
#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_framework/in_process_server.h"
#include "mir_test_framework/testing_server_configuration.h"
#include "mir_test_framework/using_stub_client_platform.h"

#include <mir_toolkit/mir_client_library.h>

namespace ms = mir::scene;
namespace mg = mir::graphics;
namespace mc = mir::compositor;

namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace mtf = mir_test_framework;

namespace
{

struct ObservableBufferStream : public mtd::StubBufferStream
{
    MOCK_METHOD1(client_acquired, void(mg::Buffer*));
    MOCK_METHOD1(client_released, void(mg::Buffer*));

    void acquire_client_buffer(
        std::function<void(mg::Buffer* buffer)> complete) override
    {
        StubBufferStream::acquire_client_buffer(complete);
        client_acquired(&stub_client_buffer);
    }

    void release_client_buffer(mg::Buffer* buffer) override
    {
        StubBufferStream::release_client_buffer(buffer);
        client_released(buffer);
    }
};

struct StubBufferStreamFactory : public ms::BufferStreamFactory
{
    StubBufferStreamFactory(std::shared_ptr<mc::BufferStream> const& buffer_stream)
        : buffer_stream(buffer_stream)
    {
    }
    
    std::shared_ptr<mc::BufferStream> create_buffer_stream(
        mg::BufferProperties const& /* buffer_properties */)
    {
        return buffer_stream;
    }

    std::shared_ptr<mc::BufferStream> const buffer_stream;
};

struct TestingServerConfiguration : public mtf::TestingServerConfiguration
{
    TestingServerConfiguration() :
        buffer_stream_factory(mt::fake_shared(buffer_stream))
    {
    }

    std::shared_ptr<ms::BufferStreamFactory> the_buffer_stream_factory()
    {
        return mt::fake_shared(buffer_stream_factory);
    }

    ObservableBufferStream buffer_stream;
    StubBufferStreamFactory buffer_stream_factory;
};

struct TestClientBufferStreamAPI : mtf::InProcessServer
{
    mir::DefaultServerConfiguration& server_config() override
    {
        return config;
    }

    TestingServerConfiguration config;
    mtf::UsingStubClientPlatform using_stub_client_platform;
};

struct ConnectingClient
{
    ConnectingClient(std::string const& connect_string, std::string const& client_name,
        std::function<void(MirConnection*)> const& do_with_connection)
        : connect_string(connect_string),
          client_name(client_name),
          do_with_connection(do_with_connection)
    {
    }
    
    virtual ~ConnectingClient()
    {
        teardown.wake_up_everyone();
        if (client_thread.joinable())
            client_thread.join();
    }
    
    void run()
    {
        mir::test::WaitCondition client_done;
        client_thread = std::thread{
            [this,&client_done]
            {
                auto const connection =
                    mir_connect_sync(connect_string.c_str(), client_name.c_str());
                do_with_connection(connection);
                mir_connection_release(connection);
                
                client_done.wake_up_everyone();
            }};

        client_done.wait_for_at_most_seconds(5);
    }

    std::string const connect_string;
    std::string const client_name;

    std::function<void(MirConnection*)> const do_with_connection;
    
    std::thread client_thread;
    mir::test::WaitCondition teardown;
};

}

TEST_F(TestClientBufferStreamAPI, client_creating_buffer_stream_receives_stream_with_native_window)
{
    using namespace ::testing;

    int width = 24, height = 24;
    MirPixelFormat format = mir_pixel_format_argb_8888;
    MirBufferUsage buffer_usage = mir_buffer_usage_hardware;

    ConnectingClient c(new_connection(), "test",
        [&](MirConnection *connection)
        {
            MirBufferStream *stream = 
                mir_connection_create_buffer_stream_sync(connection, width, height, format, buffer_usage);
            EXPECT_NE(MirEGLNativeWindowType(), mir_buffer_stream_get_egl_native_window(stream));
            mir_buffer_stream_release_sync(stream);
        });

    EXPECT_CALL(config.buffer_stream, client_acquired(_)).Times(1);

    c.run();
    
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

TEST_F(TestClientBufferStreamAPI, swapping_exchanges_buffer_with_server_side_stream)
{
    using namespace ::testing;

    mt::WaitCondition expectations_satisfied;

    int width = 24, height = 24;
    MirPixelFormat format = mir_pixel_format_argb_8888;
    MirBufferUsage buffer_usage = mir_buffer_usage_software;

    ConnectingClient c(new_connection(), "test",
        [&](MirConnection *connection)
        {
            MirBufferStream *stream = 
                mir_connection_create_buffer_stream_sync(connection, width, height, format, buffer_usage);
            
            mir_buffer_stream_swap_buffers_sync(stream);
            mir_buffer_stream_swap_buffers_sync(stream);
            mir_buffer_stream_swap_buffers_sync(stream);

            mir_buffer_stream_release_sync(stream);
        });

    InSequence seq;
    // Creation
    EXPECT_CALL(config.buffer_stream, client_acquired(_)).Times(1);
    // Swap 1
    EXPECT_CALL(config.buffer_stream, client_released(_)).Times(1);
    EXPECT_CALL(config.buffer_stream, client_acquired(_)).Times(1);
    // Swap 2
    EXPECT_CALL(config.buffer_stream, client_released(_)).Times(1);
    EXPECT_CALL(config.buffer_stream, client_acquired(_)).Times(1);
    // Swap 3
    EXPECT_CALL(config.buffer_stream, client_released(_)).Times(1);
    EXPECT_CALL(config.buffer_stream, client_acquired(_)).Times(1)
        .WillOnce(mt::WakeUp(&expectations_satisfied));

    c.run();
    
    expectations_satisfied.wait_for_at_most_seconds(5);
}
