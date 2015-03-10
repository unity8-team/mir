/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/dispatch/threaded_dispatcher.h"
#include "mir/dispatch/dispatchable.h"
#include "mir/fd.h"
#include "mir_test/pipe.h"
#include "mir_test/signal.h"
#include "mir_test/test_dispatchable.h"

#include <fcntl.h>

#include <atomic>
#include <valgrind/valgrind.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace md = mir::dispatch;
namespace mt = mir::test;

namespace
{
class ThreadedDispatcherTest : public ::testing::Test
{
public:
    ThreadedDispatcherTest()
    {
        mt::Pipe pipe{O_NONBLOCK};
        watch_fd = pipe.read_fd();
        test_fd = pipe.write_fd();
    }

    mir::Fd watch_fd;
    mir::Fd test_fd;
};

class MockDispatchable : public md::Dispatchable
{
public:
    MOCK_CONST_METHOD0(watch_fd, mir::Fd());
    MOCK_METHOD1(dispatch, bool(md::FdEvents));
    MOCK_CONST_METHOD0(relevant_events, md::FdEvents());
};

}

TEST_F(ThreadedDispatcherTest, calls_dispatch_when_fd_is_readable)
{
    using namespace testing;

    auto dispatched = std::make_shared<mt::Signal>();
    auto dispatchable = std::make_shared<mt::TestDispatchable>([dispatched]() { dispatched->raise(); });

    md::ThreadedDispatcher dispatcher{"Ducks", dispatchable};

    dispatchable->trigger();

    EXPECT_TRUE(dispatched->wait_for(std::chrono::seconds{10}));
}

TEST_F(ThreadedDispatcherTest, stops_calling_dispatch_once_fd_is_not_readable)
{
    using namespace testing;

    std::atomic<int> dispatch_count{0};
    auto dispatchable = std::make_shared<mt::TestDispatchable>([&dispatch_count]() { ++dispatch_count; });

    md::ThreadedDispatcher dispatcher{"Dispatcher loop", dispatchable};

    dispatchable->trigger();

    // 1s is fine; if things are too slow we might get a false-pass, but that's OK.
    std::this_thread::sleep_for(std::chrono::seconds{1});

    EXPECT_THAT(dispatch_count, Eq(1));
}

TEST_F(ThreadedDispatcherTest, passes_dispatch_events_through)
{
    using namespace testing;

    auto dispatched_with_only_readable = std::make_shared<mt::Signal>();
    auto dispatched_with_hangup = std::make_shared<mt::Signal>();
    auto delegate = [dispatched_with_only_readable, dispatched_with_hangup](md::FdEvents events)
    {
        if (events == md::FdEvent::readable)
        {
            dispatched_with_only_readable->raise();
        }
        if (events & md::FdEvent::remote_closed)
        {
            dispatched_with_hangup->raise();
            return false;
        }
        return true;
    };
    auto dispatchable = std::make_shared<mt::TestDispatchable>(delegate, md::FdEvent::readable | md::FdEvent::remote_closed);

    md::ThreadedDispatcher dispatcher{"Avocado", dispatchable};

    dispatchable->trigger();
    EXPECT_TRUE(dispatched_with_only_readable->wait_for(std::chrono::seconds{10}));

    dispatchable->hangup();
    EXPECT_TRUE(dispatched_with_hangup->wait_for(std::chrono::seconds{10}));
}

TEST_F(ThreadedDispatcherTest, doesnt_call_dispatch_after_first_false_return)
{
    using namespace testing;

    int constexpr expected_count{10};
    auto dispatched_more_than_enough = std::make_shared<mt::Signal>();

    auto delegate = [dispatched_more_than_enough](md::FdEvents)
    {
        static std::atomic<int> dispatch_count{0};

        if (++dispatch_count == expected_count)
        {
            return false;
        }
        if (dispatch_count > expected_count)
        {
            dispatched_more_than_enough->raise();
        }
        return true;
    };
    auto dispatchable = std::make_shared<mt::TestDispatchable>(delegate);

    md::ThreadedDispatcher dispatcher{"Unexpected walrus", dispatchable};

    for (int i = 0; i < expected_count + 1; ++i)
    {
        dispatchable->trigger();
    }

    EXPECT_FALSE(dispatched_more_than_enough->wait_for(std::chrono::seconds{1}));
}

TEST_F(ThreadedDispatcherTest, only_calls_dispatch_with_remote_closed_when_relevant)
{
    using namespace testing;

    auto dispatchable = std::make_shared<NiceMock<MockDispatchable>>();
    ON_CALL(*dispatchable, watch_fd()).WillByDefault(Return(test_fd));
    ON_CALL(*dispatchable, relevant_events()).WillByDefault(Return(md::FdEvent::writable));
    auto dispatched_writable = std::make_shared<mt::Signal>();
    auto dispatched_closed = std::make_shared<mt::Signal>();

    ON_CALL(*dispatchable, dispatch(_)).WillByDefault(Invoke([=](md::FdEvents events)
    {
        if (events & md::FdEvent::writable)
        {
            dispatched_writable->raise();
        }
        if (events & md::FdEvent::remote_closed)
        {
            dispatched_closed->raise();
        }
        return true;
    }));

    md::ThreadedDispatcher dispatcher{"Implacable iguana", dispatchable};

    EXPECT_TRUE(dispatched_writable->wait_for(std::chrono::seconds{10}));

    // Make the fd remote-closed...
    watch_fd = mir::Fd{};
    EXPECT_FALSE(dispatched_closed->wait_for(std::chrono::seconds{1}));
}

TEST_F(ThreadedDispatcherTest, dispatches_multiple_dispatchees_simultaneously)
{
    using namespace testing;

    if (RUNNING_ON_VALGRIND)
    {
        // Sadly we can't mark this as inconclusive under valgrind.
        return;
    }

    auto first_dispatched = std::make_shared<mt::Signal>();
    auto second_dispatched = std::make_shared<mt::Signal>();

    // Set up two dispatchables that can run given two threads of execution,
    // but will deadlock if run sequentially.
    auto first_dispatchable = std::make_shared<mt::TestDispatchable>([first_dispatched, second_dispatched]()
    {
        first_dispatched->raise();
        EXPECT_TRUE(second_dispatched->wait_for(std::chrono::seconds{10}));
    });
    auto second_dispatchable = std::make_shared<mt::TestDispatchable>([first_dispatched, second_dispatched]()
    {
        second_dispatched->raise();
        EXPECT_TRUE(first_dispatched->wait_for(std::chrono::seconds{10}));
    });

    auto combined_dispatchable = std::shared_ptr<md::MultiplexingDispatchable>(new md::MultiplexingDispatchable{first_dispatchable, second_dispatchable});
    md::ThreadedDispatcher dispatcher{"Barry", combined_dispatchable};

    dispatcher.add_thread();

    first_dispatchable->trigger();
    second_dispatchable->trigger();
}

TEST_F(ThreadedDispatcherTest, remove_thread_decreases_concurrency)
{
    using namespace testing;

    // Set up two dispatchables that will fail if run simultaneously
    auto second_dispatched = std::make_shared<mt::Signal>();

    auto first_dispatchable = std::make_shared<mt::TestDispatchable>([second_dispatched]()
    {
        //1s is OK here; if things are slow, we might false-pass.
        EXPECT_FALSE(second_dispatched->wait_for(std::chrono::seconds{1}));
    });
    auto second_dispatchable = std::make_shared<mt::TestDispatchable>([second_dispatched]()
    {
        second_dispatched->raise();
    });

    auto combined_dispatchable = std::make_shared<md::MultiplexingDispatchable>();
    combined_dispatchable->add_watch(first_dispatchable);
    combined_dispatchable->add_watch(second_dispatchable);
    md::ThreadedDispatcher dispatcher{"Questionable decision", combined_dispatchable};

    dispatcher.add_thread();

    first_dispatchable->trigger();
    dispatcher.remove_thread();
    second_dispatchable->trigger();

    EXPECT_TRUE(second_dispatched->wait_for(std::chrono::seconds{10}));
}

TEST_F(ThreadedDispatcherTest, handles_destruction_from_dispatch_callback)
{
    using namespace testing;
    using namespace std::chrono_literals;

    auto dispatched = std::make_shared<mt::Signal>();
    md::ThreadedDispatcher* dispatcher{nullptr};

    auto dispatchable = std::make_shared<mt::TestDispatchable>([dispatched, &dispatcher]()
                                                               {
                                                                   delete dispatcher;
                                                                   dispatched->raise();
                                                               });

    dispatchable->trigger();
    dispatchable->trigger();

    dispatcher = new md::ThreadedDispatcher{"Mournful", dispatchable};

    EXPECT_TRUE(dispatched->wait_for(10s));
}

TEST_F(ThreadedDispatcherTest, exceptions_in_threadpool_trigger_termination)
{
    using namespace testing;
    using namespace std::chrono_literals;

    FLAGS_gtest_death_test_style = "threadsafe";
    constexpr char const* exception_msg = "Ducks! Ducks attack!";

    auto dispatchable = std::make_shared<mt::TestDispatchable>([]()
                                                               {
                                                                   throw std::runtime_error{exception_msg};
                                                               });

    dispatchable->trigger();

    // Ideally we'd use terminate_with_current_exception, but that's in server
    // and we're going to be called from a C context anyway, so just using the default
    // std::terminate behaviour should be acceptable.
    EXPECT_EXIT({ md::ThreadedDispatcher dispatcher("Die!", dispatchable); std::this_thread::sleep_for(10s); },
                KilledBySignal(SIGABRT), (std::string{".*"} + exception_msg + ".*").c_str());
}

TEST_F(ThreadedDispatcherTest, sets_thread_names_appropriately)
{
    using namespace testing;
    using namespace std::chrono_literals;

    auto dispatched = std::make_shared<mt::Signal>();
    constexpr int const threadcount{3};
    constexpr char const* threadname_base{"Madness Thread"};

    auto dispatchable = std::make_shared<mt::TestDispatchable>([dispatched]()
                                                               {
                                                                   static std::atomic<int> dispatch_count{0};
                                                                   char buffer[80] = {0};
                                                                   pthread_getname_np(pthread_self(), buffer, sizeof(buffer));
                                                                   EXPECT_THAT(buffer, StartsWith(threadname_base));

                                                                   if (++dispatch_count == threadcount)
                                                                   {
                                                                       dispatched->raise();
                                                                   }
                                                                   else
                                                                   {
                                                                       dispatched->wait_for(10s);
                                                                   }
                                                               });

    md::ThreadedDispatcher dispatcher{threadname_base, dispatchable};

    for (int i = 0; i < threadcount; ++i)
    {
        dispatcher.add_thread();
        dispatchable->trigger();
    }

    EXPECT_TRUE(dispatched->wait_for(10s));
}
