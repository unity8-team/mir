/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "rpc_future_resolver.h"

#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <boost/throw_exception.hpp>

namespace mclr = mir::client::rpc;

namespace
{
void call_completion_when_ready(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future,
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

}

mclr::RpcFutureResolver::RpcFutureResolver(std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future_channel)
    : future_channel{std::move(future_channel)},
      continue_wait{true}
{
}

mclr::RpcFutureResolver::~RpcFutureResolver()
{
    if (wait_thread.joinable())
        wait_thread.join();
}


void mclr::RpcFutureResolver::set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion)
{
    using namespace std::literals::chrono_literals;

    if (!future_channel.valid())
    {
        BOOST_THROW_EXCEPTION((std::logic_error{"Called set_completion more than once"}));
    }
    if (future_channel.wait_for(0s) == std::future_status::ready)
    {
        completion(std::move(future_channel));
    }
    else
    {
        wait_thread = std::thread{&call_completion_when_ready,
                std::forward<std::future<std::unique_ptr<google::protobuf::RpcChannel>>>(std::move(future_channel)),
                completion,
                std::ref(continue_wait)};
    }
}

void mclr::RpcFutureResolver::cancel()
{
    continue_wait = false;
    if (wait_thread.joinable())
        wait_thread.join();
}
