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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include <thread>
#include <map>
#include <mutex>
#include <gtest/gtest.h>
#include "src/client/unique_id_generator.h"

using namespace mir::client;

namespace
{

class TestGenerator : public UniqueIdGenerator
{
public:
    TestGenerator() : UniqueIdGenerator(666, 10) {}

    bool id_in_use(id_t x)
    {
        return (x % 5) == 0;
    }
};

}

TEST(UniqueIds, valid_and_unique)
{
    TestGenerator gen;

    EXPECT_EQ(666, gen.invalid_id);
    EXPECT_EQ(10, gen.min_id);

    for (int n = 0; n < 100; n++)
    {
        int i = gen.new_id();

        ASSERT_NE(gen.invalid_id, i);
        ASSERT_LE(gen.min_id, i);
        ASSERT_GE(gen.max_id, i);
        ASSERT_TRUE(i % 5);
    }
}

namespace
{
std::map<int, int> counts;
std::mutex counts_lock;
TestGenerator generator;

static void busy_thread(int loops)
{
    for (int n = 0; n < loops; n++)
    {
        counts_lock.lock();
        counts[generator.new_id()]++;
        counts_lock.unlock();
    }
}

} // namespace

TEST(UniqueIds, valid_and_unique_across_threads)
{
    const int nloops = 100;
    const int nthreads = 10;

    std::thread *thread[nthreads];

    for (std::thread *& t : thread)
        t = new std::thread(busy_thread, nloops);

    for (std::thread *& t : thread)
        t->join();
    
    EXPECT_EQ(nthreads * nloops, (int)counts.size());

    for (auto const& c : counts)
    {
        ASSERT_EQ(1, c.second);
        ASSERT_NE(generator.invalid_id, c.first);
        ASSERT_LE(generator.min_id, c.first);
        ASSERT_GE(generator.max_id, c.first);
        ASSERT_TRUE(c.first % 5);
    }
}

namespace
{

}

TEST(UniqueIds, exhaustion)
{
    class SmallGenerator : public UniqueIdGenerator
    {
    public:
        SmallGenerator() : UniqueIdGenerator(0, 1, 100), highest(0) {}
        bool id_in_use(id_t x)
        {
            return x <= highest;
        }

        void reserve(id_t x)
        {
            if (x > highest)
                highest = x;
        }

    private:
        id_t highest;
    };

    SmallGenerator gen;

    for (int n = 0; n < 200; n++)
    {
        int i = gen.new_id();
        ASSERT_LE(0, i);
        ASSERT_GE(100, i);
        if (n < 100)
        {
            ASSERT_LE(1, i);
            gen.reserve(i);
        }
        else
            ASSERT_EQ(0, i);
    }
}
