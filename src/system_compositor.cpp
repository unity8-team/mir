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

SystemCompositor::SystemCompositor(int from_dm_fd, int to_dm_fd) :
        dm_connection(io_service, from_dm_fd, to_dm_fd)
{
}

int SystemCompositor::run(int argc, char const* argv[])
{
    config = std::make_shared<mir::DefaultServerConfiguration>(argc, argv);

    auto return_value = 0;
    try
    {
        mir::run_mir(*config, [this](mir::DisplayServer&)
        {
            thread = std::thread(std::mem_fn(&SystemCompositor::main), this);
        });
    }
    catch (mir::AbnormalExit const& error)
    {
        std::cerr << error.what() << std::endl;
        return_value = 1;
    }
    catch (std::exception const& error)
    {
        std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
        return_value = 1;
    }

    io_service.stop();
    if (thread.joinable())
        thread.join();

    return return_value;
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

void SystemCompositor::main()
{
    dm_connection.set_handler(this);
    dm_connection.start();
    dm_connection.send_ready();

    io_service.run();
}
