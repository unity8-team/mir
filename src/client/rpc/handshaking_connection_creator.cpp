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
#include "mir/frontend/handshake_protocol.h"
#include "protocol_interpreter.h"
#include "stream_socket_transport.h"
#include "mir/dispatch/simple_dispatch_thread.h"

#include <uuid/uuid.h>
#include <endian.h>
#include <boost/thread/future.hpp>
#include <boost/exception_ptr.hpp>

namespace mcl = mir::client;
namespace mf = mir::frontend;
namespace mclr = mir::client::rpc;
namespace md = mir::dispatch;

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

std::future<std::unique_ptr<google::protobuf::RpcChannel>> mclr::HandshakingConnectionCreator::connect_to(std::unique_ptr<mclr::StreamTransport> transport)
{
    std::promise<std::unique_ptr<google::protobuf::RpcChannel>> protocol_interpreter;
    auto protocol_future = protocol_interpreter.get_future();

    class HandshakeCompleter : public mclr::StreamTransport::Observer,
                               public std::enable_shared_from_this<HandshakeCompleter>
    {
    public:
        HandshakeCompleter(std::promise<std::unique_ptr<google::protobuf::RpcChannel>>&& promise,
                           std::vector<std::unique_ptr<mclr::ProtocolInterpreter>>&& protocolings)
            : promise{std::move(promise)},
              protocols{std::move(protocolings)}
        {
        }

        void on_data_available() override
        {
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

                        // We need to unregister ourselves as a transport observer, but the transport
                        // might be holding our only reference.
                        //
                        // Add a new shared_ptr to ensure we're not destroyed until we've finished
                        auto enlivener = shared_from_this();
                        transport->unregister_observer(*this);
                        eventloop.reset();

                        promise.set_value(protocol->create_interpreter_for(std::move(transport)));
                        return;
                    }
                }
                throw std::runtime_error{"Failed to complete protocol handshake with server"};
            }
            catch (...)
            {
                auto enlivener = shared_from_this();
                transport->unregister_observer(*this);
                eventloop.reset();

                promise.set_exception(std::current_exception());
            }
        }

        void on_disconnected() override
        {
            promise.set_exception(std::make_exception_ptr(std::runtime_error{"Socket disconnected before handshake complete"}));
        }

        void complete_handshake(std::unique_ptr<mclr::StreamTransport> transport)
        {
            this->transport = std::move(transport);

            auto dispatchable_transport = std::shared_ptr<md::Dispatchable>{this->transport.get(), [](md::Dispatchable*){}};
            eventloop = std::make_unique<md::SimpleDispatchThread>(dispatchable_transport);
        }

    private:
        std::unique_ptr<mclr::StreamTransport> transport;
        std::unique_ptr<md::SimpleDispatchThread> eventloop;
        std::promise<std::unique_ptr<google::protobuf::RpcChannel>> promise;
        std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
    };

    auto observer = std::make_shared<HandshakeCompleter>(std::move(protocol_interpreter), std::move(protocols));

    transport->register_observer(observer);
    transport->send_message(buffer, {});

    observer->complete_handshake(std::move(transport));

    return protocol_future;
}
