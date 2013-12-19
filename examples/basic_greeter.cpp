/*
 * Copyright © 2012, 2013 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com> 
 */

#include "server_configuration.h"

#include "translucent_outputs.h"

#include "mir/abnormal_exit.h"
#include "mir/frontend/connector.h"
#include "mir/options/option.h"
#include "mir/report_exception.h"
#include "mir/run_mir.h"

#include <iostream>

namespace mg = mir::graphics;
namespace me = mir::examples;

namespace
{
char const* const launch_child_opt = "launch-client";

struct ServerConfiguration : mir::examples::ServerConfiguration
{
    ServerConfiguration(int argc, char const** argv) :
        mir::examples::ServerConfiguration(argc, argv)
    {
        namespace po = boost::program_options;

        add_options()
            (launch_child_opt, po::value<std::string>(), "system() command to launch client");
    }

    std::shared_ptr<mg::DisplayConfigurationPolicy> the_display_configuration_policy() override
    {
        return display_configuration_policy(
            [this]() -> std::shared_ptr<mg::DisplayConfigurationPolicy>
            {
                return std::make_shared<me::TranslucentOutputs>(
                    me::ServerConfiguration::the_display_configuration_policy() 
                    );
            });
    }


    void launch_client()
    {
        if (the_options()->is_set(launch_child_opt))
        {
            char buffer[128] = {0};
            sprintf(buffer, "fd://%d", the_connector()->client_socket_fd());
            setenv("MIR_SOCKET", buffer, 1);
            auto ignore = system((the_options()->get(launch_child_opt, "") + "&").c_str());
            (void)ignore;
        }
    }

    using mir::examples::ServerConfiguration::the_options;
};
}

int main(int argc, char const* argv[])
try
{
    ServerConfiguration config(argc, argv);

    run_mir(config, [&](mir::DisplayServer&){ config.launch_client(); });
    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
