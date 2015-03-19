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

#include "src/client/rpc/rpc_future_resolver.h"

#include <google/protobuf/service.h>

#include "mir_test/signal.h"

#include <gtest/gtest.h>


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

TEST(RpcFutureResolver, cancellation_after_immediate_trigger_has_no_effect)
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

    resolver.cancel();
}

TEST(RpcFutureResolver, cancellation_after_delayed_trigger_has_no_effect)
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

    resolver.cancel();
}
