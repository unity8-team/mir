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


#ifndef MIR_FRONTEND_PROTOBUF_MESSAGE_PROCESSOR_H_
#define MIR_FRONTEND_PROTOBUF_MESSAGE_PROCESSOR_H_

#include "message_processor.h"

#include "mir_protobuf.pb.h"
#include "mir_protobuf_wire.pb.h"

#include <vector>
#include <memory>
#include <iosfwd>
#include <cstdint>

namespace mir
{
namespace protobuf { class DisplayServer; }

namespace frontend
{
class ResourceCache;
class MessageProcessorReport;

namespace detail
{

struct ProtobufMessageProcessor : MessageProcessor
{
    ProtobufMessageProcessor(
        MessageSender* sender,
        std::shared_ptr<protobuf::DisplayServer> const& display_server,
        std::shared_ptr<ResourceCache> const& resource_cache,
        std::shared_ptr<MessageProcessorReport> const& report);

private:
    void send_generic_response(::google::protobuf::uint32 id,
                               google::protobuf::Message* response);

    void send_buffer_response(::google::protobuf::uint32 id,
                              mir::protobuf::Buffer* response);

    void send_platform_response(::google::protobuf::uint32 id,
                                mir::protobuf::Platform* response);

    void send_connection_response(::google::protobuf::uint32 id,
                                  mir::protobuf::Connection* response);

    void send_surface_response(::google::protobuf::uint32 id,
                               mir::protobuf::Surface* response);

    template<class Response>
    std::vector<int32_t> extract_fds_from(Response* response);

    bool process_message(std::istream& msg);

    bool dispatch(mir::protobuf::wire::Invocation const& invocation);

    MessageSender* const sender;
    std::shared_ptr<protobuf::DisplayServer> const display_server;
    std::shared_ptr<ResourceCache> const resource_cache;
    std::shared_ptr<MessageProcessorReport> const report;
};
}
}
}

#endif /* PROTOBUF_MESSAGE_PROCESSOR_H_ */
