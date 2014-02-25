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

namespace
{
struct Prefix
{
    template<int Size>
    Prefix(char const (&prefix)[Size]) : size(Size-1), prefix(prefix) {}

    bool is_start_of(std::string const& name) const
    { return !strncmp(name.c_str(), prefix, size); }

    int const size;
    char const* const prefix;
} const fd_prefix("fd://");
}

std::shared_ptr<google::protobuf::RpcChannel>
mclr::make_rpc_channel(std::string const& name,
                       std::shared_ptr<mcl::SurfaceMap> const& map,
                       std::shared_ptr<mcl::DisplayConfiguration> const& disp_conf,
                       std::shared_ptr<RpcReport> const& rpc_report,
                       std::shared_ptr<mcl::LifecycleControl> const& lifecycle_control)
{
    int fd;
    if (fd_prefix.is_start_of(name))
    {
        fd = atoi(name.c_str()+fd_prefix.size);
    }
    else
    {
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, name.c_str());

        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            BOOST_THROW_EXCEPTION(
                        boost::enable_error_info(
                            std::runtime_error(std::string("Failed to create socket: ") +
                                               strerror(errno)))<<boost::errinfo_errno(errno));
        }
        if (connect(fd, (struct sockaddr* const)&addr, sizeof addr) < 0)
        {
            BOOST_THROW_EXCEPTION(
                        boost::enable_error_info(
                            std::runtime_error(std::string("Failed to connect to server socket: ") +
                                               strerror(errno)))<<boost::errinfo_errno(errno));
        }
    }

    // Do the protocol dance here, as this is only used in testing.
    if (write(fd, "60019143-2648-4904-9719-7817f0b9fb13", 36) != 36)
    {
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(
                std::runtime_error("Failed to send client protocol string"))<<boost::errinfo_errno(errno));
    }

    return std::make_shared<MirSocketRpcChannel>(fd, map, disp_conf, rpc_report, lifecycle_control);
}
