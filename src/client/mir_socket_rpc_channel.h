/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_CLIENT_MIR_SOCKET_RPC_CHANNEL_H_
#define MIR_CLIENT_MIR_SOCKET_RPC_CHANNEL_H_

#include "pending_call_cache.h"
#include "mir_logger.h"

#include <boost/asio.hpp>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <thread>
#include <iosfwd>

namespace mir
{
namespace protobuf
{
namespace wire
{
class Invocation;
class Result;
}
}

namespace client
{
class MirSocketRpcChannel : public google::protobuf::RpcChannel
{
public:
    MirSocketRpcChannel();
    MirSocketRpcChannel(const std::string& endpoint, const std::shared_ptr<Logger>& log);
    ~MirSocketRpcChannel();

private:
    mir::protobuf::wire::Invocation invocation_for(
        const google::protobuf::MethodDescriptor* method,
        const google::protobuf::Message* request);

    virtual void CallMethod(const google::protobuf::MethodDescriptor* method, google::protobuf::RpcController*,
        const google::protobuf::Message* parameters, google::protobuf::Message* response,
        google::protobuf::Closure* complete);
    std::shared_ptr<Logger> log;
    detail::PendingCallCache pending_calls;
    UniqueIdGenerator call_ids;
    std::thread io_service_thread;
    boost::asio::io_service io_service;
    boost::asio::io_service::work work;
    boost::asio::local::stream_protocol::endpoint endpoint;
    boost::asio::local::stream_protocol::socket socket;

    static size_t const size_of_header = 2;
    unsigned char header_bytes[size_of_header];

    void receive_file_descriptors(google::protobuf::Message* response, google::protobuf::Closure* complete);
    void send_message(const std::string& body, detail::SendBuffer& buffer);
    void on_message_sent(const boost::system::error_code& error);
    void on_header_read(const boost::system::error_code& error);

    void read_message();

    size_t read_message_header();

    mir::protobuf::wire::Result read_message_body(const size_t body_size);
};

}
}

#endif /* MIR_CLIENT_MIR_SOCKET_RPC_CHANNEL_H_ */
