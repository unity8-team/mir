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

#include "mir/frontend/vsync_provider.h"
#include "mir/graphics/display_configuration.h"
#include "mir/input/input_platform.h"
#include "mir/input/input_receiver_thread.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test/fake_shared.h"
#include "mir_test_framework/in_process_server.h"
#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/stub_client_connection_configuration.h"
#include "mir_test_framework/using_stub_client_platform.h"

namespace mg = mir::graphics;
namespace mi = mir::input;
namespace mf = mir::frontend;
namespace mircv = mi::receiver;
namespace mt = mir::test;
namespace mtf = mir_test_framework;

// TODO: Perhaps consider a better file name.

namespace
{
// Client mocks and configuration
struct MockInputReceiverThread : public mircv::InputReceiverThread
{
    MOCK_METHOD1(notify_of_frame_time, void(std::chrono::nanoseconds));
    // Stub of uninteresting methods
    void start() override {}
    void stop() override {}
    void join() override {}
};

struct MockInputThreadInputPlatform : public mircv::InputPlatform
{
    MockInputThreadInputPlatform(std::shared_ptr<mircv::InputReceiverThread> const& receiver_thread)
        : mock_receiver_thread(receiver_thread)
    {
    }
    std::shared_ptr<mircv::InputReceiverThread> create_input_thread(int, 
        std::function<void(MirEvent *)> const&)
    {
        return mock_receiver_thread;
    }

    std::shared_ptr<mircv::InputReceiverThread> const mock_receiver_thread;
};

struct InputMockInjectingClientConnectionConfiguration : public mtf::StubConnectionConfiguration
{
    InputMockInjectingClientConnectionConfiguration(std::string const& socket_file, std::shared_ptr<mircv::InputReceiverThread> const& receiver_thread)
        : StubConnectionConfiguration(socket_file),
          mock_input_platform(receiver_thread)
    {
    }
    
    std::shared_ptr<mircv::InputPlatform> the_input_platform() override
    {
        return mt::fake_shared(mock_input_platform);
    }
    MockInputThreadInputPlatform mock_input_platform;
};

// Server mocks and configuration
struct StubVsyncProvider : public mf::VsyncProvider
{
    std::chrono::nanoseconds last_vsync_for(mg::DisplayConfigurationOutputId) override
    {
        static int count = 0;
        return std::chrono::nanoseconds(count++);
    }
};

struct StubVsyncProviderServerConfiguration : mtf::StubbedServerConfiguration
{
    std::shared_ptr<mf::VsyncProvider>
    the_vsync_provider() /* TODO: override */
    {
        return vsync_provider([]()
        {
            return std::make_shared<StubVsyncProvider>();
        });
    }
};


struct VsyncProviderTest : mtf::InProcessServer
{
    StubVsyncProviderServerConfiguration server_configuration;

    mir::DefaultServerConfiguration& server_config() override { return server_configuration; }
};

}

TEST_F(VsyncProviderTest, last_display_time)
{
    using namespace ::testing;

    mtf::UsingStubClientPlatform using_stub_client_platform;

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    MirSurfaceParameters const request_params =
    {
        __PRETTY_FUNCTION__,
        640, 480,
        mir_pixel_format_abgr_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };
    auto surface = mir_connection_create_surface_sync(connection, &request_params);
    EXPECT_EQ(true, mir_surface_is_valid(surface));

    // The fake vsync provider on server just increments an integer
    // for each vsync request.
    mir_surface_swap_buffers_sync(surface);
    EXPECT_EQ(1, mir_surface_get_last_display_time(surface));
    mir_surface_swap_buffers_sync(surface);
    EXPECT_EQ(2, mir_surface_get_last_display_time(surface));
    mir_surface_swap_buffers_sync(surface);
    EXPECT_EQ(3, mir_surface_get_last_display_time(surface));
    
    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

TEST_F(VsyncProviderTest, client_input_thread_receives_information_from_server_vsync_provider_on_buffer_swap)
{
    using namespace ::testing;


    MockInputReceiverThread mock_input_receiver_thread;
    {
        InSequence seq;
        // The fake vsync provider just uses increments for each vsync request.
        EXPECT_CALL(mock_input_receiver_thread, notify_of_frame_time(std::chrono::nanoseconds(0)));
        EXPECT_CALL(mock_input_receiver_thread, notify_of_frame_time(std::chrono::nanoseconds(1)));
        EXPECT_CALL(mock_input_receiver_thread, notify_of_frame_time(std::chrono::nanoseconds(2)));
    }

    mtf::UsingStubClientPlatform using_stub_client_platform([&](std::string const& socket_file) {
        return std::unique_ptr<InputMockInjectingClientConnectionConfiguration>(
            new InputMockInjectingClientConnectionConfiguration(socket_file,
                mt::fake_shared(mock_input_receiver_thread)));
    });

    auto connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    MirSurfaceParameters const request_params =
    {
        __PRETTY_FUNCTION__,
        640, 480,
        mir_pixel_format_abgr_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };
    auto surface = mir_connection_create_surface_sync(connection, &request_params);
    mir_surface_swap_buffers_sync(surface);
    mir_surface_swap_buffers_sync(surface);
    mir_surface_swap_buffers_sync(surface);
    
    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

// TODO: Make sure to install UsingStubClientPlatform
