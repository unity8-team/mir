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

#include <sys/socket.h>
#include <boost/asio.hpp>
#include <uuid/uuid.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace
{
char const* mock_uuid = "28e0bbfb-20e6-4066-ba80-aa38a5538638";

class MockDispatchedSessionCreator : public mir::frontend::DispatchedSessionCreator
{
public:
    MockDispatchedSessionCreator()
    {
        using namespace testing;
        ON_CALL(*this, protocol_id(_))
            .WillByDefault(Invoke([](uuid_t id){ uuid_parse(mock_uuid, id); }));
    }

    MOCK_METHOD2(create_session_for, void(const std::shared_ptr<boost::asio::local::stream_protocol::socket> &, std::string const&));
    MOCK_CONST_METHOD1(protocol_id, void(uuid_t id));
    MOCK_CONST_METHOD0(header_size, size_t(void));
};

class NullSessionAuthorizer : public mir::frontend::SessionAuthorizer
{
public:
    bool connection_is_allowed(pid_t /*pid*/)
    {
        return true;
    }
    bool configure_display_is_allowed(pid_t /*pid*/)
    {
        return true;
    }
};
}

TEST(DispatchingSessionCreatorTest, DispatchesSingleUUIDSupported)
{
    using namespace testing;
    auto authorizer = std::make_shared<NullSessionAuthorizer> ();
    auto mock_proto = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedSessionCreator>>>();
    protos->push_back(mock_proto);

    mir::frontend::DispatchingSessionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    ASSERT_TRUE(write(socket_fds[0], mock_uuid, strlen(mock_uuid)) == static_cast<ssize_t>(strlen(mock_uuid)));
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    EXPECT_CALL(*mock_proto, create_session_for(_, StrEq("")));

    dispatcher.create_session_for(reader);

    io_service.run();
}

TEST(DispatchingSessionCreatorTest, DispatchesToCorrectUUID)
{
    using namespace testing;
    auto authorizer = std::make_shared<NullSessionAuthorizer> ();
    auto mock_proto_one = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto mock_proto_two = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedSessionCreator>>>();
    protos->push_back(mock_proto_one);
    protos->push_back(mock_proto_two);

    char const* proto_two_uuid = "eba0cf92-30c2-4375-9560-305f592a4161";

    mir::frontend::DispatchingSessionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    ASSERT_TRUE(write(socket_fds[0], proto_two_uuid, strlen(proto_two_uuid)) == static_cast<ssize_t>(strlen(proto_two_uuid)));
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto_two, protocol_id(_))
        .WillByDefault(Invoke([proto_two_uuid](uuid_t id) { uuid_parse(proto_two_uuid, id);}));
    EXPECT_CALL(*mock_proto_two, create_session_for(_, StrEq("")));

    dispatcher.create_session_for(reader);

    io_service.run();
}

TEST(DispatchingSessionCreatorTest, NoCommonProtocolRaisesException)
{
    using namespace testing;
    auto authorizer = std::make_shared<NullSessionAuthorizer> ();
    auto mock_proto_one = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto mock_proto_two = std::make_shared<NiceMock<MockDispatchedSessionCreator>>();
    auto protos = std::make_shared<std::vector<std::shared_ptr<mir::frontend::DispatchedSessionCreator>>>();
    protos->push_back(mock_proto_one);
    protos->push_back(mock_proto_two);

    char const* proto_two_uuid = "eba0cf92-30c2-4375-9560-305f592a4161";
    std::string unknown_client_proto("d29d39ba-c5fe-44a8-93c0-126ca9c4a41f");

    mir::frontend::DispatchingSessionCreator dispatcher(protos, authorizer);

    boost::asio::io_service io_service;
    int socket_fds[2];
    ASSERT_TRUE(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socket_fds) == 0);

    ASSERT_TRUE(write(socket_fds[0], unknown_client_proto.data(), unknown_client_proto.length())
            == static_cast<ssize_t>(unknown_client_proto.length()));
    auto reader = std::make_shared<boost::asio::local::stream_protocol::socket>(io_service,
                                                                                boost::asio::local::stream_protocol(),
                                                                                socket_fds[1]);

    ON_CALL(*mock_proto_two, protocol_id(_))
        .WillByDefault(Invoke([proto_two_uuid](uuid_t id) { uuid_parse(proto_two_uuid, id);}));

    dispatcher.create_session_for(reader);

    EXPECT_THROW({ io_service.run(); }, std::runtime_error);
}
