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

#include "messageprocessor.h"
#include <mir/frontend/template_protobuf_message_processor.h>

bool MessageProcessor::dispatch(mir::frontend::detail::Invocation const& invocation)
{
    if ("copy" == invocation.method_name()) {
        mir::frontend::detail::invoke(
            this,
            m_unityService.get(),
            &unity::protobuf::UnityService::copy,
            invocation);
        return true;
    } else if ("paste" == invocation.method_name()) {
        mir::frontend::detail::invoke(
            this,
            m_unityService.get(),
            &unity::protobuf::UnityService::paste,
            invocation);
        return true;
    } else {
        return m_wrapped->dispatch(invocation);
    }
}

void MessageProcessor::client_pid(int pid)
{
    m_wrapped->client_pid(pid);
}

void MessageProcessor::send_response(::google::protobuf::uint32 id, ::google::protobuf::Message* response)
{
    m_sender->send_response(id, response, {});
}
