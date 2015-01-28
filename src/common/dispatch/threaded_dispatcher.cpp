/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir/dispatch/threaded_dispatcher.h"
#include "mir/dispatch/dispatchable.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <system_error>
#include <signal.h>
#include <boost/exception/all.hpp>
#include <algorithm>

namespace md = mir::dispatch;

thread_local bool md::ThreadedDispatcher::running;

class md::ThreadedDispatcher::ThreadShutdownRequestHandler : public md::Dispatchable
{
public:
    ThreadShutdownRequestHandler(md::ThreadedDispatcher& dispatcher)
        : dispatcher{dispatcher}
    {
        int pipefds[2];
        if (pipe2(pipefds, O_NONBLOCK) < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to create shutdown pipe for IO thread"}));
        }

        terminate_read_fd = mir::Fd{pipefds[0]};
        terminate_write_fd = mir::Fd{pipefds[1]};
    }

    mir::Fd watch_fd() const override
    {
        return terminate_read_fd;
    }

    bool dispatch(md::FdEvents events) override
    {
        char dummy;
        if (::read(terminate_read_fd, &dummy, sizeof(dummy)) != sizeof(dummy))
        {
            // We'll get a successful 0-length read when the write end is closed;
            // this is fine, it just means we're shutting everything down.
            if (!(events & md::FdEvent::remote_closed))
            {
                BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                         std::system_category(),
                                                         "Failed to clear shutdown notification"}));
            }
        }
        dispatcher.running = false;

        // Even if the remote end has been closed we're still dispatchable -
        // we want the other dispatch threads to pick up the shutdown signal.
        return true;
    }

    md::FdEvents relevant_events() const override
    {
        return md::FdEvent::readable | md::FdEvent::remote_closed;
    }

    void terminate_one_thread()
    {
        char dummy{0};
        if (::write(terminate_write_fd, &dummy, sizeof(dummy)) != sizeof(dummy))
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to trigger thread shutdown"}));
        }
    }

    void terminate_all_threads()
    {
        terminate_write_fd = mir::Fd{};
    }
private:
    md::ThreadedDispatcher& dispatcher;
    mir::Fd terminate_read_fd;
    mir::Fd terminate_write_fd;
};

md::ThreadedDispatcher::ThreadedDispatcher(std::shared_ptr<md::Dispatchable> const& dispatchee)
    : thread_exiter{std::make_shared<ThreadShutdownRequestHandler>(std::ref(*this))}
{

    // We rely on exactly one thread at a time getting a shutdown message
    dispatcher.add_watch(thread_exiter, md::DispatchReentrancy::sequential);

    // But our target dispatchable is welcome to be dispatched on as many threads
    // as desired.
    dispatcher.add_watch(dispatchee, md::DispatchReentrancy::reentrant);

    threadpool.emplace_back(&dispatch_loop, std::ref(*this));
}

md::ThreadedDispatcher::~ThreadedDispatcher() noexcept
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};

    thread_exiter->terminate_all_threads();

    for (auto& thread : threadpool)
    {
        thread.join();
    }
}

void md::ThreadedDispatcher::add_thread()
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};
    threadpool.emplace_back(&dispatch_loop, std::ref(*this));
}

void md::ThreadedDispatcher::remove_thread()
{
    // First we wake a thread, we don't care which...
    thread_exiter->terminate_one_thread();

    // ...now we wait for a thread to die and tell us its ID...
    std::unique_lock<decltype(terminating_thread_mutex)> lock{terminating_thread_mutex};
    if (!thread_terminating.wait_for (lock,
                                      std::chrono::seconds{1},
                                      [this]() { return terminating_thread_id != std::thread::id{}; }))
    {
        BOOST_THROW_EXCEPTION((std::runtime_error{"Thread failed to shutdown"}));
    }

    // ...finally, find that thread in our vector, join() it, then remove it.
    std::lock_guard<decltype(thread_pool_mutex)> threadpool_lock{thread_pool_mutex};

    auto dying_thread = std::find_if(threadpool.begin(),
                                     threadpool.end(),
                                     [this](std::thread const& candidate)
    {
            return candidate.get_id() == terminating_thread_id;
    });
    dying_thread->join();
    threadpool.erase(dying_thread);

    terminating_thread_id = std::thread::id{};
}

void md::ThreadedDispatcher::dispatch_loop(ThreadedDispatcher& me)
{
    sigset_t all_signals;
    sigfillset(&all_signals);

    if (auto error = pthread_sigmask(SIG_BLOCK, &all_signals, NULL))
        BOOST_THROW_EXCEPTION((std::system_error{error,
                                                 std::system_category(),
                                                 "Failed to block signals on IO thread"}));

    running = true;

    struct pollfd waiter;
    waiter.fd = me.dispatcher.watch_fd();
    waiter.events = POLL_IN;
    while (running)
    {
        if (poll(&waiter, 1, -1) < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to wait for event"}));
        }
        me.dispatcher.dispatch(md::FdEvent::readable);
    }

    {
        std::lock_guard<decltype(terminating_thread_mutex)> lock{me.terminating_thread_mutex};
        me.terminating_thread_id = std::this_thread::get_id();
    }
    me.thread_terminating.notify_one();
}
