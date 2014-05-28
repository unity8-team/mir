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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/server/sentinel_action.h"
#include "mir_test_doubles/mock_server_action_queue.h"

#include <thread>

namespace mtd = mir::test::doubles;

TEST(SentinelActionTest, just_executes_action_if_queue_is_not_running)
{
    using namespace ::testing;

    NiceMock<mtd::MockServerActionQueue> mock_queue;
    EXPECT_CALL(mock_queue, enqueue(_,_)).Times(0);
    int val = 0;
    boost::optional<std::thread::id> empty_queue_thread_id;
    {
        mir::SentinelAction action(mock_queue, empty_queue_thread_id, [&val]{val=1;});
    }
    EXPECT_EQ(val, 1);
}

TEST(SentinelActionTest, just_executes_action_if_thread_identical)
{
    using namespace ::testing;

    NiceMock<mtd::MockServerActionQueue> mock_queue;
    EXPECT_CALL(mock_queue, enqueue(_,_)).Times(0);
    int val = 0;
    boost::optional<std::thread::id> this_one{std::this_thread::get_id()};
    {
        mir::SentinelAction action(mock_queue, this_one, [&val]{val=1;});
    }
    EXPECT_EQ(val, 1);
}

TEST(SentinelActionTest, enqueues_action_if_thread_differs)
{
    using namespace ::testing;

    class DirectlyExecutingServerActionQueue : public StrictMock<mtd::MockServerActionQueue>
    {
        void enqueue(void const*, mir::ServerAction const& action) override
        {
            action();
        }
    };

    DirectlyExecutingServerActionQueue execute_it;

    int val = 0;
    std::thread::id arbitrary_thread;
    boost::optional<std::thread::id> not_this_one{arbitrary_thread};

    {
        mir::SentinelAction action(execute_it, not_this_one, [&val]{val=1;});
    }
    EXPECT_EQ(1,val);
}
