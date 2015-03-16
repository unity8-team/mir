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

#include <future>
#include <thread>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace
{
template<typename WrappedFuture>
class Continuation
{
public:
    Continuation(WrappedFuture&& future)
        : future{std::move(future)}
    {
    }

    void then(std::function<void(WrappedFuture&&)> continuation)
    {
        continuation(std::move(future));
    }

private:
    WrappedFuture future;
};
}

TEST(Continuation, calls_continuation_immediately_on_ready_future)
{
    using namespace testing;

    std::promise<int> test;
    test.set_value(5);

    bool continuation_called{false};
    Continuation<std::future<int>>{test.get_future()}.then([&continuation_called](std::future<int>&& value)
    {
        EXPECT_THAT(value.get(), Eq(5));
        continuation_called = true;
    });

    EXPECT_TRUE(continuation_called);
}
