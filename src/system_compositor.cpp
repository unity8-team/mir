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
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 */

#include "system_compositor.h"

#include <mir/run_mir.h>
#include <mir/abnormal_exit.h>
#include <mir/default_server_configuration.h>
#include <mir/main_loop.h>
#include <mir/shell/session.h>
#include <mir/shell/session_container.h>
#include <mir/shell/focus_setter.h>
#include <cstdio>
#include <thread>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/asio.hpp>

namespace mf = mir::frontend;
namespace msh = mir::shell;

struct SignalHandler
{
    SignalHandler(boost::asio::io_service& io,
                  std::initializer_list<int> signals,
                  std::function<void(int)> const& handler)
        : signal_set{io},
          handler{handler}
    {
        for (auto sig : signals)
            signal_set.add(sig);
    }

    void async_wait()
    {
        signal_set.async_wait(
            std::bind(&SignalHandler::handle, this,
                      std::placeholders::_1, std::placeholders::_2));
    }

    void handle(boost::system::error_code err, int sig)
    {
        if (!err)
        {
            handler(sig);
            signal_set.async_wait(
                std::bind(&SignalHandler::handle, this,
                          std::placeholders::_1, std::placeholders::_2));
        }
    }

    boost::asio::signal_set signal_set;
    std::function<void(int)> handler;
};

struct MainLoop : mir::MainLoop
{
    MainLoop(boost::asio::io_service& io_service) :
        io_service(io_service)
    {
    }

    ~MainLoop() noexcept(true) {}

    void run()
    {
        io_service.run();
    }

    void stop()
    {
        io_service.stop();
    }

    void register_signal_handler(std::initializer_list<int> signals, std::function<void(int)> const& handler)
    {
        auto sig_handler = std::unique_ptr<SignalHandler>{new SignalHandler{io_service, signals, handler}};

        sig_handler->async_wait();

        signal_handlers.push_back(std::move(sig_handler));
    }

    boost::asio::io_service& io_service;
    std::vector<std::unique_ptr<SignalHandler>> signal_handlers;
};

struct Configuration : mir::DefaultServerConfiguration
{
    Configuration(int argc, char const* argv[], boost::asio::io_service& io_service) :
      DefaultServerConfiguration(argc, argv),
      io_service(io_service)
    {
    }
  
    std::shared_ptr<mir::MainLoop> the_main_loop()
    {
        return main_loop(
            [this]()
            {
                return std::make_shared<MainLoop>(io_service);
            });
    }
  
    boost::asio::io_service& io_service;
};

int SystemCompositor::run(int argc, char const* argv[])
{
    dm_connection.start();

    config = std::make_shared<Configuration>(argc, argv, io_service);

    try
    {
        mir::run_mir(*config, [this](mir::DisplayServer&)
        {
            dm_connection.send_ready();          
        });
        return 0;
    }
    catch (mir::AbnormalExit const& error)
    {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    catch (std::exception const& error)
    {
        std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
        return 1;
    }
}

void SystemCompositor::set_active_session(std::string client_name)
{
    std::cerr << "set_active_session" << std::endl;

    std::shared_ptr<msh::Session> session;
    config->the_shell_session_container()->for_each([&client_name, &session](std::shared_ptr<msh::Session> const& s)
    {
        if (s->name() == client_name)
            session = s;
    });

    if (session)
        config->the_shell_focus_setter()->set_focus_to(session);
    else
        std::cerr << "Unable to set active session, unknown client name " << client_name << std::endl;
}
