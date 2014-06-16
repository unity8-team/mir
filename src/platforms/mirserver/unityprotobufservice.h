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

#ifndef UNITY_PROTOBUF_SERVICE_H
#define UNITY_PROTOBUF_SERVICE_H

#include "unityrpc.h"
#include <string>

class UnityProtobufService : public unity::protobuf::UnityService
{
public:
    void copy(::google::protobuf::RpcController* controller,
            const ::unity::protobuf::Clip* request,
            ::unity::protobuf::Void* response,
            ::google::protobuf::Closure* done) override;

    void paste(::google::protobuf::RpcController* controller,
            const ::unity::protobuf::Void* request,
            ::unity::protobuf::Clip* response,
            ::google::protobuf::Closure* done) override;

private:
    std::string m_clip;
};

#endif // UNITY_PROTOBUF_SERVICE_H
