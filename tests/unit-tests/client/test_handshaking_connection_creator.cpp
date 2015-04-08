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
#include "src/client/rpc/rpc_channel_resolver.h"

#include <sys/eventfd.h>
#include <uuid/uuid.h>
#include <algorithm>
#include <system_error>
#include <cstring>
#include <endian.h>
#include <chrono>
#include <thread>
#include <future>
#include <experimental/optional>

#include <boost/exception/all.hpp>

#include "mir_test_framework/process.h"

#include "mir_test/gmock_fixes.h"
#include "mir_test/fd_utils.h"
#include "mir_test/signal.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mclr = mir::client::rpc;
namespace mt = mir::test;
namespace mtf = mir_test_framework;

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

    std::unique_ptr<google::protobuf::RpcChannel> create_interpreter_for(std::unique_ptr<mclr::StreamTransport> transport)
    {
        transport_holder = std::move(transport);
        return std::unique_ptr<google::protobuf::RpcChannel>{ reinterpret_cast<google::protobuf::RpcChannel*>(this) };
    }

    mir::frontend::HandshakeProtocol &connection_protocol() override
    {
        return *protocol;
    }

private:
    std::unique_ptr<mclr::StreamTransport> transport_holder;
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
            decltype(observers) observers_copy;
            {
                std::lock_guard<decltype(observer_mutex)> lock(observer_mutex);
                observers_copy = observers;
            }
            for (auto& observer : observers_copy)
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
        std::lock_guard<decltype(observer_mutex)> lock(observer_mutex);
        observers.push_back(observer);
    }
    void unregister_observer(Observer const& observer) override
    {
        std::lock_guard<decltype(observer_mutex)> lock(observer_mutex);
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
    std::mutex observer_mutex;
};

bool has_broken_stdcpp()
{
    static std::experimental::optional<bool> broken;
    if (!broken)
    {
        using namespace std::literals::chrono_literals;
        auto child = mtf::fork_and_run_in_a_different_process([]()
        {
            auto except = std::make_exception_ptr(0);
            try
            {
                std::rethrow_exception(except);
            }
            catch(...)
            {
            }
            if (std::uncaught_exception())
            {
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        },[](){ return 0; });

        broken = child->wait_for_termination(10s).exit_code != EXIT_SUCCESS;

        if (broken.value())
        {
            std::cerr << "*** Working around broken std::rethrow_exception behaviour ***" << std::endl;
        }
    }
    return broken.value();
}

class ForkGuard
{
public:
    ForkGuard()
        : test{nullptr}
    {
    }

    ForkGuard(pid_t pid, testing::Test* test)
        : test{test}
    {
        if (pid != 0)
        {
            process = std::make_unique<mtf::Process>(pid);
        }
    }

    ForkGuard(ForkGuard&&) = default;

    bool run_tests_in_this_process()
    {
        return !process;
    }

    ~ForkGuard()
    {
        using namespace std::literals::chrono_literals;
        if (process)
        {
            // In parent...
            EXPECT_TRUE(process->wait_for_termination(10s).succeeded());
        }
        else if (test)
        {
            // In child...
            exit(test->HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        }
    }

private:
    testing::Test* const test;
    std::unique_ptr<mtf::Process> process;
};

ForkGuard maybe_fork_to_run_test(testing::Test* test)
{
    if(has_broken_stdcpp())
    {
        pid_t pid = fork();

        if (pid < 0)
        {
            BOOST_THROW_EXCEPTION((std::system_error{errno,
                                                     std::system_category(),
                                                     "Failure to fork"}));
        }

        return ForkGuard{pid, test};
    }
    return ForkGuard{};
}

class ClientHandshakingConnectionCreator : public testing::Test
{
};
}


TEST_F(ClientHandshakingConnectionCreator, writes_handshake_header_for_single_protocol)
{
    using namespace testing;

    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

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

TEST_F(ClientHandshakingConnectionCreator, dispatches_to_correct_protocol_based_on_server_reply)
{
    using namespace testing;

    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

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
    auto resolver = handshake.connect_to(std::move(transport));

    std::vector<uint8_t> buffer(second_uuid.begin(), second_uuid.end());
    transport_observer->add_received_message(buffer);

    auto proto = reinterpret_cast<void*>(resolver->get().release());
    EXPECT_THAT(proto, Eq(second_protocol_addr));
}

TEST_F(ClientHandshakingConnectionCreator, throws_exception_on_server_protocol_mismatch)
{
    using namespace testing;

    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

    auto transport = std::make_unique<RecordingStreamTransport>();
    std::string const client_uuid{"be094b17-4ca0-40fd-9394-913a4aab05f0"};
    std::string const mismatching_uuid{"ec8480b6-0246-4399-8dc1-c54adf7a8985"};

    auto protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(client_uuid, std::vector<uint8_t>{}));

    std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
    protocols.emplace_back(std::move(protocol));
    mclr::HandshakingConnectionCreator handshake{std::move(protocols)};

    auto* transport_observer = transport.get();
    auto resolver = handshake.connect_to(std::move(transport));

    std::vector<uint8_t> buffer(mismatching_uuid.begin(), mismatching_uuid.end());
    transport_observer->add_received_message(buffer);

    EXPECT_THROW(resolver->get().release(), std::runtime_error);
}

class RpcChannelResolver : public testing::Test
{
public:
    RpcChannelResolver()
        : transport{std::make_unique<RecordingStreamTransport>()},
          transport_observer{transport.get()}
    {
    }

    std::unique_ptr<mclr::RpcChannelResolver> get_resolver()
    {
        auto protocol = std::make_unique<MockProtocolInterpreter>(std::make_unique<MockClientHandshakeProtocol>(uuid, std::vector<uint8_t>{}));

        std::vector<std::unique_ptr<mclr::ProtocolInterpreter>> protocols;
        protocols.emplace_back(std::move(protocol));
        mclr::HandshakingConnectionCreator handshake{std::move(protocols)};
        return handshake.connect_to(std::move(transport));
    }

    void successfully_complete_handhake()
    {
        using namespace std::literals::chrono_literals;
        std::vector<uint8_t> buffer(uuid.begin(), uuid.end());
        transport_observer->add_received_message(buffer);

        auto end = std::chrono::steady_clock::now() + 60s;
        while (mt::fd_is_readable(transport_observer->watch_fd()) &&
               (std::chrono::steady_clock::now() < end))
        {
            std::this_thread::sleep_for(10ms);
        }
    }

private:
    std::unique_ptr<RecordingStreamTransport> transport;
    RecordingStreamTransport* const transport_observer;
    std::string const uuid{"be094b17-4ca0-40fd-9394-913a4aab05f0"};
};

TEST_F(RpcChannelResolver, completion_is_called_immediately_if_set_on_ready_resolver)
{
    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

    auto resolver = get_resolver();

    successfully_complete_handhake();

    bool called{false};
    resolver->set_completion([&called](std::future<std::unique_ptr<google::protobuf::RpcChannel>> future)
    {
        future.get().release();
        called = true;
    });
    EXPECT_TRUE(called);
}

TEST_F(RpcChannelResolver, completion_isnt_called_until_future_is_ready)
{
    using namespace std::literals::chrono_literals;

    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

    auto resolver = get_resolver();

    auto called = std::make_shared<mt::Signal>();
    resolver->set_completion([called](std::future<std::unique_ptr<google::protobuf::RpcChannel>> future)
    {
        future.get().release();
        called->raise();
    });
    EXPECT_FALSE(called->raised());

    successfully_complete_handhake();

    EXPECT_TRUE(called->wait_for(60s));
}

TEST_F(RpcChannelResolver, calling_set_continuation_twice_is_an_error)
{
    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

    auto resolver = get_resolver();

    resolver->set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {});
    EXPECT_THROW(resolver->set_completion([](std::future<std::unique_ptr<google::protobuf::RpcChannel>>) {}),
            std::logic_error);
}

TEST_F(RpcChannelResolver, destruction_cancels_completion)
{
    using namespace std::literals::chrono_literals;

    auto guard = maybe_fork_to_run_test(this);
    if (!guard.run_tests_in_this_process())
    {
        return;
    }

    auto resolver = get_resolver();

    auto called_with_error = std::make_shared<mt::Signal>();
    resolver->set_completion([called_with_error](std::future<std::unique_ptr<google::protobuf::RpcChannel>> future)
    {
        try
        {
            future.get();
        }
        catch(std::future_error& err)
        {
           called_with_error->raise();
        }
    });

    resolver.reset();
    EXPECT_TRUE(called_with_error->wait_for(60s));
}
