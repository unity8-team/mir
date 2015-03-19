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
 * Authored by: Christohper James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <future>
#include <functional>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <boost/throw_exception.hpp>

#include <google/protobuf/service.h>

#include "mir_test/signal.h"

#include <gtest/gtest.h>

namespace google
{
namespace protobuf
{
class RpcChannel;
}
}

namespace mir
{
namespace client
{
namespace rpc
{
class RpcFutureResolver
{
public:
    RpcFutureResolver(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future_channel)
        : future_channel{std::move(future_channel)},
          continue_wait{true}
    {
    }

    ~RpcFutureResolver()
    {
        if (wait_thread.joinable())
            wait_thread.join();
    }

    static void call_completion_when_ready(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future,
                                           std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion,
                                           std::atomic<bool>& continue_wait)
    {
        using namespace std::literals::chrono_literals;
        while (continue_wait)
        {
            if (future.wait_for(10ms) == std::future_status::ready)
            {
                completion(std::move(future));
                return;
            }
        }
    }

    void set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion)
    {
        using namespace std::literals::chrono_literals;

        auto continuation_future = std::move(future_channel);
        if (!continuation_future.valid())
        {
            BOOST_THROW_EXCEPTION((std::logic_error{"Called set_completion more than once"}));
        }
        if (continuation_future.wait_for(0s) == std::future_status::ready)
        {
            completion(std::move(continuation_future));
        }
        else
        {
            wait_thread = std::thread{&call_completion_when_ready,
                                      std::forward<std::future<std::unique_ptr<google::protobuf::RpcChannel>>>(std::move(continuation_future)),
                                      completion,
                                      std::ref(continue_wait)};
        }
    }

    void cancel()
    {
        continue_wait = false;
        if (wait_thread.joinable())
            wait_thread.join();
    }

private:
    std::future<std::unique_ptr<google::protobuf::RpcChannel>> future_channel;
    std::thread wait_thread;
    std::atomic<bool> continue_wait;
};
}
}
}

namespace mclr = mir::client::rpc;
namespace mt = mir::test;

TEST(RpcFutureResolver, completion_is_called_immediately_if_set_on_ready_resolver)
{
    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;
    promised_rpc.set_value({});

    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    bool called{false};
    resolver.set_completion([&called](std::future<std::unique_ptr<google::protobuf::RpcChannel>>)
    {
        called = true;
    });
    EXPECT_TRUE(called);
}

TEST(RpcFutureResolver, completion_isnt_called_until_future_is_ready)
{
    using namespace std::literals::chrono_literals;

    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;

    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    auto called = std::make_shared<mt::Signal>();
    resolver.set_completion([called](std::future<std::unique_ptr<google::protobuf::RpcChannel>>)
    {
        called->raise();
    });
    EXPECT_FALSE(called->raised());

    promised_rpc.set_value({});

    EXPECT_TRUE(called->wait_for(60s));
}

TEST(RpcFutureResolver, calling_set_continuation_twice_is_an_error)
{
    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;
    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    resolver.set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {});
    EXPECT_THROW(resolver.set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {}),
            std::logic_error);

    promised_rpc.set_value({});
}

TEST(RpcFutureResolver, is_cancellable)
{
    using namespace std::literals::chrono_literals;

    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promised_rpc;
    mclr::RpcFutureResolver resolver{promised_rpc.get_future()};

    auto called = std::make_shared<mt::Signal>();
    resolver.set_completion([called](std::future<std::unique_ptr<google::protobuf::RpcChannel>>)
    {
        called->raise();
    });

    resolver.cancel();
    promised_rpc.set_value({});
    EXPECT_FALSE(called->wait_for(1s));
}
