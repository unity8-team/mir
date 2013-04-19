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
#include <google/protobuf/descriptor.h>
#include <boost/exception/diagnostic_information.hpp>

#include <sstream>
#include <cassert>

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

template<class ResultMessage>
void mfd::ProtobufMessageProcessor::send_response(::google::protobuf::uint32 id, ResultMessage* response)
{
    send_response(id, static_cast<google::protobuf::Message*>(response));
}

void mfd::ProtobufMessageProcessor::send_response(::google::protobuf::uint32 id, mir::protobuf::Buffer* response)
{
    const auto& fd = extract_fds_from(response);
    send_response(id, static_cast<google::protobuf::Message*>(response));
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_response(::google::protobuf::uint32 id, mir::protobuf::Platform* response)
{
    const auto& fd = extract_fds_from(response);
    send_response(id, static_cast<google::protobuf::Message*>(response));
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_response(::google::protobuf::uint32 id, mir::protobuf::Connection* response)
{
    const auto& fd = response->has_platform() ?
        extract_fds_from(response->mutable_platform()) :
        std::vector<int32_t>();

    send_response(id, static_cast<google::protobuf::Message*>(response));
    sender->send_fds(fd);
    resource_cache->free_resource(response);
}

void mfd::ProtobufMessageProcessor::send_response(::google::protobuf::uint32 id, mir::protobuf::Surface* response)
{
    auto const& surface_fd = extract_fds_from(response);
    const auto& buffer_fd = response->has_buffer() ?
        extract_fds_from(response->mutable_buffer()) :
        std::vector<int32_t>();

    send_response(id, static_cast<google::protobuf::Message*>(response));
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

template<class ParameterMessage, class ResultMessage>
void mfd::ProtobufMessageProcessor::invoke(
    void (protobuf::DisplayServer::*function)(
        ::google::protobuf::RpcController* controller,
        const ParameterMessage* request,
        ResultMessage* response,
        ::google::protobuf::Closure* done),
    mir::protobuf::wire::Invocation const& invocation)
{
    ParameterMessage parameter_message;
    parameter_message.ParseFromString(invocation.parameters());
    ResultMessage result_message;

    try
    {
        std::unique_ptr<google::protobuf::Closure> callback(
            google::protobuf::NewPermanentCallback(this,
                &ProtobufMessageProcessor::send_response,
                invocation.id(),
                &result_message));

        (display_server.get()->*function)(
            0,
            &parameter_message,
            &result_message,
            callback.get());
    }
    catch (std::exception const& x)
    {
        result_message.set_error(boost::diagnostic_information(x));
        send_response(invocation.id(), &result_message);
    }
}

void mfd::ProtobufMessageProcessor::send_response(
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
    using namespace ::google::protobuf;
    const ServiceDescriptor *service = display_server->GetDescriptor();
    int index = invocation.method_index();
    const MethodDescriptor *method = service->method(index);
    const std::string name = method ? method->name() : to_string(index);

    report->received_invocation(display_server.get(), invocation.id(), name);

    bool result = true;

    try
    {
        switch (index)
        {  // One feature Protobuf is missing is enums for these. So use ints..
        case 0:
            assert(name == "connect");
            invoke(&protobuf::DisplayServer::connect, invocation);
            break;
        case 1:
            assert(name == "disconnect");
            invoke(&protobuf::DisplayServer::disconnect, invocation);
            result = false;
            break;
        case 2:
            assert(name == "create_surface");
            invoke(&protobuf::DisplayServer::create_surface, invocation);
            break;
        case 3:
            assert(name == "next_buffer");
            invoke(&protobuf::DisplayServer::next_buffer, invocation);
            break;
        case 4:
            assert(name == "release_surface");
            invoke(&protobuf::DisplayServer::release_surface, invocation);
            break;
        case 5:
            assert(name == "select_focus_by_lightdm_id");
            invoke(&protobuf::DisplayServer::select_focus_by_lightdm_id,
                   invocation);
            break;
        case 6:
            assert(name == "drm_auth_magic");
            invoke(&protobuf::DisplayServer::drm_auth_magic, invocation);
            break;
        case 7:
            assert(name == "test_file_descriptors");
            invoke(&protobuf::DisplayServer::test_file_descriptors,
                   invocation);
            break;
        case 8:
            assert(name == "configure_surface");
            invoke(&protobuf::DisplayServer::configure_surface, invocation);
            break;
        default:
            report->unknown_method(display_server.get(), invocation.id(),
                name);
            result = false;
            break;
        }
    }
    catch (std::exception const& error)
    {
        report->exception_handled(display_server.get(), invocation.id(), error);
        result = false;
    }

    report->completed_invocation(display_server.get(), invocation.id(), result);

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
