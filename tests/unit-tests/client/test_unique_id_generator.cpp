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
    TestGenerator() : UniqueIdGenerator(4) {}

    bool id_in_use(id_t x)
    {
        return (x % 5) == 0;
    }
};

}

TEST(UniqueIds, valid_and_unique)
{
    TestGenerator gen;

    EXPECT_EQ(4, gen.invalid_id);

    for (int n = 0; n < 100; n++)
    {
        int i = gen.new_id();

        ASSERT_NE(gen.invalid_id, i);
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
        ASSERT_TRUE(c.first % 5);
    }
}
