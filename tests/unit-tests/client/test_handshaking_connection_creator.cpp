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

#include "src/client/rpc/handshaking_connection_creator.h"
#include "src/client/rpc/protocol_interpreter.h"
#include "mir/frontend/handshake_protocol.h"
#include "src/client/rpc/stream_transport.h"

#include <sys/eventfd.h>
#include <uuid/uuid.h>
#include <algorithm>
#include <system_error>
#include <cstring>
#include <endian.h>


#include <boost/exception/all.hpp>

#include "mir_test/gmock_fixes.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mclr = mir::client::rpc;

namespace
{
class MockClientHandshakeProtocol : public mir::frontend::HandshakeProtocol
{
public:
    MockClientHandshakeProtocol(std::string const& uuid, std::vector<uint8_t> client_header)
        : uuid{uuid},
          client_header{client_header}
    {
    }

    void protocol_id(uuid_t id) const
    {
        uuid_parse(uuid.c_str(), id);
    }

    size_t header_size() const override
    {
        return client_header.size();
    }

    void write_client_header(uint8_t* buffer) const override
    {
        std::copy(client_header.begin(), client_header.end(), buffer);
    }

    void send_server_header() override
    {
    }

    void receive_server_header(mir::client::rpc::StreamTransport&) override
    {
    }

    std::string const& uuid;
    std::vector<uint8_t> const client_header;
};

class MockProtocolInterpreter : public mclr::ProtocolInterpreter
{
public:
    MockProtocolInterpreter(std::unique_ptr<MockClientHandshakeProtocol> protocol)
        : protocol{std::move(protocol)}
    {
    }

    std::unique_ptr<google::protobuf::RpcChannel> create_interpreter_for(std::unique_ptr<mclr::StreamTransport>)
    {
        return std::unique_ptr<google::protobuf::RpcChannel>{ reinterpret_cast<google::protobuf::RpcChannel*>(this) };
    }

    mir::frontend::HandshakeProtocol &connection_protocol() override
    {
        return *protocol;
    }

private:
    std::unique_ptr<MockClientHandshakeProtocol> protocol;
};

class RecordingStreamTransport : public mclr::StreamTransport
{
public:
    RecordingStreamTransport()
        : event_fd{eventfd(0, EFD_CLOEXEC)}
    {
        if (event_fd == mir::Fd::invalid)
        {
            throw std::system_error{errno, std::system_category(), "Failed to create eventfd"};
        }
    }

    mir::Fd watch_fd() const override
    {
        return event_fd;
    }

    bool dispatch(mir::dispatch::FdEvents events) override
    {
        if (events & mir::dispatch::FdEvent::readable)
        {
            for (auto& observer : observers)
            {
                observer->on_data_available();
            }
        }
        return true;
    }

    mir::dispatch::FdEvents relevant_events() const override
    {
        return mir::dispatch::FdEvent::readable;
    }

    void register_observer(std::shared_ptr<Observer> const& observer) override
    {
        observers.push_back(observer);
    }
    void unregister_observer(Observer const& observer) override
    {
        auto victim = std::find_if (observers.begin(), observers.end(),
                                    [&observer] (std::shared_ptr<Observer> candidate)
        {
            return candidate.get() == &observer;
        });
        observers.erase(victim);
    }

    void receive_data(void* buffer, size_t size) override
    {
        memcpy(buffer, receive_buffer.data(), size);
        receive_buffer.erase(receive_buffer.begin(), receive_buffer.begin() + size);
        eventfd_t remaining_bytes;
        eventfd_read(event_fd, &remaining_bytes);
        eventfd_write(event_fd, remaining_bytes - size);
    }

    void receive_data(void*, size_t, std::vector<mir::Fd>&) override
    {
    }

    void send_message(std::vector<uint8_t> const& buffer, std::vector<mir::Fd> const&) override
    {
        send_buffer.insert(send_buffer.end(), buffer.begin(), buffer.end());
    }

    void add_received_message(std::vector<uint8_t> const& buffer)
    {
        receive_buffer.insert(receive_buffer.end(), buffer.begin(), buffer.end());
        eventfd_write(event_fd, buffer.size());
    }

    std::vector<uint8_t> send_buffer;
    std::vector<uint8_t> receive_buffer;
    std::vector<std::shared_ptr<Observer>> observers;
private:
    mir::Fd event_fd;
};
}


TEST(ClientHandshakingConnectionCreator, writes_handshake_header_for_single_protocol)
{
    using namespace testing;

    auto transport = std::make_unique<RecordingStreamTransport>();
    std::string const uuid_str{"be094b17-4ca0-40fd-9394-913a4aab05f0"};
    std::vector<char> uuid{uuid_str.begin(), uuid_str.end()};
    std::vector<uint8_t> client_header{ 0xde, 0xad, 0xbe, 0xef, 0x00, 0x01, 0x02 };

    auto protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(uuid_str, client_header));
    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
    protocols.emplace_back(std::move(protocol));
    mclr::HandshakingConnectionCreator handshake{std::move(protocols)};

    auto* transport_observer = transport.get();

    auto future = handshake.connect_to(std::move(transport));

    std::vector<uint8_t> expected_header;
    expected_header.push_back(0x2D);
    expected_header.push_back(0x00);    // Header size (little endian) = 002D₁₆ = 45₁₀
    expected_header.push_back(0x2B);
    expected_header.push_back(0x00);   // Proto header size (little endian) = 002B₁₆ = 36₁₀ + 7₁₀
    expected_header.insert(expected_header.end(), uuid.begin(), uuid.end());     // UUID, no trailing null
    expected_header.insert(expected_header.end(), client_header.begin(), client_header.end());

    uint16_t total_header_size = le16toh(*reinterpret_cast<uint16_t*>(transport_observer->send_buffer.data()));
    uint16_t client_header_size = le16toh(*reinterpret_cast<uint16_t*>(transport_observer->send_buffer.data() + 2));
    EXPECT_THAT(transport_observer->send_buffer.size(), Eq(total_header_size + 2));
    EXPECT_THAT(client_header_size, Eq(client_header.size() + 36));
    EXPECT_THAT(transport_observer->send_buffer, ContainerEq(expected_header));
}

TEST(ClientHandshakingConnectionCreator, dispatches_to_correct_protocol_based_on_server_reply)
{
    using namespace testing;

    auto transport = std::make_unique<RecordingStreamTransport>();
    std::string const first_uuid{"be094b17-4ca0-40fd-9394-913a4aab05f0"};
    std::string const second_uuid{"ec8480b6-0246-4399-8dc1-c54adf7a8985"};

    auto first_protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(first_uuid, std::vector<uint8_t>{}));
    auto second_protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(second_uuid, std::vector<uint8_t>{}));
    auto second_protocol_addr = reinterpret_cast<void*>(second_protocol.get());

    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
    protocols.emplace_back(std::move(first_protocol));
    protocols.emplace_back(std::move(second_protocol));
    mclr::HandshakingConnectionCreator handshake{std::move(protocols)};

    auto* transport_observer = transport.get();
    auto future = handshake.connect_to(std::move(transport));

    std::vector<uint8_t> buffer(second_uuid.begin(), second_uuid.end());
    transport_observer->add_received_message(buffer);

    auto proto = reinterpret_cast<void*>(future.get().release());
    EXPECT_THAT(proto, Eq(second_protocol_addr));
}

TEST(ClientHandshakingConnectionCreator, throws_exception_on_server_protocol_mismatch)
{
    using namespace testing;

    auto transport = std::make_unique<RecordingStreamTransport>();
    std::string const client_uuid{"be094b17-4ca0-40fd-9394-913a4aab05f0"};
    std::string const mismatching_uuid{"ec8480b6-0246-4399-8dc1-c54adf7a8985"};

    auto protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(client_uuid, std::vector<uint8_t>{}));

    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
    protocols.emplace_back(std::move(protocol));
    mclr::HandshakingConnectionCreator handshake{std::move(protocols)};

    auto* transport_observer = transport.get();
    auto future = handshake.connect_to(std::move(transport));

    std::vector<uint8_t> buffer(mismatching_uuid.begin(), mismatching_uuid.end());
    transport_observer->add_received_message(buffer);

    EXPECT_THROW(future.get().release(), std::runtime_error);
}
