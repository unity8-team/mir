/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/frontend/protobuf_message_sender.h"
#include "mir/frontend/message_processor_report.h"
#include "src/server/frontend/display_server.h"
#include "src/server/frontend/protobuf_message_processor.h"
#include "mir_test/fake_shared.h"
#include "mir_protobuf_wire.pb.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mf = mir::frontend;
namespace mfd = mir::frontend::detail;
namespace mt = mir::test;
namespace gp = google::protobuf;
namespace mp = mir::protobuf;
namespace mpw = mir::protobuf::wire;
namespace
{
struct StubProtobufMessageSender : mfd::ProtobufMessageSender
{
    void send_response(gp::uint32, gp::Message*, mfd::FdSets const&) override
    {
    }
};

struct StubMessageProcessorReport : mf::MessageProcessorReport
{
    void received_invocation(void const*, int, std::string const&) override
    {
    }
    void completed_invocation(void const*, int, bool) override
    {
    }
    void unknown_method(void const*, int, std::string const&) override
    {
    }
    void exception_handled(void const*, int, std::exception const&) override
    {
    }
    void exception_handled(void const*, std::exception const&) override
    {
    }
};

struct StubDisplayServer : mfd::DisplayServer
{
    void exchange_buffer(
        gp::RpcController*,
        mp::BufferRequest const*,
        mp::Buffer* response,
        gp::Closure* closure) override
    {   
        exchange_buffer_response = response;
        exchange_closure = closure;
    }
    void client_pid(int) override
    {
    }
    mp::Buffer* exchange_buffer_response;
    gp::Closure* exchange_closure;
};
}

TEST(ProtobufMessageProcessor, preserves_response_resource_for_exchange_buffer)
{
    using namespace testing;
    StubProtobufMessageSender stub_msg_sender;
    StubMessageProcessorReport stub_report;
    StubDisplayServer stub_display_server;
    mfd::ProtobufMessageProcessor pb_message_processor(
        mt::fake_shared(stub_msg_sender),
        mt::fake_shared(stub_display_server),
        mt::fake_shared(stub_report));
    std::shared_ptr<mfd::MessageProcessor> mp = mt::fake_shared(pb_message_processor);

    mpw::Invocation raw_invocation;
    mp::BufferRequest buffer_request;
    std::string str_parameters;
    buffer_request.SerializeToString(&str_parameters);
    raw_invocation.set_parameters(str_parameters.c_str());
    raw_invocation.set_method_name("exchange_buffer");
    mfd::Invocation invocation(raw_invocation);

    std::vector<mir::Fd> fds;
    mp->dispatch(invocation, fds);

    ASSERT_THAT(stub_display_server.exchange_buffer_response, testing::Ne(nullptr));
    ASSERT_THAT(stub_display_server.exchange_closure, testing::Ne(nullptr));
    int num_data{5};
    stub_display_server.exchange_buffer_response->clear_data();
    for(auto i = 0; i < num_data; i++)
        stub_display_server.exchange_buffer_response->add_data(i);

    EXPECT_THAT(stub_display_server.exchange_buffer_response->data().size(), Eq(num_data));
    stub_display_server.exchange_closure->Run();
}
