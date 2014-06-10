/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/run_mir.h"
#include "mir/display_server.h"
#include "mir/main_loop.h"
#include "mir/server_configuration.h"
#include "mir/frontend/connector.h"
#include "mir/raii.h"
#include "mir/emergency_cleanup.h"

#include <exception>
#include <mutex>
#include <csignal>
#include <cstdlib>
#include <cassert>

namespace
{
auto const intercepted = { SIGQUIT, SIGABRT, SIGFPE, SIGSEGV, SIGBUS };

std::weak_ptr<mir::EmergencyCleanup> weak_emergency_cleanup;
std::exception_ptr termination_exception;
std::mutex termination_exception_mutex;

extern "C" void perform_emergency_cleanup()
{
    if (auto emergency_cleanup = weak_emergency_cleanup.lock())
    {
        weak_emergency_cleanup.reset();
        (*emergency_cleanup)();
    }
}

extern "C" { typedef void (*sig_handler)(int); }

volatile sig_handler old_handler[SIGUNUSED]  = { nullptr };

extern "C" void fatal_signal_cleanup(int sig)
{
    perform_emergency_cleanup();

    signal(sig, old_handler[sig]);
    raise(sig);
}
}

void mir::run_mir(ServerConfiguration& config, std::function<void(DisplayServer&)> init)
{
    DisplayServer* server_ptr{nullptr};
    {
        std::lock_guard<std::mutex> lock{termination_exception_mutex};
        termination_exception = nullptr;
    }
    auto main_loop = config.the_main_loop();

    main_loop->register_signal_handler(
        {SIGINT, SIGTERM},
        [&server_ptr](int)
        {
            assert(server_ptr);
            server_ptr->stop();
        });

    DisplayServer server(config);
    server_ptr = &server;

    weak_emergency_cleanup = config.the_emergency_cleanup();

    auto const raii = raii::paired_calls(
        [&]{ for (auto sig : intercepted) old_handler[sig] = signal(sig, fatal_signal_cleanup); },
        [&]{ for (auto sig : intercepted) signal(sig, old_handler[sig]); });

    init(server);
    server.run();

    std::lock_guard<std::mutex> lock{termination_exception_mutex};
    if (termination_exception)
        std::rethrow_exception(termination_exception);
}

void mir::terminate_with_current_exception()
{
    std::lock_guard<std::mutex> lock{termination_exception_mutex};
    if (!termination_exception)
    {
        termination_exception = std::current_exception();
        raise(SIGTERM);
    }
}
