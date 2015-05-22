/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/compositor/display_buffer_compositor.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/renderer.h"
#include "mir/compositor/renderer_factory.h"

#include "mir/run_mir.h"
#include "mir/main_loop.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/fake_event_hub_server_configuration.h"
#include "mir_test_framework/any_surface.h"

#include "mir_test/fake_event_hub.h"
#include "mir_test_doubles/stub_renderer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <fcntl.h>
#include <thread>

namespace geom = mir::geometry;
namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace mia = mir::input::android;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{
char const* const mir_test_socket = mtf::test_socket_file().c_str();

class StubRendererFactory : public mc::RendererFactory
{
public:
    std::unique_ptr<mc::Renderer> create_renderer_for(geom::Rectangle const&)
    {
        return std::unique_ptr<mc::Renderer>(new mtd::StubRenderer());
    }
};

void null_buffer_stream_callback(MirBufferStream*, void*)
{
}

void null_lifecycle_callback(MirConnection*, MirLifecycleState, void*)
{
}

class ExceptionThrowingDisplayBufferCompositorFactory : public mc::DisplayBufferCompositorFactory
{
public:
    std::unique_ptr<mc::DisplayBufferCompositor>
        create_compositor_for(mg::DisplayBuffer&) override
    {
        struct ExceptionThrowingDisplayBufferCompositor : mc::DisplayBufferCompositor
        {
            void composite(mc::SceneElementSequence&&) override
            {
                throw std::runtime_error("ExceptionThrowingDisplayBufferCompositor");
            }
        };

        return std::unique_ptr<mc::DisplayBufferCompositor>(
            new ExceptionThrowingDisplayBufferCompositor{});
    }
};

class Flag
{
public:
    explicit Flag(std::string const& flag_file)
        : flag_file{flag_file}
    {
        std::remove(flag_file.c_str());
    }

    void set()
    {
        close(open(flag_file.c_str(), O_CREAT, S_IWUSR | S_IRUSR));
    }

    bool is_set()
    {
        int fd = -1;
        if ((fd = open(flag_file.c_str(), O_RDONLY, S_IWUSR | S_IRUSR)) != -1)
        {
            close(fd);
            return true;
        }
        return false;
    }

    void wait()
    {
        while (!is_set())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

private:
    std::string const flag_file;
};
}

using ServerShutdown = BespokeDisplayServerTestFixture;

TEST_F(ServerShutdown, server_can_shut_down_when_clients_are_blocked)
{
    Flag next_buffer_done1{"next_buffer_done1_c5d49978.tmp"};
    Flag next_buffer_done2{"next_buffer_done2_c5d49978.tmp"};
    Flag next_buffer_done3{"next_buffer_done3_c5d49978.tmp"};
    Flag server_done{"server_done_c5d49978.tmp"};

    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<mc::RendererFactory> the_renderer_factory() override
        {
            return renderer_factory([] { return std::make_shared<StubRendererFactory>(); });
        }
    } server_config;

    launch_server_process(server_config);

    struct ClientConfig : TestingClientConfiguration
    {
        ClientConfig(Flag& next_buffer_done,
                     Flag& server_done)
            : next_buffer_done(next_buffer_done),
              server_done(server_done)
        {
        }

        void exec()
        {
            MirConnection* connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            ASSERT_TRUE(connection != NULL);

            /* Default lifecycle handler terminates the process on disconnect, so override it */
            mir_connection_set_lifecycle_event_callback(connection, null_lifecycle_callback, nullptr);

            auto surf = mtf::make_any_surface(connection);

            /* Ask for the first buffer (should succeed) */
            mir_buffer_stream_swap_buffers_sync(mir_surface_get_buffer_stream(surf));
            /* Ask for the first second buffer (should block) */
            mir_buffer_stream_swap_buffers(mir_surface_get_buffer_stream(surf), null_buffer_stream_callback, nullptr);

            next_buffer_done.set();
            server_done.wait();

            mir_connection_release(connection);
        }


        Flag& next_buffer_done;
        Flag& server_done;
    };

    ClientConfig client_config1{next_buffer_done1, server_done};
    ClientConfig client_config2{next_buffer_done2, server_done};
    ClientConfig client_config3{next_buffer_done3, server_done};

    launch_client_process(client_config1);
    launch_client_process(client_config2);
    launch_client_process(client_config3);

    run_in_test_process([&]
    {
        /* Wait until the clients are blocked on getting the second buffer */
        next_buffer_done1.wait();
        next_buffer_done2.wait();
        next_buffer_done3.wait();

        /* Shutting down the server should not block */
        shutdown_server_process();

        /* Notify the clients that we are done (we only need to set the flag once) */
        server_done.set();
    });
}

TEST(ServerShutdownWithThreadException,
     server_releases_resources_on_abnormal_compositor_thread_termination)
{
    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<mc::DisplayBufferCompositorFactory>
            the_display_buffer_compositor_factory() override
        {
            return display_buffer_compositor_factory(
                [this]()
                {
                    return std::make_shared<ExceptionThrowingDisplayBufferCompositorFactory>();
                });
        }
    };

    auto server_config = std::make_shared<ServerConfig>();

    std::thread server{
        [&]
        {
            EXPECT_THROW(
                mir::run_mir(*server_config, [](mir::DisplayServer&){}),
                std::runtime_error);
        }};

    server.join();

    std::weak_ptr<mir::graphics::Display> display = server_config->the_display();
    std::weak_ptr<mir::compositor::Compositor> compositor = server_config->the_compositor();
    std::weak_ptr<mir::frontend::Connector> connector = server_config->the_connector();
    std::weak_ptr<mir::input::InputManager> input_manager = server_config->the_input_manager();

    server_config.reset();

    EXPECT_EQ(0, display.use_count());
    EXPECT_EQ(0, compositor.use_count());
    EXPECT_EQ(0, connector.use_count());
    EXPECT_EQ(0, input_manager.use_count());
}

TEST_F(ServerShutdown, server_releases_resources_on_shutdown_with_connected_clients)
{
    Flag surface_created1{"surface_created1_7e9c69fc.tmp"};
    Flag surface_created2{"surface_created2_7e9c69fc.tmp"};
    Flag surface_created3{"surface_created3_7e9c69fc.tmp"};
    Flag server_done{"server_done_7e9c69fc.tmp"};
    Flag resources_freed_success{"resources_free_success_7e9c69fc.tmp"};
    Flag resources_freed_failure{"resources_free_failure_7e9c69fc.tmp"};

    auto server_config = std::make_shared<mtf::FakeEventHubServerConfiguration>();
    launch_server_process(*server_config);

    struct ClientConfig : TestingClientConfiguration
    {
        ClientConfig(Flag& surface_created,
                     Flag& server_done)
            : surface_created(surface_created),
              server_done(server_done)
        {
        }

        void exec()
        {
            MirConnection* connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);

            ASSERT_TRUE(connection != NULL);

            mtf::make_any_surface(connection);

            surface_created.set();
            server_done.wait();

            mir_connection_release(connection);
        }

        Flag& surface_created;
        Flag& server_done;
    };

    ClientConfig client_config1{surface_created1, server_done};
    ClientConfig client_config2{surface_created2, server_done};
    ClientConfig client_config3{surface_created3, server_done};

    launch_client_process(client_config1);
    launch_client_process(client_config2);
    launch_client_process(client_config3);

    run_in_test_process([&]
    {
        /* Wait until the clients have created a surface */
        surface_created1.wait();
        surface_created2.wait();
        surface_created3.wait();

        /* Shut down the server */
        shutdown_server_process();

        /* Wait until we have checked the resources in the server process */
        while (!resources_freed_failure.is_set() && !resources_freed_success.is_set())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        /* Fail if the resources have not been freed */
        if (resources_freed_failure.is_set())
            ADD_FAILURE();

        /* Notify the clients that we are done (we only need to set the flag once) */
        server_done.set();

        wait_for_shutdown_client_processes();
    });

    /*
     * Check that all resources are freed after destroying the server config.
     * Note that these checks are run multiple times: in the server process,
     * in each of the client processes and in the test process. We only care about
     * the results in the server process (in the other cases the checks will
     * succeed anyway, since we are not using the config object).
     */
    std::weak_ptr<mir::graphics::Display> display = server_config->the_display();
    std::weak_ptr<mir::compositor::Compositor> compositor = server_config->the_compositor();
    std::weak_ptr<mir::frontend::Connector> connector = server_config->the_connector();
    std::weak_ptr<mir::input::InputManager> input_manager = server_config->the_input_manager();

    server_config.reset();

    EXPECT_EQ(0, display.use_count());
    EXPECT_EQ(0, compositor.use_count());
    EXPECT_EQ(0, connector.use_count());
    EXPECT_EQ(0, input_manager.use_count());

    if (display.use_count() != 0 ||
        compositor.use_count() != 0 ||
        connector.use_count() != 0 ||
        input_manager.use_count() != 0)
    {
        resources_freed_failure.set();
    }
    else
    {
        resources_freed_success.set();
    }
}

TEST(ServerShutdownWithThreadException,
     server_releases_resources_on_abnormal_input_thread_termination)
{
    auto server_config = std::make_shared<mtf::FakeEventHubServerConfiguration>();
    auto fake_event_hub = server_config->the_fake_event_hub();

    std::thread server{
        [&server_config]
        {
            EXPECT_THROW(
                mir::run_mir(*server_config, [](mir::DisplayServer&){}),
                std::runtime_error);
        }};

    fake_event_hub->throw_exception_in_next_get_events();
    server.join();

    std::weak_ptr<mir::graphics::Display> display = server_config->the_display();
    std::weak_ptr<mir::compositor::Compositor> compositor = server_config->the_compositor();
    std::weak_ptr<mir::frontend::Connector> connector = server_config->the_connector();
    std::weak_ptr<mir::input::InputManager> input_manager = server_config->the_input_manager();

    server_config.reset();

    EXPECT_EQ(0, display.use_count());
    EXPECT_EQ(0, compositor.use_count());
    EXPECT_EQ(0, connector.use_count());
    EXPECT_EQ(0, input_manager.use_count());
}

// This also acts as a regression test for LP: #1378740
TEST(ServerShutdownWithThreadException,
     server_releases_resources_on_abnormal_main_thread_termination)
{
    // Use the FakeEventHubServerConfiguration to get the production input components
    // (with the exception of EventHub, of course).
    auto server_config = std::make_shared<mtf::FakeEventHubServerConfiguration>();

    std::thread server{
        [&]
        {
            EXPECT_THROW(
                mir::run_mir(*server_config,
                    [server_config](mir::DisplayServer&)
                    {
                        server_config->the_main_loop()->enqueue(
                            server_config.get(),
                            [] { throw std::runtime_error(""); });
                    }),
                std::runtime_error);
        }};

    server.join();

    std::weak_ptr<mir::graphics::Display> display = server_config->the_display();
    std::weak_ptr<mir::compositor::Compositor> compositor = server_config->the_compositor();
    std::weak_ptr<mir::frontend::Connector> connector = server_config->the_connector();
    std::weak_ptr<mir::input::InputManager> input_manager = server_config->the_input_manager();

    server_config.reset();

    EXPECT_EQ(0, display.use_count());
    EXPECT_EQ(0, compositor.use_count());
    EXPECT_EQ(0, connector.use_count());
    EXPECT_EQ(0, input_manager.use_count());
}
