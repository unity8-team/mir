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

#include "handshaking_connection_creator.h"
#include "rpc_channel_resolver.h"
#include "mir/frontend/handshake_protocol.h"
#include "protocol_interpreter.h"
#include "stream_socket_transport.h"
#include "mir/dispatch/simple_dispatch_thread.h"

#include <uuid/uuid.h>
#include <endian.h>
#include <atomic>
#include <mutex>
#include <boost/throw_exception.hpp>

namespace mcl = mir::client;
namespace mf = mir::frontend;
namespace mclr = mir::client::rpc;
namespace md = mir::dispatch;

namespace
{
template<class T>
class AtomicPtr
{
private:
    std::atomic<T*> val;
  
public:
    AtomicPtr()
        : val{nullptr}
    {
    }

    AtomicPtr(std::unique_ptr<T> move_from) noexcept
    {
        val = move_from.release();
    }

    AtomicPtr(AtomicPtr&& move_from) noexcept
    {
        val = move_from.val.exchange(nullptr);
    }
    AtomicPtr& operator=(AtomicPtr&& other) noexcept
    {
        val = other.val.exchange(nullptr);
        return *this;
    }

    AtomicPtr(AtomicPtr const&) = delete;

    void reset() noexcept(noexcept(val.load()->~T()))
    {
        auto old_value = val.exchange(nullptr);
        if (old_value != nullptr)
        {
            delete old_value;
        }
    }
    ~AtomicPtr() noexcept(noexcept(val.load()->~T()))
    {
        reset();
    }

    T* release() noexcept
    {
        return val.exchange(nullptr);
    }

    operator bool() noexcept
    {
        return val != nullptr;
    }
};

class HandshakeCompleter : public mclr::StreamTransport::Observer,
                           public mclr::RpcChannelResolver
{
public:
    HandshakeCompleter(std::vector<std::unique_ptr<mclr::ProtocolInterpreter>>&& protocols,
                       std::unique_ptr<mclr::StreamTransport> transport)
        : transport{std::move(transport)},
          protocols{std::move(protocols)},
          promise{std::make_unique<std::promise<std::unique_ptr<google::protobuf::RpcChannel>>>()}
    {
        this->transport->register_observer(std::shared_ptr<mclr::StreamTransport::Observer>{this, [](mclr::StreamTransport::Observer*){}});
        auto dispatchable_transport = std::shared_ptr<md::Dispatchable>{this->transport.get(), [](md::Dispatchable*){}};

        std::lock_guard<std::mutex> lock(mutex);
        eventloop = std::make_unique<md::SimpleDispatchThread>(dispatchable_transport);
    }

    virtual ~HandshakeCompleter()
    {
        auto loop_ptr = eventloop.release();
        if (loop_ptr != nullptr)
        {
            delete loop_ptr;
            // SimpleDispatchThread's destructor guarantees that we're no longer in any Observer
            // callbacks.
        }
        else
        {
            // The eventloop has already been destroyed from one of the callbacks. In this case,
            // it's possible that thread is still in a callback.
            //
            // Wait on the lock to ensure the destructor doesn't destroy anything still in use.
            std::lock_guard<std::mutex> lock(mutex);
        }

        if (completion)
        {
            auto future = promise->get_future();
            promise.reset();
            completion(std::move(future));
        }
    }

    void on_data_available() override
    {
        auto notify_at_end_of_scope = unregister_self();

        try
        {
            char uuid_str[37];
            transport->receive_data(uuid_str, sizeof(uuid_str) - 1);
            uuid_str[36] = '\0';

            uuid_t server_protocol;
            uuid_parse(uuid_str, server_protocol);

            for (auto& protocol : protocols)
            {
                uuid_t uuid;
                auto& handshake = protocol->connection_protocol();

                handshake.protocol_id(uuid);

                if (uuid_compare(uuid, server_protocol) == 0)
                {
                    handshake.receive_server_header(*transport);

                    promise->set_value(protocol->create_interpreter_for(std::move(transport)));
                    return;
                }
            }
            throw std::runtime_error{"Failed to complete protocol handshake with server"};
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        }
    }

    void on_disconnected() override
    {
        auto notify_at_end_of_scope = unregister_self();

        promise->set_exception(std::make_exception_ptr(std::runtime_error{"Socket disconnected before handshake complete"}));
    }

    void set_completion(std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion) override
    {
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            if (eventloop)
            {
                if (this->completion)
                {
                    BOOST_THROW_EXCEPTION((std::logic_error{"Called set_completion more than once"}));
                }
                this->completion = completion;
                return;
            }
        }
        try
        {
            completion(promise->get_future());
        }
        catch(std::future_error& err)
        {
            if (err.code() == std::future_errc::future_already_retrieved)
            {
                BOOST_THROW_EXCEPTION((std::logic_error{"Called set_completion more than once"}));
            }
            throw;
        }
    }

    std::unique_ptr<google::protobuf::RpcChannel> get() override
    {
        decltype(promise->get_future()) future;
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            future = promise->get_future();
        }
        return future.get();
    }
private:
    class NotifyOnDestroy
    {
    public:
        NotifyOnDestroy(std::unique_lock<std::mutex>&& lock,
                             std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion,
                             std::promise<std::unique_ptr<google::protobuf::RpcChannel>>& promise)
            : lock{std::move(lock)},
              completion{completion}
        {
            if (completion)
            {
                future = promise.get_future();
            }
        }

        ~NotifyOnDestroy()
        {
            lock.unlock();
            if (completion)
            {
                completion(std::move(future));
            }
        }

    private:
        std::unique_lock<std::mutex> lock;
        std::future<std::unique_ptr<google::protobuf::RpcChannel>> future;
        std::function<void(decltype(future))> completion;
    };

    std::unique_ptr<NotifyOnDestroy> unregister_self()
    {
        std::unique_lock<decltype(mutex)> lock(mutex);

        transport->unregister_observer(*this);
        eventloop.reset();
        
        decltype(completion) completion_copy;
        completion.swap(completion_copy);

        return std::make_unique<NotifyOnDestroy>(std::move(lock), completion_copy, *promise);
    }

    std::unique_ptr<mclr::StreamTransport> transport;
    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> const protocols;
    AtomicPtr<md::SimpleDispatchThread> eventloop;
    std::mutex mutex;
    std::unique_ptr<std::promise<std::unique_ptr<google::protobuf::RpcChannel>>> promise;
    std::function<void(std::future<std::unique_ptr<google::protobuf::RpcChannel>>)> completion;
};
}

mclr::HandshakingConnectionCreator::HandshakingConnectionCreator(
    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>>&& protocolings)
    : protocols(std::move(protocolings))
{
    static_assert(sizeof(uint8_t) == 1, "Unnecessary paranoia");

    total_header_size = sizeof(uint16_t);
    for (auto& protocol : protocols)
    {
        auto const& handshake = protocol->connection_protocol();
        total_header_size += sizeof(uint16_t);
        total_header_size += 36;
        total_header_size += handshake.header_size();
    }
    buffer.resize(total_header_size);

    uint8_t* buffer_pos = buffer.data();
    *reinterpret_cast<uint16_t*>(buffer_pos) = htole16(total_header_size - 2);
    buffer_pos += sizeof(uint16_t);
    for (auto& protocol : protocols)
    {
        char uuid_str[37];
        uuid_t uuid;
        auto const& handshake = protocol->connection_protocol();
        *reinterpret_cast<uint16_t*>(buffer_pos) = htole16(handshake.header_size() + 36);
        buffer_pos += sizeof(uint16_t);

        handshake.protocol_id(uuid);
        uuid_unparse(uuid, uuid_str);
        memcpy(buffer_pos, uuid_str, 36);
        buffer_pos += 36;

        handshake.write_client_header(buffer_pos);
        buffer_pos += handshake.header_size();
    }
}

std::unique_ptr<mclr::RpcChannelResolver> mclr::HandshakingConnectionCreator::connect_to(std::unique_ptr<mclr::StreamTransport> transport)
{
    transport->send_message(buffer, {});

    return std::make_unique<HandshakeCompleter>(std::move(protocols), std::move(transport));
}
