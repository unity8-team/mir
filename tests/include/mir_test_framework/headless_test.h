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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_TEST_FRAMEWORK_HEADLESS_TEST_H_
#define MIR_TEST_FRAMEWORK_HEADLESS_TEST_H_

#include "mir_test_framework/temporary_environment_value.h"

#include "mir/server.h"

#include <gtest/gtest.h>

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace mir_test_framework
{
/** Basic fixture for tests that don't use graphics hardware.
 *  This provides a mechanism for temporarily setting environment variables.
 *  It automatically sets "MIR_SERVER_PLATFORM_GRAPHICS_LIB" to "libmirplatformstub.so"
 *  as the tests do not hit the graphics hardware.
 */
class HeadlessTest : public ::testing::Test
{
public:
    HeadlessTest();
    ~HeadlessTest() noexcept;

    void add_to_environment(char const* key, char const* value);

    /// Starts the server on a new thread
    void start_server();

    /// Stops the server and joins thread
    void stop_server();

    /// \return a connection string for a new client to connect to the server
    auto new_connection() -> std::string;

    /// \return a connection string for a client to connect to the server
    auto connection(mir::Fd fd) -> std::string;

    mir::Server server;

private:
    std::list<TemporaryEnvironmentValue> env;
    std::thread server_thread;

    std::list<mir::Fd> connections;

    std::mutex mutex;
    std::condition_variable started;
    bool server_running{false};
};

std::string const& test_socket_file();
}

#endif /* MIR_TEST_FRAMEWORK_HEADLESS_TEST_H_ */
