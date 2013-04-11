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

bool id_virtually_in_use(UniqueIdGenerator::Id x)
{
    // Pretend every 5th ID is in use.
    return (x % 5) == 0;
}

class TestGenerator : public UniqueIdGenerator
{
public:
    enum
    {
        ERR = 666,
        MIN = 10
    };

    TestGenerator() : UniqueIdGenerator(ERR, MIN) {}

    bool id_in_use(Id x) const
    {
        return id_virtually_in_use(x);
    }
};

}

TEST(UniqueIds, valid_and_unique)
{
    TestGenerator gen;

    ASSERT_EQ(TestGenerator::ERR, gen.invalid_id);
    ASSERT_EQ(TestGenerator::MIN, gen.min_id);

    for (int n = 0; n < 100; n++)
    {
        int i = gen.new_id();

        ASSERT_NE(gen.invalid_id, i);
        ASSERT_LE(gen.min_id, i);
        ASSERT_GE(gen.max_id, i);
        ASSERT_FALSE(id_virtually_in_use(i));
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
    {
        t->join();
        delete t;
        t = nullptr;
    }

    EXPECT_EQ(nthreads * nloops, (int)counts.size());

    for (auto const& c : counts)
    {
        ASSERT_EQ(1, c.second);
        ASSERT_NE(generator.invalid_id, c.first);
        ASSERT_LE(generator.min_id, c.first);
        ASSERT_GE(generator.max_id, c.first);
        ASSERT_FALSE(id_virtually_in_use(c.first));
    }
}

TEST(UniqueIds, exhaustion)
{
    class SmallGenerator : public UniqueIdGenerator
    {
    public:
        enum
        {
            ERR = 0,
            MIN = 1,
            MAX = 100
        };

        SmallGenerator() : UniqueIdGenerator(ERR, MIN, MAX), highest(0) {}
        bool id_in_use(Id x) const
        {
            return x <= highest;
        }

        void reserve(Id x)
        {
            if (x > highest)
                highest = x;
        }

    private:
        Id highest;
    };

    SmallGenerator gen;

    for (int n = 0; n < 2 * SmallGenerator::MAX; n++)
    {
        int i = gen.new_id();
        if (n < SmallGenerator::MAX)
        {
            ASSERT_LE(SmallGenerator::MIN, i);
            ASSERT_GE(SmallGenerator::MAX, i);
            ASSERT_EQ(n+1, i);
            gen.reserve(i);
        }
        else
            ASSERT_EQ(SmallGenerator::ERR, i);
    }
}
