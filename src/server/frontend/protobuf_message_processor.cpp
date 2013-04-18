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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "protobuf_message_processor.h"
#include "mir/frontend/message_processor_report.h"
#include "mir/frontend/resource_cache.h"

#include <boost/exception/diagnostic_information.hpp>
#include <google/protobuf/descriptor.h>

#include <sstream>
#include <cstdio>

namespace mfd = mir::frontend::detail;

mfd::ProtobufMessageProcessor::ProtobufMessageProcessor(
    MessageSender* sender,
    std::shared_ptr<protobuf::DisplayServer> const& display_server,
    std::shared_ptr<ResourceCache> const& resource_cache,
    std::shared_ptr<MessageProcessorReport> const& report) :
    sender(sender),
    display_server(display_server),
    resource_cache(resource_cache),
    report(report)
{
}

void mfd::ProtobufMessageProcessor::send_buffer_response(
    ::google::protobuf::uint32 id, mir::protobuf::Buffer* response)
{
    const auto& fd = extract_fds_from(response);
    send_generic_response(id, response);
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_platform_response(
    ::google::protobuf::uint32 id, mir::protobuf::Platform* response)
{
    const auto& fd = extract_fds_from(response);
    send_generic_response(id, response);
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_connection_response(
    ::google::protobuf::uint32 id, mir::protobuf::Connection* response)
{
    const auto& fd = response->has_platform() ?
        extract_fds_from(response->mutable_platform()) :
        std::vector<int32_t>();

    send_generic_response(id, response);
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_surface_response(
    ::google::protobuf::uint32 id, mir::protobuf::Surface* response)
{
    auto const& surface_fd = extract_fds_from(response);
    const auto& buffer_fd = response->has_buffer() ?
        extract_fds_from(response->mutable_buffer()) :
        std::vector<int32_t>();

    send_generic_response(id, response);
    sender->send_fds(surface_fd);
    sender->send_fds(buffer_fd);
    resource_cache->free_resource(response);
}

template<class Response>
std::vector<int32_t> mfd::ProtobufMessageProcessor::extract_fds_from(Response* response)
{
    std::vector<int32_t> fd(response->fd().data(), response->fd().data() + response->fd().size());
    response->clear_fd();
    response->set_fds_on_side_channel(fd.size());
    return fd;
}

void mfd::ProtobufMessageProcessor::send_generic_response(
    ::google::protobuf::uint32 id,
    google::protobuf::Message* response)
{
    std::string buffer;
    response->SerializeToString(&buffer);

    mir::protobuf::wire::Result result;
    result.set_id(id);
    result.set_response(buffer);

    result.SerializeToString(&buffer);

    sender->send(buffer);
}

bool mfd::ProtobufMessageProcessor::dispatch(mir::protobuf::wire::Invocation const& invocation)
{
    ::google::protobuf::uint32 id = invocation.id();

    report->received_invocation(display_server.get(), id,
      "TODO");
      //invocation.method_name());

    using namespace ::google::protobuf;

    bool result = true;
    const ServiceDescriptor *desc = display_server->GetDescriptor();
    const MethodDescriptor *method = desc->method(invocation.method_name());

    if (!method)
    {
        report->unknown_method(display_server.get(), id,
                               "TODO"); //invocation.method_name());
        return false;
    }

    typedef void (mfd::ProtobufMessageProcessor::*SendResponse)(
        ::google::protobuf::uint32, Message *);

    const std::string &name = method->name();
    SendResponse send_response =
        &ProtobufMessageProcessor::send_generic_response;
    if (name == "next_buffer")
        send_response = (SendResponse)
            &ProtobufMessageProcessor::send_buffer_response;
    else if (name == "connect")
        send_response = (SendResponse)
            &ProtobufMessageProcessor::send_connection_response;
    else if (name == "create_surface")
        send_response = (SendResponse)
            &ProtobufMessageProcessor::send_surface_response;
 
    Message *request  = display_server->GetRequestPrototype (method).New();
    Message *response = display_server->GetResponsePrototype(method).New();

    request->ParseFromString(invocation.parameters());

    std::unique_ptr<google::protobuf::Closure> callback(
        google::protobuf::NewPermanentCallback(this,
            send_response,
            id,
            response));

    fprintf(stderr, "Call %s\n", method->name().c_str());

    try
    {
        display_server->CallMethod(method, 0, request, response, callback.get());
    }
    catch (std::exception const& x)
    {
        // TODO response->set_error(boost::diagnostic_information(x));
        (this->*send_response)(id, response);
        report->exception_handled(display_server.get(), id, x);
        result = false;
    }

    report->completed_invocation(display_server.get(), id, result);

    return result;
}


bool mfd::ProtobufMessageProcessor::process_message(std::istream& msg)
{
    try
    {
        mir::protobuf::wire::Invocation invocation;
        invocation.ParseFromIstream(&msg);

        return dispatch(invocation);
    }
    catch (std::exception const& error)
    {
        report->exception_handled(display_server.get(), error);
        return false;
    }
}
