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

#include "unityprotobufservice.h"

void UnityProtobufService::copy(::google::protobuf::RpcController* /*controller*/,
        const ::unity::protobuf::Clip* newClip,
        ::unity::protobuf::Void* /*response*/,
        ::google::protobuf::Closure* done)
{
    m_clip = newClip->content();
    done->Run();
}

void UnityProtobufService::paste(::google::protobuf::RpcController* /*controller*/,
        const ::unity::protobuf::Void* /*request*/,
        ::unity::protobuf::Clip* clipReturned,
        ::google::protobuf::Closure* done)
{
    clipReturned->set_content(m_clip);
    done->Run();
}
