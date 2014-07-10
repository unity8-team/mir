/*
 * Copyright © 2014 Canonical Ltd.
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

#include "src/server/frontend/dispatching_session_creator.h"
#include "src/server/frontend/dispatched_session_creator.h"
#include "mir/frontend/connection_context.h"

#include "mir_test_doubles/stub_session_authorizer.h"

#include <sys/socket.h>
#include <boost/asio.hpp>
#include <uuid/uuid.h>
#include <thread>
#include <condition_variable>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtd = mir::test::doubles;

namespace
{
char const* mock_uuid = "28e0bbfb-20e6-4066-ba80-aa38a5538638";

void write_header(int socket, std::string const& uuid, uint16_t header_size)
{
    uint16_t const network_header = htons(header_size);
    ASSERT_TRUE(write(socket, &network_header, sizeof network_header)
                == static_cast<ssize_t>(sizeof network_header));
    ASSERT_TRUE(write(socket, uuid.data(), 36)
                == static_cast<ssize_t>(36));
}

class MockDispatchedSessionCreator : public mir::frontend::DispatchedConnectionCreator
{
public:
    MockDispatchedSessionCreator()
    {
        using namespace testing;
        ON_CALL(*this, protocol_id(_))
            .WillByDefault(Invoke([](uuid_t id){ uuid_parse(mock_uuid, id); }));
    }

    MOCK_METHOD3(create_connection_for, void(const std::shared_ptr<boost::asio::local::stream_protocol::socket> &, mir::frontend::ConnectionContext const&, std::string const&));
    MOCK_CONST_METHOD1(protocol_id, void(uuid_t id));
    MOCK_CONST_METHOD0(header_size, size_t(void));
};

}

TEST(DispatchingConnectionCreatorTest, DispatchesSingleUUIDSupported)
{
    using namespace testing;
    auto authorizer = std::make_shared<mtd::StubSessionAuthorizer> ();
    auto mock_proto = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedConnectionCreator>>>();
    protos->push_back(mock_proto);

    mir::frontend::DispatchingConnectionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    write_header(socket_fds[0], mock_uuid, mock_proto->header_size());
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    EXPECT_CALL(*mock_proto, create_connection_for(_, _,StrEq("")));

    dispatcher.create_connection_for(reader, mir::frontend::ConnectionContext(nullptr));

    io_service.run();
}

TEST(DispatchingConnectionCreatorTest, DispatchesToCorrectUUID)
{
    using namespace testing;
    auto authorizer = std::make_shared<mtd::StubSessionAuthorizer> ();
    auto mock_proto_one = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto mock_proto_two = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedConnectionCreator>>>();
    protos->push_back(mock_proto_one);
    protos->push_back(mock_proto_two);

    char const* proto_two_uuid = "eba0cf92-30c2-4375-9560-305f592a4161";

    mir::frontend::DispatchingConnectionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    write_header(socket_fds[0], proto_two_uuid, mock_proto_two->header_size());
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto_two, protocol_id(_))
        .WillByDefault(Invoke([proto_two_uuid](uuid_t id) { uuid_parse(proto_two_uuid, id);}));
    EXPECT_CALL(*mock_proto_two, create_connection_for(_, _, StrEq("")));

    dispatcher.create_connection_for(reader, mir::frontend::ConnectionContext(nullptr));

    io_service.run();
}

TEST(DispatchingConnectionCreatorTest, NoCommonProtocolRaisesException)
{
    using namespace testing;
    auto authorizer = std::make_shared<mtd::StubSessionAuthorizer> ();
    auto mock_proto_one = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto mock_proto_two = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedConnectionCreator>>>();
    protos->push_back(mock_proto_one);
    protos->push_back(mock_proto_two);

    char const* proto_two_uuid = "eba0cf92-30c2-4375-9560-305f592a4161";
    std::string unknown_client_proto("d29d39ba-c5fe-44a8-93c0-126ca9c4a41f");

    mir::frontend::DispatchingConnectionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    write_header(socket_fds[0], unknown_client_proto, 0);
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto_two, protocol_id(_))
        .WillByDefault(Invoke([proto_two_uuid](uuid_t id) { uuid_parse(proto_two_uuid, id);}));

    dispatcher.create_connection_for(reader, mir::frontend::ConnectionContext(nullptr));

    EXPECT_THROW({ io_service.run(); }, std::runtime_error);
}

TEST(DispatchingConnectionCreatorTest, LazyClientTimesOutInConnect)
{
    using namespace testing;
    auto authorizer = std::make_shared<mtd::StubSessionAuthorizer> ();
    auto mock_proto = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedConnectionCreator>>>();
    protos->push_back(mock_proto);

    mir::frontend::DispatchingConnectionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    ASSERT_TRUE(write(socket_fds[0], "partial-uuid-...", strlen("partial-uuid-..."))
            == static_cast<ssize_t>(strlen("partial-uuid-...")));
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto, header_size())
        .WillByDefault(Return(8));

    dispatcher.create_connection_for(reader, mir::frontend::ConnectionContext(nullptr));

    std::mutex m;
    std::condition_variable cv;
    bool done{false};

    std::thread watchdog{[&io_service, &m, &cv, &done]()
        {
            std::unique_lock<std::mutex> lk(m);
            if (!cv.wait_for(lk, std::chrono::seconds{1}, [&done]() { return done; }))
            {
                io_service.stop();
                FAIL()<<"Timeout waiting for client disconnect";
            }
        }};

    EXPECT_THROW({ io_service.run(); }, std::runtime_error);

    {
        std::unique_lock<std::mutex> lk(m);
        done = true;
    }
    cv.notify_one();

    if (watchdog.joinable())
        watchdog.join();
}


TEST(DispatchingConnectionCreatorTest, IncorrectProtocolSizeRaisesException)
{
    using namespace testing;
    auto authorizer = std::make_shared<mtd::StubSessionAuthorizer> ();
    auto mock_proto = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedConnectionCreator>>>();
    protos->push_back(mock_proto);

    mir::frontend::DispatchingConnectionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    uint16_t const mock_header_length{8};
    write_header(socket_fds[0], mock_uuid, mock_header_length - 1);
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto, header_size())
        .WillByDefault(Return(mock_header_length));

    dispatcher.create_connection_for(reader, mir::frontend::ConnectionContext(nullptr));

    EXPECT_THROW({ io_service.run(); }, std::runtime_error);
}
