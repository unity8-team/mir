/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/asio_server_action_queue.h"

#include "mir_test/pipe.h"
#include "mir_test/auto_unblock_thread.h"
#include "mir_test/wait_object.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <array>
#include <boost/throw_exception.hpp>

#include <sys/types.h>
#include <unistd.h>

namespace mt = mir::test;

namespace
{

class AsioServerActionQueueTest : public ::testing::Test
{
public:
    boost::asio::io_service service;
    mir::AsioServerActionQueue queue{service};
};

}

TEST_F(AsioServerActionQueueTest, dispatches_action)
{
    using namespace testing;

    int num_actions{0};
    int const owner{0};

    queue.enqueue(
        &owner,
        [&]
        {
            ++num_actions;
            service.stop();
        });

    service.run();

    EXPECT_THAT(num_actions, Eq(1));
}

TEST_F(AsioServerActionQueueTest, dispatches_multiple_actions_in_order)
{
    using namespace testing;

    int const num_actions{5};
    std::vector<int> actions;
    int const owner{0};

    for (int i = 0; i < num_actions; ++i)
    {
        queue.enqueue(
            &owner,
            [&,i]
            {
                actions.push_back(i);
                if (i == num_actions - 1)
                    service.stop();
            });
    }

    service.run();

    ASSERT_THAT(actions.size(), Eq(num_actions));
    for (int i = 0; i < num_actions; ++i)
        EXPECT_THAT(actions[i], Eq(i)) << "i = " << i;
}

TEST_F(AsioServerActionQueueTest, does_not_dispatch_paused_actions)
{
    using namespace testing;

    std::vector<int> actions;
    int const owner1{0};
    int const owner2{0};

    queue.enqueue(
        &owner1,

        [&]
        {
            int const id = 0;
            actions.push_back(id);
        });

    queue.enqueue(
        &owner2,
        [&]
        {
            int const id = 1;
            actions.push_back(id);
        });

    queue.enqueue(
        &owner1,
        [&]
        {
            int const id = 2;
            actions.push_back(id);
        });

    queue.enqueue(
        &owner2,
        [&]
        {
            int const id = 3;
            actions.push_back(id);
            service.stop();
        });

    queue.pause_processing_for(&owner1);

    service.run();

    ASSERT_THAT(actions.size(), Eq(2));
    EXPECT_THAT(actions[0], Eq(1));
    EXPECT_THAT(actions[1], Eq(3));
}

TEST_F(AsioServerActionQueueTest, dispatches_resumed_actions)
{
    using namespace testing;

    std::vector<int> actions;
    void const* const owner1_ptr{&actions};
    int const owner2{0};

    queue.enqueue(
        owner1_ptr,
        [&]
        {
            int const id = 0;
            actions.push_back(id);
            service.stop();
        });

    queue.enqueue(
        &owner2,
        [&]
        {
            int const id = 1;
            actions.push_back(id);
            queue.resume_processing_for(owner1_ptr);
        });

    queue.pause_processing_for(owner1_ptr);

    service.run();

    ASSERT_THAT(actions.size(), Eq(2));
    EXPECT_THAT(actions[0], Eq(1));
    EXPECT_THAT(actions[1], Eq(0));
}

TEST_F(AsioServerActionQueueTest, handles_enqueue_from_within_action)
{
    using namespace testing;

    std::vector<int> actions;
    int const num_actions{10};
    void const* const owner{&num_actions};

    queue.enqueue(
        owner,
        [&]
        {
            int const id = 0;
            actions.push_back(id);

            for (int i = 1; i < num_actions; ++i)
            {
                queue.enqueue(
                    owner,
                    [&,i]
                    {
                        actions.push_back(i);
                        if (i == num_actions - 1)
                            service.stop();
                    });
            }
        });

    service.run();

    ASSERT_THAT(actions.size(), Eq(num_actions));
    for (int i = 0; i < num_actions; ++i)
        EXPECT_THAT(actions[i], Eq(i)) << "i = " << i;
}
