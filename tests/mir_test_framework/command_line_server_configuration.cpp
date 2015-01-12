/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mir_test_framework/command_line_server_configuration.h"
#include "mir/options/default_configuration.h"
#include "mir/server.h"
#include "mir/logging/logger.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

namespace mo=mir::options;

namespace
{
int argc;
char const** argv;
}

auto mir_test_framework::configuration_from_commandline()
-> std::shared_ptr<mo::DefaultConfiguration>
{
    return std::make_shared<mo::DefaultConfiguration>(::argc, ::argv);
}

void mir_test_framework::configure_from_commandline(mir::Server& server)
{
    server.set_command_line(::argc, ::argv);
}

int main(int argc, char** argv)
{
    // Override this standard gtest message
    std::cout << "Running main() from " << basename(__FILE__) << std::endl;

    // Limit log output during tests to error messages only
    auto limit = mir::logging::Severity::error;
    auto logger = mir::logging::get_logger(limit);
    // limit would only have been applied if logger didn't exist yet. So now
    // force it:
    logger->set_level(limit);

    ::testing::InitGoogleTest(&argc, argv);

    ::argv = const_cast<char const**>(argv);
    ::argc = argc;

    return RUN_ALL_TESTS();
}
