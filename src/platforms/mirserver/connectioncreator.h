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

#ifndef CONNECTIONCREATOR_H
#define CONNECTIONCREATOR_H

#include <mir/frontend/protobuf_connection_creator.h>

namespace unity {
    namespace protobuf {
        class UnityService;
    }
}

class ConnectionCreator : public mir::frontend::ProtobufConnectionCreator
{
public:
    ConnectionCreator(
        std::shared_ptr<unity::protobuf::UnityService> const& unityService,
        std::shared_ptr<mir::frontend::ProtobufIpcFactory> const& ipc_factory,
        std::shared_ptr<mir::frontend::SessionAuthorizer> const& session_authorizer,
        std::shared_ptr<mir::frontend::MessageProcessorReport> const& report);

    std::shared_ptr<mir::frontend::detail::MessageProcessor> create_processor(
        std::shared_ptr<mir::frontend::detail::ProtobufMessageSender> const& sender,
        std::shared_ptr<mir::frontend::detail::DisplayServer> const& display_server,
        std::shared_ptr<mir::frontend::MessageProcessorReport> const& report) const override;
private:
    std::shared_ptr<unity::protobuf::UnityService> m_unityService;
};

#endif // CONNECTIONCREATOR_H

