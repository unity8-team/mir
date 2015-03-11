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
#include "mir_protobuf_rpc_channel.h"
#include "protocol_interpreter.h"
#include "stream_socket_transport.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <cstring>

struct Prefix
{
    template <int Size>
    Prefix(char const (&prefix)[Size])
        : size(Size - 1), prefix(prefix)
    {
    }

    bool is_start_of(std::string const& name) const
    {
        return !strncmp(name.c_str(), prefix, size);
    }

    int const size;
    char const* const prefix;
} const fd_prefix("fd://");

std::unique_ptr<mir::client::rpc::StreamTransport> mir::client::rpc::transport_for(std::string const& name)
{
    if (fd_prefix.is_start_of(name))
    {
        auto const fd = atoi(name.c_str() + fd_prefix.size);
        return std::make_unique<mir::client::rpc::StreamSocketTransport>(mir::Fd{fd});
    }
    else
    {
        return std::make_unique<mir::client::rpc::StreamSocketTransport>(name);
    }
}
