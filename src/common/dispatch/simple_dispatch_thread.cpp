/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/dispatch/simple_dispatch_thread.h"
#include "mir/dispatch/dispatchable.h"

#include <poll.h>
#include <unistd.h>
#include <system_error>
#include <signal.h>
#include <boost/exception/all.hpp>

namespace md = mir::dispatch;

thread_local bool md::SimpleDispatchThread::running;

namespace
{
void clear_dummy_signal(mir::Fd const& fd)
{
    char dummy;
    ::read(fd, &dummy, sizeof(dummy));
}

}

md::SimpleDispatchThread::SimpleDispatchThread(std::shared_ptr<md::Dispatchable> const& dispatchee)
{
    int pipefds[2];
    if (pipe(pipefds) < 0)
    {
        BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                 std::system_category(),
                                                 "Failed to create shutdown pipe for IO thread"}));
    }

    wakeup_fd = mir::Fd{pipefds[1]};
    mir::Fd const terminate_fd = mir::Fd{pipefds[0]};

    // We rely on exactly one thread at a time getting a shutdown message
    dispatcher.add_watch(terminate_fd,
                         [terminate_fd, this]()
                         {
                             clear_dummy_signal(terminate_fd);
                             running = false;
                         });

    // But our target dispatchable is welcome to be dispatched on as many threads
    // as desired.
    dispatcher.add_watch(dispatchee, md::DispatchReentrancy::reentrant);

    threadpool.emplace_back(&dispatch_loop, std::ref(dispatcher));
}

md::SimpleDispatchThread::~SimpleDispatchThread() noexcept
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};
    ::close(wakeup_fd);
    for (auto& thread : threadpool)
    {
        thread.join();
    }
}

void md::SimpleDispatchThread::add_thread()
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};
    threadpool.emplace_back(&dispatch_loop, std::ref(dispatcher));
}

void md::SimpleDispatchThread::dispatch_loop(md::Dispatchable& dispatcher)
{
    sigset_t all_signals;
    sigfillset(&all_signals);

    if (auto error = pthread_sigmask(SIG_BLOCK, &all_signals, NULL))
        BOOST_THROW_EXCEPTION((std::system_error{error,
                                                 std::system_category(),
                                                 "Failed to block signals on IO thread"}));

    running = true;

    struct pollfd waiter;
    waiter.fd = dispatcher.watch_fd();
    waiter.events = POLL_IN;
    while (running)
    {
        if (poll(&waiter, 1, -1) < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to wait for event"}));
        }
        dispatcher.dispatch(md::FdEvent::readable);
    }
}
