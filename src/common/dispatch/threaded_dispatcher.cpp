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
#include "mir/thread_name.h"

#include "mir/raii.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <system_error>
#include <signal.h>
#include <boost/exception/all.hpp>
#include <algorithm>
#include <unordered_map>

namespace md = mir::dispatch;

class md::ThreadedDispatcher::ThreadShutdownRequestHandler : public md::Dispatchable
{
public:
    ThreadShutdownRequestHandler()
    {
        int pipefds[2];
        if (pipe2(pipefds, O_NONBLOCK | O_CLOEXEC) < 0)
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
        std::lock_guard<decltype(running_flag_guard)> lock{running_flag_guard};
        *running_flags.at(std::this_thread::get_id()) = false;

        // Even if the remote end has been closed we're still dispatchable -
        // we want the other dispatch threads to pick up the shutdown signal.
        return true;
    }

    md::FdEvents relevant_events() const override
    {
        return md::FdEvent::readable | md::FdEvent::remote_closed;
    }

    std::thread::id terminate_one_thread()
    {
        // First we tell a thread to die, any thread...
        char dummy{0};
        if (::write(terminate_write_fd, &dummy, sizeof(dummy)) != sizeof(dummy))
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to trigger thread shutdown"}));
        }

        // ...now we wait for a thread to die and tell us its ID...
        // We wait for a surprisingly long time because our threads are potentially blocked
        // in client code that we don't control.
        //
        // If the client is entirely unresponsive for a whole minute, it deserves to die.
        std::unique_lock<decltype(terminating_thread_mutex)> lock{terminating_thread_mutex};
        if (!thread_terminating.wait_for (lock,
                                          std::chrono::seconds{60},
                                          [this]() { return !terminating_threads.empty(); }))
        {
            BOOST_THROW_EXCEPTION((std::runtime_error{"Thread failed to shutdown"}));
        }

        auto killed_thread_id = terminating_threads.back();
        terminating_threads.pop_back();
        return killed_thread_id;
    }

    void terminate_all_threads()
    {
        terminate_write_fd = mir::Fd{};
    }

    void register_thread(bool& run_flag)
    {
        std::lock_guard<decltype(running_flag_guard)> lock{running_flag_guard};

        running_flags[std::this_thread::get_id()] = &run_flag;
    }

    void unregister_thread()
    {
        {
            std::lock_guard<decltype(terminating_thread_mutex)> lock{terminating_thread_mutex};
            terminating_threads.push_back(std::this_thread::get_id());
        }
        thread_terminating.notify_one();
        {
            std::lock_guard<decltype(running_flag_guard)> lock{running_flag_guard};

            if (running_flags.erase(std::this_thread::get_id()) != 1)
            {
                BOOST_THROW_EXCEPTION((std::logic_error{"Attempted to unregister a not-registered thread"}));
            }
        }
    }

private:
    mir::Fd terminate_read_fd;
    mir::Fd terminate_write_fd;

    std::mutex terminating_thread_mutex;
    std::condition_variable thread_terminating;
    std::vector<std::thread::id> terminating_threads;

    std::mutex running_flag_guard;
    std::unordered_map<std::thread::id, bool*> running_flags;
};

md::ThreadedDispatcher::ThreadedDispatcher(std::string const& name, std::shared_ptr<md::Dispatchable> const& dispatchee)
    : name_base{name},
      thread_exiter{std::make_shared<ThreadShutdownRequestHandler>()},
      dispatcher{std::make_shared<MultiplexingDispatchable>()}
{

    // We rely on exactly one thread at a time getting a shutdown message
    dispatcher->add_watch(thread_exiter, md::DispatchReentrancy::sequential);

    // But our target dispatchable is welcome to be dispatched on as many threads
    // as desired.
    dispatcher->add_watch(dispatchee, md::DispatchReentrancy::reentrant);

    threadpool.emplace_back(&dispatch_loop, name_base, thread_exiter, dispatcher);
}

md::ThreadedDispatcher::~ThreadedDispatcher() noexcept
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};

    thread_exiter->terminate_all_threads();

    for (auto& thread : threadpool)
    {
        if (thread.get_id() == std::this_thread::get_id())
        {
            // We're being destroyed from a thread currently in dispatch().
            // This is ok; the thread loop's shared_ptrs keep everything necessary
            // alive, and we'll just drop out of the end of the while(running) loop.
            //
            // However, we need to manually get the thread_exiter to mark the current
            // thread as no longer running, as we're not going to dispatch it again.
            thread_exiter->dispatch(md::FdEvent::remote_closed);
            thread.detach();
        }
        else
        {
            thread.join();
        }
    }
}

void md::ThreadedDispatcher::add_thread()
{
    std::lock_guard<decltype(thread_pool_mutex)> lock{thread_pool_mutex};
    threadpool.emplace_back(&dispatch_loop, name_base, thread_exiter, dispatcher);
}

void md::ThreadedDispatcher::remove_thread()
{
    auto terminated_thread_id = thread_exiter->terminate_one_thread();

    // Find that thread in our vector, join() it, then remove it.
    std::lock_guard<decltype(thread_pool_mutex)> threadpool_lock{thread_pool_mutex};

    auto dying_thread = std::find_if(threadpool.begin(),
                                     threadpool.end(),
                                     [this, &terminated_thread_id](std::thread const& candidate)
    {
            return candidate.get_id() == terminated_thread_id;
    });
    dying_thread->join();
    threadpool.erase(dying_thread);
}

void md::ThreadedDispatcher::dispatch_loop(std::string const& name,
                                           std::shared_ptr<ThreadShutdownRequestHandler> thread_register,
                                           std::shared_ptr<Dispatchable> dispatcher)
{
    sigset_t all_signals;
    sigfillset(&all_signals);

    if (auto error = pthread_sigmask(SIG_BLOCK, &all_signals, NULL))
        BOOST_THROW_EXCEPTION((std::system_error{error,
                                                 std::system_category(),
                                                 "Failed to block signals on IO thread"}));

    mir::set_thread_name(name);

    // This does not have to be std::atomic<bool> because thread_register is guaranteed to
    // only ever be dispatch()ed from one thread at a time.
    bool running{true};

    auto thread_registrar = mir::raii::paired_calls(
    [&running, thread_register]()
    {
        thread_register->register_thread(running);
    },
    [thread_register]()
    {
        thread_register->unregister_thread();
    });

    struct pollfd waiter;
    waiter.fd = dispatcher->watch_fd();
    waiter.events = POLL_IN;
    while (running)
    {
        if (poll(&waiter, 1, -1) < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failed to wait for event"}));
        }
        dispatcher->dispatch(md::FdEvent::readable);
    }
}
