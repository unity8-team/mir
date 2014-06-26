/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mir/frontend/message_processor.h>
#include <mir/frontend/protobuf_message_sender.h>

#include "unityrpc.h"

#include <memory>

class MessageProcessor : public mir::frontend::detail::MessageProcessor
{
public:
    MessageProcessor(
        std::shared_ptr<unity::protobuf::UnityService> const& unityService,
        std::shared_ptr<mir::frontend::detail::ProtobufMessageSender> const& sender,
        std::shared_ptr<mir::frontend::detail::MessageProcessor> const& wrapped) :
        m_sender(sender),
        m_wrapped(wrapped),
        m_unityService(unityService) {}

    bool dispatch(mir::frontend::detail::Invocation const& invocation) override;
    void client_pid(int pid) override;

    void send_response(::google::protobuf::uint32 id, ::google::protobuf::Message* response);

private:
    std::shared_ptr<mir::frontend::detail::ProtobufMessageSender> const m_sender;
    std::shared_ptr<mir::frontend::detail::MessageProcessor> const m_wrapped;
    std::shared_ptr<unity::protobuf::UnityService> const m_unityService;
};
