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
#include <mir/shell/session.h>
#include <mir/shell/session_container.h>
#include <mir/shell/focus_setter.h>

namespace msh = mir::shell;

SystemCompositor::SystemCompositor(int from_dm_fd, int to_dm_fd) :
        dm_connection(io_service, from_dm_fd, to_dm_fd)
{
}

void SystemCompositor::run(int argc, char const* argv[])
{
    config = std::make_shared<mir::DefaultServerConfiguration>(argc, argv);

    struct ScopeGuard
    {
        explicit ScopeGuard(boost::asio::io_service& io_service) : io_service(io_service) {}
        ~ScopeGuard() { io_service.stop(); if (thread.joinable()) thread.join(); }

        boost::asio::io_service& io_service;
        std::thread thread;
    } guard(io_service);

    mir::run_mir(*config, [&](mir::DisplayServer&)
        {
            guard.thread = std::thread(&SystemCompositor::main, this);
        });
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
