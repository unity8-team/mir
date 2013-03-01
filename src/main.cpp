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

#include "mir/display_server.h"
#include "mir/default_server_configuration.h"

#include <thread>
#include <boost/program_options/errors.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include <csignal>
#include <iostream>

namespace
{
// TODO: Get rid of the volatile-hack here and replace it with
// some sane atomic-pointer once we have left GCC 4.4 behind.
mir::DisplayServer* volatile signal_display_server;
}

namespace mir
{
extern "C"
{
void signal_terminate (int )
{
    while (!signal_display_server)
        std::this_thread::yield();

    signal_display_server->stop();
}
}
}

namespace
{
void run_mir(mir::ServerConfiguration& config)
{

    signal(SIGINT, mir::signal_terminate);
    signal(SIGTERM, mir::signal_terminate);
    signal(SIGPIPE, SIG_IGN);

    mir::DisplayServer server(config);

    signal_display_server = &server;

    server.start();
}
}

int main(int argc, char const* argv[])
try
{
    mir::DefaultServerConfiguration config(argc, argv);

    run_mir(config);
    return 0;
}
catch (boost::program_options::error const&)
{
    // Can't run with these options - but no need for additional output
    return 1;
}
catch (std::exception const& error)
{
    std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
    return 1;
}
