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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/frontend/session_authorizer.h"

#include <mir_toolkit/mir_client_library.h>

#include "mir_test/fake_shared.h"
#include "mir_test_framework/display_server_test_fixture.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>

namespace mf = mir::frontend;
namespace mt = mir::test;
namespace mtf = mir_test_framework;

namespace
{
    char const* const mir_test_socket = mtf::test_socket_file().c_str();
}

namespace
{
struct ConnectingClient : TestingClientConfiguration
{
    MirConnection *connection;
    void exec()
    {
        connection = mir_connect_sync(
            mir_test_socket,
            __PRETTY_FUNCTION__);
        mir_connection_release(connection);
    }
};

struct ClientPidTestFixture : BespokeDisplayServerTestFixture
{
    ClientPidTestFixture()
    {
        shared_region = static_cast<SharedRegion*>(
            mmap(NULL, sizeof(SharedRegion), PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_SHARED, 0, 0));
        sem_init(&shared_region->client_pid_set, 1, 0);
        shared_region->client_pid = 0;
    }

    struct SharedRegion
    {
        sem_t client_pid_set;
        pid_t client_pid;

        pid_t get_client_process_pid()
        {
            static time_t const timeout_seconds = 60;
            struct timespec abs_timeout = { time(NULL) + timeout_seconds, 0 };
            sem_timedwait(&client_pid_set, &abs_timeout);
            return client_pid;
        }
        void post_client_process_pid(pid_t pid)
        {
            client_pid = pid;
            sem_post(&client_pid_set);
        }
    };

    SharedRegion* shared_region;
};

struct MockSessionAuthorizer : public mf::SessionAuthorizer
{
    MOCK_METHOD1(connection_is_allowed, bool(pid_t));
    MOCK_METHOD1(configure_display_is_allowed, bool(pid_t));
    MOCK_METHOD1(screencast_is_allowed, bool(pid_t));
};

}

TEST_F(ClientPidTestFixture, session_authorizer_receives_pid_of_connecting_clients)
{
    mtf::CrossProcessSync connect_sync;

    struct ServerConfiguration : TestingServerConfiguration
    {
        ServerConfiguration(ClientPidTestFixture::SharedRegion *shared_region,
                            mtf::CrossProcessSync const& connect_sync)
            : shared_region(shared_region),
              connect_sync(connect_sync)
        {
        }

        void on_start() override
        {
            using namespace ::testing;
            pid_t client_pid = shared_region->get_client_process_pid();

            EXPECT_CALL(mock_authorizer, connection_is_allowed(client_pid)).Times(1)
                .WillOnce(Return(true));
            EXPECT_CALL(mock_authorizer, configure_display_is_allowed(client_pid)).Times(1)
                .WillOnce(Return(false));
            EXPECT_CALL(mock_authorizer, screencast_is_allowed(client_pid)).Times(1)
                .WillOnce(Return(false));
            connect_sync.try_signal_ready_for();
        }

        std::shared_ptr<mf::SessionAuthorizer> the_session_authorizer() override
        {
            return mt::fake_shared(mock_authorizer);
        }

        ClientPidTestFixture::SharedRegion* shared_region;
        MockSessionAuthorizer mock_authorizer;
        mtf::CrossProcessSync connect_sync;
    } server_config(shared_region, connect_sync);
    launch_server_process(server_config);


    struct ClientConfiguration : ConnectingClient
    {
        ClientConfiguration(ClientPidTestFixture::SharedRegion *shared_region,
                            mtf::CrossProcessSync const& connect_sync)
            : shared_region(shared_region),
              connect_sync(connect_sync)
        {
        }

        void exec() override
        {
            pid_t client_pid = getpid();
            shared_region->post_client_process_pid(client_pid);

            connect_sync.wait_for_signal_ready_for();
            ConnectingClient::exec();
        }

        ClientPidTestFixture::SharedRegion* shared_region;
        mtf::CrossProcessSync connect_sync;
    } client_config(shared_region, connect_sync);
    launch_client_process(client_config);
}

// TODO this test exposes a race condition when the client processes both an error
// TODO from connect and the socket being dropped. RAOF is doing some rework that
// TODO should fix this a side effect. It should be re-enabled when that is done.
TEST_F(ClientPidTestFixture, DISABLED_authorizer_may_prevent_connection_of_clients)
{
    using namespace ::testing;

    struct ServerConfiguration : TestingServerConfiguration
    {
        ServerConfiguration(ClientPidTestFixture::SharedRegion *shared_region)
            : shared_region(shared_region)
        {
        }

        void exec() override
        {
            using namespace ::testing;
            pid_t client_pid = shared_region->get_client_process_pid();

            EXPECT_CALL(mock_authorizer, connection_is_allowed(client_pid)).Times(1)
                .WillOnce(Return(false));
        }

        std::shared_ptr<mf::SessionAuthorizer> the_session_authorizer() override
        {
            return mt::fake_shared(mock_authorizer);
        }

        ClientPidTestFixture::SharedRegion* shared_region;
        MockSessionAuthorizer mock_authorizer;
    } server_config(shared_region);
    launch_server_process(server_config);


    struct ClientConfiguration : ConnectingClient
    {
        ClientConfiguration(ClientPidTestFixture::SharedRegion *shared_region)
            : shared_region(shared_region)
        {
        }

        void exec() override
        {
            pid_t client_pid = getpid();
            shared_region->post_client_process_pid(client_pid);

            connection = mir_connect_sync(
                mir_test_socket,
                __PRETTY_FUNCTION__);

            MirSurfaceParameters const parameters =
            {
                __PRETTY_FUNCTION__,
                1, 1,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };
            mir_connection_create_surface_sync(connection, &parameters);
            EXPECT_GT(strlen(mir_connection_get_error_message(connection)), static_cast<unsigned int>(0));
            mir_connection_release(connection);
        }

        //we are testing the connect function itself, without getting to the
        // point where drivers are used, so force using production config
        bool use_real_graphics(mir::options::Option const&) override
        {
            return true;
        }

        ClientPidTestFixture::SharedRegion* shared_region;
    } client_config(shared_region);
    launch_client_process(client_config);
}
