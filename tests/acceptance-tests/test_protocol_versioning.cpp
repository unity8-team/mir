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

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/in_process_server.h"
#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/using_stub_client_platform.h"

#include "mir/frontend/handshake_protocol.h"
#include "src/server/frontend/protobuf_protocol.h"
#include "src/server/frontend/handshaking_connection_creator.h"
#include "src/server/frontend/protobuf_ipc_factory.h"

#include <functional>
#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mf = mir::frontend;
namespace mtf = mir_test_framework;

namespace
{
class ProtocolVersioningTest : public mtf::InProcessServer
{
public:
    ProtocolVersioningTest()
    {
        protocol_impls = std::make_shared<std::vector<std::shared_ptr<mf::ProtocolInterpreter>>>();
    }

    ~ProtocolVersioningTest()
    {
        protocol_impls.reset();
    }

    mir::DefaultServerConfiguration& server_config() override
    {
        return config;
    }

    static std::shared_ptr<std::vector<std::shared_ptr<mf::ProtocolInterpreter>>> protocol_impls;

    class DispatcherOverridingServerConfig : public mtf::StubbedServerConfiguration
    {
    public:
        std::shared_ptr<mf::ProtobufProtocol> make_protobuf_connection_creator()
        {
            return std::make_shared<mf::ProtobufProtocol>(
                new_ipc_factory(),
                the_message_processor_report());
        }

        std::shared_ptr<std::vector<std::shared_ptr<mf::ProtocolInterpreter>>> the_connection_protocols() override
        {
            return protocol_impls;
        }
    } config;
};
std::shared_ptr<std::vector<std::shared_ptr<mir::frontend::ProtocolInterpreter>>> ProtocolVersioningTest::protocol_impls;

class DummySessionCreator : public mf::ProtocolInterpreter
{
public:
    DummySessionCreator()
    {
    }

    void create_connection_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& /*socket*/,
                               mir::frontend::SessionAuthorizer&,
                               mir::frontend::ConnectionContext const&,
                               std::string const& /*connection_data*/) override
    {
    }

    mf::HandshakeProtocol& connection_protocol() override
    {
        return conn_proto;
    }

    class DummyConnectionProtocol : public mf::HandshakeProtocol
    {
    public:
        void protocol_id(uuid_t id) const override
        {
            uuid_parse("f14e4484-b475-4406-a0ed-cd01fbcf356a", id);
        }

        size_t header_size() const override
        {
            return 8;
        }

        void write_client_header(uint8_t*) const override
        {
        }

        void send_server_header() override
        {
        }

        void receive_server_header(mir::client::rpc::StreamTransport&) override
        {
        }
    } conn_proto;
};
}

TEST_F(ProtocolVersioningTest, ClientV1ConnectsToServerV1or2)
{
    config.the_connection_protocols()->push_back(std::make_shared<DummySessionCreator>());
    config.the_connection_protocols()->push_back(config.make_protobuf_connection_creator());

    auto connection = mir_connect_sync(new_connection().c_str(), "test-client");
    ASSERT_TRUE(mir_connection_is_valid(connection));

    MirSurfaceParameters const request_params =
    {
        "input-test-client",
        600, 600,
        mir_pixel_format_argb_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };
    auto surface = mir_connection_create_surface_sync(connection, &request_params);
    ASSERT_TRUE(mir_surface_is_valid(surface));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}

TEST_F(ProtocolVersioningTest, client_v1_or_v2_connects_to_server_v1)
{
    config.the_connection_protocols()->push_back(config.make_protobuf_connection_creator());

    auto connection = mir_connect_sync(new_connection().c_str(), "test-client");
    ASSERT_TRUE(mir_connection_is_valid(connection));

    MirSurfaceParameters const request_params =
    {
        "input-test-client",
        600, 600,
        mir_pixel_format_argb_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };
    auto surface = mir_connection_create_surface_sync(connection, &request_params);
    ASSERT_TRUE(mir_surface_is_valid(surface));

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}
