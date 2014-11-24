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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/compositor/compositor.h"
#include "mir/frontend/connector.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/display_configuration_policy.h"
#include "mir/server_action_queue.h"
#include "mir/graphics/event_handler_register.h"
#include "mir/server_status_listener.h"

#include "mir_test/pipe.h"
#include "mir_test/wait_condition.h"
#include "mir_test_framework/testing_server_configuration.h"
#include "mir_test_doubles/mock_input_manager.h"
#include "mir_test_doubles/mock_input_dispatcher.h"
#include "mir_test_doubles/mock_compositor.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/mock_server_status_listener.h"
#include "mir/run_mir.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <atomic>
#include <thread>
#include <chrono>

#include <sys/types.h>
#include <unistd.h>

namespace mi = mir::input;
namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mf = mir::frontend;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{

class MockConnector : public mf::Connector
{
public:
    MOCK_METHOD0(start, void());
    MOCK_METHOD0(stop, void());
    /*
     * We don't have expectations for these, so use stubs
     * to silence gmock warnings.
     */
    int client_socket_fd() const override { return 0; }
    int client_socket_fd(std::function<void(std::shared_ptr<mf::Session> const&)> const&) const override { return 0; }
    void remove_endpoint() const {}
};

class MockDisplay : public mtd::NullDisplay
{
public:
    MockDisplay(std::shared_ptr<mg::Display> const& display,
                int pause_signal, int resume_signal, int fd)
        : display{display},
          pause_signal{pause_signal},
          resume_signal{resume_signal},
          fd{fd},
          pause_handler_invoked_{false},
          resume_handler_invoked_{false},
          conf_change_handler_invoked_{false}
    {
    }

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f) override
    {
        display->for_each_display_buffer(f);
    }

    std::unique_ptr<mg::DisplayConfiguration> configuration() const
    {
        return std::unique_ptr<mg::DisplayConfiguration>(
            new mtd::NullDisplayConfiguration
        );
    }

    MOCK_METHOD1(configure, void(mg::DisplayConfiguration const&));

    void register_configuration_change_handler(
        mg::EventHandlerRegister& handlers,
        mg::DisplayConfigurationChangeHandler const& conf_change_handler) override
    {
        handlers.register_fd_handler(
            {fd},
            this,
            [this,conf_change_handler](int fd)
            {
                char c;
                if (read(fd, &c, 1) == 1)
                {
                    conf_change_handler();
                    conf_change_handler_invoked_ = true;
                }
            });
    }

    void register_pause_resume_handlers(
        mg::EventHandlerRegister& handlers,
        mg::DisplayPauseHandler const& pause_handler,
        mg::DisplayResumeHandler const& resume_handler) override
    {
        handlers.register_signal_handler(
            {pause_signal},
            [this,pause_handler](int)
            {
                pause_handler();
                pause_handler_invoked_ = true;
            });
        handlers.register_signal_handler(
            {resume_signal},
            [this,resume_handler](int)
            {
                resume_handler();
                resume_handler_invoked_ = true;
            });
    }

    MOCK_METHOD0(pause, void());
    MOCK_METHOD0(resume, void());

    bool pause_handler_invoked()
    {
        return pause_handler_invoked_.exchange(false);
    }

    bool resume_handler_invoked()
    {
        return resume_handler_invoked_.exchange(false);
    }

    bool conf_change_handler_invoked()
    {
        return conf_change_handler_invoked_.exchange(false);
    }

private:
    std::shared_ptr<mg::Display> const display;
    int const pause_signal;
    int const resume_signal;
    int const fd;
    std::atomic<bool> pause_handler_invoked_;
    std::atomic<bool> resume_handler_invoked_;
    std::atomic<bool> conf_change_handler_invoked_;
};

class ServerConfig : public mtf::TestingServerConfiguration
{
public:
    std::shared_ptr<mi::InputManager> the_input_manager() override
    {
        if (!mock_input_manager)
        {
            mock_input_manager = std::make_shared<mtd::MockInputManager>();

            EXPECT_CALL(*mock_input_manager, start()).Times(1);
            EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        }

        return mock_input_manager;
    }

    std::shared_ptr<mc::Compositor> the_compositor() override
    {
        if (!mock_compositor)
        {
            mock_compositor = std::make_shared<mtd::MockCompositor>();

            EXPECT_CALL(*mock_compositor, start()).Times(1);
            EXPECT_CALL(*mock_compositor, stop()).Times(1);
        }

        return mock_compositor;
    }

private:
    std::shared_ptr<mtd::MockInputManager> mock_input_manager;
    std::shared_ptr<mtd::MockCompositor> mock_compositor;
};


class TestMainLoopServerConfig : public mtf::TestingServerConfiguration
{
public:
    TestMainLoopServerConfig()
        : pause_signal{SIGUSR1}, resume_signal{SIGUSR2}
    {
    }

    std::shared_ptr<mg::Display> the_display() override
    {
        if (!mock_display)
        {
            auto display = mtf::TestingServerConfiguration::the_display();
            mock_display = std::make_shared<MockDisplay>(display,
                                                         pause_signal,
                                                         resume_signal,
                                                         p.read_fd());
        }

        return mock_display;
    }

    std::shared_ptr<mc::Compositor> the_compositor() override
    {
        if (!mock_compositor)
            mock_compositor = std::make_shared<mtd::MockCompositor>();

        return mock_compositor;
    }

    std::shared_ptr<mf::Connector> the_connector() override
    {
        if (!mock_connector)
            mock_connector = std::make_shared<MockConnector>();

        return mock_connector;
    }

    std::shared_ptr<mi::InputManager> the_input_manager() override
    {
        if (!mock_input_manager)
            mock_input_manager = std::make_shared<mtd::MockInputManager>();

        return mock_input_manager;
    }

    std::shared_ptr<mi::InputDispatcher> the_input_dispatcher() override
    {
        if (!mock_input_dispatcher)
            mock_input_dispatcher = std::make_shared<mtd::MockInputDispatcher>();

        return mock_input_dispatcher;
    }

    std::shared_ptr<MockDisplay> the_mock_display()
    {
        the_display();
        return mock_display;
    }

    std::shared_ptr<mtd::MockCompositor> the_mock_compositor()
    {
        the_compositor();
        return mock_compositor;
    }

    std::shared_ptr<MockConnector> the_mock_connector()
    {
        the_connector();
        return mock_connector;
    }

    std::shared_ptr<mtd::MockInputManager> the_mock_input_manager()
    {
        the_input_manager();
        return mock_input_manager;
    }

    std::shared_ptr<mtd::MockInputDispatcher> the_mock_input_dispatcher()
    {
        the_input_dispatcher();
        return mock_input_dispatcher;
    }

    void emit_pause_event_and_wait_for_handler()
    {
        kill(getpid(), pause_signal);
        while (!mock_display->pause_handler_invoked())
            std::this_thread::sleep_for(std::chrono::microseconds{500});
    }

    void emit_resume_event_and_wait_for_handler()
    {
        kill(getpid(), resume_signal);
        while (!mock_display->resume_handler_invoked())
            std::this_thread::sleep_for(std::chrono::microseconds{500});
    }

    void emit_configuration_change_event_and_wait_for_handler()
    {
        if (write(p.write_fd(), "a", 1)) {}
        while (!mock_display->conf_change_handler_invoked())
            std::this_thread::sleep_for(std::chrono::microseconds{500});
    }

    void wait_for_server_actions_to_finish()
    {
        mt::WaitCondition last_action_done;
        the_server_action_queue()->enqueue(&last_action_done,
            [&] { last_action_done.wake_up_everyone(); });

        last_action_done.wait_for_at_most_seconds(5);
    }

private:
    std::shared_ptr<mtd::MockCompositor> mock_compositor;
    std::shared_ptr<MockDisplay> mock_display;
    std::shared_ptr<MockConnector> mock_connector;
    std::shared_ptr<mtd::MockInputManager> mock_input_manager;
    std::shared_ptr<mtd::MockInputDispatcher> mock_input_dispatcher;

    mt::Pipe p;
    int const pause_signal;
    int const resume_signal;
};

class TestServerStatusListenerConfig : public TestMainLoopServerConfig
{
public:
    std::shared_ptr<mir::ServerStatusListener> the_server_status_listener() override
    {
        if (!mock_server_status_listener)
            mock_server_status_listener = std::make_shared<mtd::MockServerStatusListener>();

        return mock_server_status_listener;
    }

    std::shared_ptr<mtd::MockServerStatusListener> the_mock_server_status_listener()
    {
        the_server_status_listener();
        return mock_server_status_listener;
    }

private:
    std::shared_ptr<mtd::MockServerStatusListener> mock_server_status_listener;
};

}

TEST(DisplayServerMainLoopEvents, DisplayServerShutsDownProperlyOnSigint)
{
    ServerConfig server_config;

    mir::run_mir(server_config,
                 [](mir::DisplayServer&)
                 {
                    kill(getpid(), SIGINT);
                 });
}

TEST(DisplayServerMainLoopEvents, DisplayServerShutsDownProperlyOnSigterm)
{
    ServerConfig server_config;

    mir::run_mir(server_config,
                 [](mir::DisplayServer&)
                 {
                    kill(getpid(), SIGTERM);
                 });
}

TEST(DisplayServerMainLoopEvents, DisplayServerComponentsPauseAndResume)
{
    using namespace testing;

    TestMainLoopServerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();

    {
        InSequence s;

        /* Start */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Pause */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
        EXPECT_CALL(*mock_display, pause()).Times(1);

        /* Resume */
        EXPECT_CALL(*mock_display, resume()).Times(1);
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&server_config](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_pause_event_and_wait_for_handler();
                            server_config.emit_resume_event_and_wait_for_handler();
                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}

TEST(DisplayServerMainLoopEvents, DisplayServerQuitsWhenPaused)
{
    using namespace testing;

    TestMainLoopServerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();

    {
        InSequence s;

        /* Start */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Pause */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
        EXPECT_CALL(*mock_display, pause()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&server_config](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_pause_event_and_wait_for_handler();
                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}

TEST(DisplayServerMainLoopEvents, DisplayServerAttemptsToContinueOnPauseFailure)
{
    using namespace testing;

    TestMainLoopServerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();

    {
        InSequence s;

        /* Start */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Pause failure */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
        EXPECT_CALL(*mock_display, pause())
            .WillOnce(Throw(std::runtime_error("")));

        /* Attempt to continue */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&server_config](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_pause_event_and_wait_for_handler();
                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}

TEST(DisplayServerMainLoopEvents, DisplayServerHandlesConfigurationChange)
{
    using namespace testing;

    TestMainLoopServerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();

    {
        InSequence s;

        /* Start */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Change configuration */
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_display, configure(_)).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_configuration_change_event_and_wait_for_handler();
                            server_config.wait_for_server_actions_to_finish();
                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}

TEST(DisplayServerMainLoopEvents, PostponesConfigurationWhenPaused)
{
    using namespace testing;

    TestMainLoopServerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();

    {
        InSequence s;

        /* Start */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);

        /* Pause event */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
        EXPECT_CALL(*mock_display, pause()) .Times(1);

        /* Resume event */
        EXPECT_CALL(*mock_display, resume()).Times(1);
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);

        /* Change configuration (after resuming) */
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_display, configure(_)).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_pause_event_and_wait_for_handler();
                            server_config.emit_configuration_change_event_and_wait_for_handler();
                            server_config.emit_resume_event_and_wait_for_handler();
                            server_config.wait_for_server_actions_to_finish();

                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}

TEST(DisplayServerMainLoopEvents, ServerStatusListener)
{
    using namespace testing;

    TestServerStatusListenerConfig server_config;

    auto mock_compositor = server_config.the_mock_compositor();
    auto mock_display = server_config.the_mock_display();
    auto mock_connector = server_config.the_mock_connector();
    auto mock_input_manager = server_config.the_mock_input_manager();
    auto mock_input_dispatcher = server_config.the_mock_input_dispatcher();
    auto mock_server_status_listener = server_config.the_mock_server_status_listener();

    {
        InSequence s;

        /* "started" is emitted after all components have been started */
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);
        EXPECT_CALL(*mock_server_status_listener, started()).Times(1);

        /* "paused" is emitted after all components have been paused/stopped */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
        EXPECT_CALL(*mock_display, pause()).Times(1);
        EXPECT_CALL(*mock_server_status_listener, paused()).Times(1);

        /* "resumed" is emitted after all components have been resumed/started */
        EXPECT_CALL(*mock_display, resume()).Times(1);
        EXPECT_CALL(*mock_connector, start()).Times(1);
        EXPECT_CALL(*mock_input_manager, start()).Times(1);
        EXPECT_CALL(*mock_input_dispatcher, start()).Times(1);
        EXPECT_CALL(*mock_compositor, start()).Times(1);
        EXPECT_CALL(*mock_server_status_listener, resumed()).Times(1);

        /* Stop */
        EXPECT_CALL(*mock_input_dispatcher, stop()).Times(1);
        EXPECT_CALL(*mock_input_manager, stop()).Times(1);
        EXPECT_CALL(*mock_compositor, stop()).Times(1);
        EXPECT_CALL(*mock_connector, stop()).Times(1);
    }

    mir::run_mir(server_config,
                 [&server_config](mir::DisplayServer&)
                 {
                    std::thread t{
                        [&]
                        {
                            server_config.emit_pause_event_and_wait_for_handler();
                            server_config.emit_resume_event_and_wait_for_handler();
                            kill(getpid(), SIGTERM);
                        }};
                    t.detach();
                 });
}
