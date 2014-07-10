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

#include "make_rpc_channel.h"
#include "mir_socket_rpc_channel.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <cstring>

namespace mcl = mir::client;
namespace mclr = mir::client::rpc;

std::shared_ptr<google::protobuf::RpcChannel>
mclr::make_rpc_channel(int fd,
                       std::shared_ptr<mcl::SurfaceMap> const& map,
                       std::shared_ptr<mcl::DisplayConfiguration> const& disp_conf,
                       std::shared_ptr<RpcReport> const& rpc_report,
                       std::shared_ptr<mcl::LifecycleControl> const& lifecycle_control,
                       std::shared_ptr<mcl::EventSink> const& event_sink)
{
    return std::make_shared<MirSocketRpcChannel>(fd, map, disp_conf, rpc_report, lifecycle_control, event_sink);
}

std::shared_ptr<google::protobuf::RpcChannel>
mclr::make_rpc_channel(std::string const& name,
                       std::shared_ptr<mcl::SurfaceMap> const& map,
                       std::shared_ptr<mcl::DisplayConfiguration> const& disp_conf,
                       std::shared_ptr<RpcReport> const& rpc_report,
                       std::shared_ptr<mcl::LifecycleControl> const& lifecycle_control,
                       std::shared_ptr<mcl::EventSink> const& event_sink)
{
    return std::make_shared<MirSocketRpcChannel>(name, map, disp_conf, rpc_report, lifecycle_control, event_sink);
}
