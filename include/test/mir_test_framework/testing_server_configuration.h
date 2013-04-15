/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_TEST_TESTING_SERVER_CONFIGURATION_H_
#define MIR_TEST_TESTING_SERVER_CONFIGURATION_H_

#include "mir/default_server_configuration.h"

namespace mir
{
class DisplayServer;
}

namespace mir_test_framework
{
using namespace mir;

class TestingServerConfiguration : public DefaultServerConfiguration
{
public:
    TestingServerConfiguration();

    // Code to run in server process
    virtual void exec();

    // Code to run in server process after server exits
    virtual void on_exit();

    // TODO can we remove this function and default to real graphics in tests?
    std::shared_ptr<graphics::Platform> the_graphics_platform();
    std::shared_ptr<graphics::Renderer> the_renderer();
    // We override the_input_manager in the default server configuration
    // to avoid starting and stopping the full android input stack for tests
    // which do not leverage input.
    virtual std::shared_ptr<input::InputManager> the_input_manager();

    virtual std::string the_socket_file() const;
    using DefaultServerConfiguration::the_options;


private:
    std::shared_ptr<graphics::Platform> graphics_platform;
};

std::string const& test_socket_file();
}

#endif /* MIR_TEST_TESTING_SERVER_CONFIGURATION_H_ */
