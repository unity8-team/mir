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

#include "mir_test_framework/headless_test.h"
#include "mir_test_framework/command_line_server_configuration.h"

#include "mir/fd.h"
#include "mir/main_loop.h"
#include "mir/shared_library.h"
#include "mir/geometry/rectangle.h"
#include "mir/options/option.h"
#include "mir_test_doubles/null_logger.h"

#include <boost/throw_exception.hpp>

namespace geom = mir::geometry;
namespace ml = mir::logging;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{
const char* const mir_server_platform_graphics_lib = "MIR_SERVER_PLATFORM_GRAPHICS_LIB";

std::chrono::seconds const timeout{10};
}

mtf::HeadlessTest::HeadlessTest()
{
    configure_from_commandline(server);
    add_to_environment(mir_server_platform_graphics_lib, "libmirplatformstub.so");
    server.add_configuration_option(mtd::logging_opt, mtd::logging_descr, false);
    server.override_the_logger([&]()
        {
            std::shared_ptr<ml::Logger> result{};

            if (!server.get_options()->get<bool>(mtd::logging_opt))
                result = std::make_shared<mtd::NullLogger>();

            return result;
        });
}

void mtf::HeadlessTest::add_to_environment(char const* key, char const* value)
{
    env.emplace_back(key, value);
}

void mtf::HeadlessTest::start_server()
{
    server.add_init_callback([&]
        {
            auto const main_loop = server.the_main_loop();
            // By enqueuing the notification code in the main loop, we are
            // ensuring that the server has really and fully started before
            // leaving start_mir_server().
            main_loop->enqueue(
                this,
                [&]
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    server_running = true;
                    started.notify_one();
                });
        });

    server.apply_settings();

    server_thread = std::thread([&]
        {
            try
            {
                server.run();
            }
            catch (std::exception const& e)
            {
                FAIL() << e.what();
            }
            std::lock_guard<std::mutex> lock(mutex);
            server_running = false;
            started.notify_one();
        });

    std::unique_lock<std::mutex> lock(mutex);
    started.wait_for(lock, timeout, [&] { return server_running; });

    if (!server_running)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error{"Failed to start server thread"});
    }
}

void mtf::HeadlessTest::stop_server()
{
    connections.clear();
    server.stop();
    wait_for_server_exit();
}

void mtf::HeadlessTest::wait_for_server_exit()
{
    std::unique_lock<std::mutex> lock(mutex);
    started.wait_for(lock, timeout, [&] { return !server_running; });

    if (server_running)
    {
        BOOST_THROW_EXCEPTION(std::logic_error{"stop_server() failed to stop server"});
    }
}

mtf::HeadlessTest::~HeadlessTest() noexcept
{
    if (server_thread.joinable()) server_thread.join();
}

auto mtf::HeadlessTest::new_connection() -> std::string
{
    return connection(server.open_client_socket());
}

auto mtf::HeadlessTest::connection(mir::Fd fd) -> std::string
{
    char connect_string[64] = {0};
    connections.push_back(fd);
    sprintf(connect_string, "fd://%d", fd.operator int());
    return connect_string;
}

void mtf::HeadlessTest::initial_display_layout(std::vector<geom::Rectangle> const& display_rects)
{
    server_platform_graphics_lib.reset(new mir::SharedLibrary{getenv(mir_server_platform_graphics_lib)});

    typedef void (*SetDisplayRects)(std::unique_ptr<std::vector<geom::Rectangle>>&&);

    auto const set_display_rects =
        server_platform_graphics_lib->load_function<SetDisplayRects>("set_display_rects");

    set_display_rects(std::unique_ptr<std::vector<geom::Rectangle>>(new std::vector<geom::Rectangle>(display_rects)));
}

