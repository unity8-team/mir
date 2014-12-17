/*
 * Copyright © 2012-2014 Canonical Ltd.
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

#include "server_example_log_options.h"
#include "server_example_input_event_filter.h"
#include "server_example_input_filter.h"
#include "server_example_fullscreen_placement_strategy.h"
#include "server_example_display_configuration_policy.h"
#include "server_example_host_lifecycle_event_listener.h"

#include "mir/server.h"
#include "mir/main_loop.h"

#include "mir/report_exception.h"
#include "mir/options/option.h"

#include <chrono>
#include <cstdlib>

namespace me = mir::examples;

///\example server_example.cpp
/// A simple server illustrating several customisations

namespace
{
void add_launcher_option_to(mir::Server& server)
{
    static const char* const launch_child_opt = "launch-client";
    static const char* const launch_client_descr = "system() command to launch client";

    server.add_configuration_option(launch_child_opt, launch_client_descr, mir::OptionType::string);
    server.add_init_callback([&]
    {
        const auto options = server.get_options();
        if (options->is_set(launch_child_opt))
        {
            auto ignore = std::system((options->get<std::string>(launch_child_opt) + "&").c_str());
            (void)(ignore);
        }
    });
}

void add_timeout_option_to(mir::Server& server)
{
    static const char* const timeout_opt = "timeout";
    static const char* const timeout_descr = "Seconds to run before exiting";

    server.add_configuration_option(timeout_opt, timeout_descr, mir::OptionType::integer);

    server.add_init_callback([&]
    {
        const auto options = server.get_options();
        if (options->is_set(timeout_opt))
        {
            static auto const exit_action = server.the_main_loop()->notify_in(
                std::chrono::seconds(options->get<int>(timeout_opt)),
                [&] { server.stop(); });
        }
    });
}
}

int main(int argc, char const* argv[])
try
{
    mir::Server server;

    // Create some input filters (we need to keep them or they deactivate)
    auto const quit_filter = me::make_quit_filter_for(server);
    auto const printing_filter = me::make_printing_input_filter_for(server);

    // Add example options for display layout, logging, launching clients and timeout
    me::add_display_configuration_options_to(server);
    me::add_log_host_lifecycle_option_to(server);
    me::add_glog_options_to(server);
    me::add_fullscreen_option_to(server);
    add_launcher_option_to(server);
    add_timeout_option_to(server);

    // Provide the command line and run the server
    server.set_command_line(argc, argv);
    server.apply_settings();
    server.run();
    return server.exited_normally() ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch (...)
{
    mir::report_exception();
    return EXIT_FAILURE;
}
