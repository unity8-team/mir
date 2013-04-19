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
#include <mir/shell/session.h>
#include <mir/shell/session_container.h>
#include <mir/shell/focus_setter.h>
#include <cstdio>
#include <thread>
#include <boost/exception/diagnostic_information.hpp>

namespace mf = mir::frontend;
namespace msh = mir::shell;

int SystemCompositor::run(int argc, char const* argv[])
{
    dm_connection.start();

    dm_connection.send_ready();

    config = std::make_shared<mir::DefaultServerConfiguration>(argc, argv);

    try
    {
        mir::run_mir(*config, [](mir::DisplayServer&) {/* empty init */});
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
}
