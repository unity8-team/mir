/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_TEST_TEST_CLIENT_H_
#define MIR_TEST_TEST_CLIENT_H_

#include "mir_protobuf.pb.h"
#include "src/client/make_rpc_channel.h"

#include <gmock/gmock.h>

#include <atomic>

namespace mir
{
namespace test
{
namespace doubles
{
class MockRpcReport;
}
struct TestProtobufClient
{
    TestProtobufClient(std::string socket_file, int timeout_ms);

    std::shared_ptr<doubles::MockRpcReport> rpc_report;
    std::shared_ptr<google::protobuf::RpcChannel> channel;
    mir::protobuf::DisplayServer::Stub display_server;
    mir::protobuf::ConnectParameters connect_parameters;
    mir::protobuf::SurfaceParameters surface_parameters;
    mir::protobuf::Surface surface;
    mir::protobuf::Void ignored;
    mir::protobuf::Connection connection;

    MOCK_METHOD0(connect_done, void ());
    MOCK_METHOD0(create_surface_done, void ());
    MOCK_METHOD0(next_buffer_done, void ());
    MOCK_METHOD0(release_surface_done, void ());
    MOCK_METHOD0(disconnect_done, void ());

    void on_connect_done();

    void on_create_surface_done();

    void on_next_buffer_done();

    void on_release_surface_done();

    void on_disconnect_done();

    void wait_for_connect_done();

    void wait_for_create_surface();

    void wait_for_next_buffer();

    void wait_for_release_surface();

    void wait_for_disconnect_done();

    void wait_for_surface_count(int count);

    void wait_for_disconnect_count(int count);

    void tfd_done();

    void wait_for_tfd_done();

    const int maxwait;
    std::atomic<bool> connect_done_called;
    std::atomic<bool> create_surface_called;
    std::atomic<bool> next_buffer_called;
    std::atomic<bool> release_surface_called;
    std::atomic<bool> disconnect_done_called;
    std::atomic<bool> tfd_done_called;

    std::atomic<int> connect_done_count;
    std::atomic<int> create_surface_done_count;
    std::atomic<int> disconnect_done_count;
};
}
}
#endif /* MIR_TEST_TEST_CLIENT_H_ */
