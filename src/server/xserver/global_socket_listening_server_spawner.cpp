/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <unistd.h>
#include <errno.h>

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include "global_socket_listening_server_spawner.h"
#include "mir/process/handle.h"

namespace mx = mir::X;

mx::GlobalSocketListeningServerContext::GlobalSocketListeningServerContext(std::unique_ptr<mir::process::Handle> server_handle, std::string connection_string)
    : server_handle(std::move(server_handle)),
      connection_string(connection_string)
{
}

char const* mx::GlobalSocketListeningServerContext::client_connection_string()
{
    return connection_string.c_str();
}

std::future<std::unique_ptr<mx::ServerContext>> mx::GlobalSocketListeningServerSpawner::create_server(std::shared_ptr<mir::process::Spawner> const& spawner, std::shared_ptr<mir::frontend::Connector> const& connector)
{
    return std::async(std::launch::async, [spawner, connector]()
    {
        mir::pipe::Pipe displayfd_pipe;
        auto displayfd = std::to_string(displayfd_pipe.write_fd());
        int mir_fd = connector->client_socket_fd();
        auto mir_fd_arg = std::string("fd://") + std::to_string(mir_fd);

        auto future_handle = spawner->run_from_path("Xorg",
                                                    {"-displayfd", displayfd.c_str(),
                                                     "-mir", "xserver",
                                                     "-mirSocket", mir_fd_arg.c_str()},
                                                    {displayfd_pipe.write_fd(), mir_fd});

        char display_number[10];
        errno = 0;
        int bytes_read = read(displayfd_pipe.read_fd(), display_number, sizeof display_number);

        while (bytes_read == -1 && errno == EINTR)
            bytes_read = read(displayfd_pipe.read_fd(), display_number, sizeof display_number);;

        if (errno != 0)
            BOOST_THROW_EXCEPTION(boost::enable_error_info(std::runtime_error("Failed to receive display number from Xserver"))
                                  << boost::errinfo_errno(errno));

        display_number[bytes_read] = '\0';

        return std::unique_ptr<mx::ServerContext>(new mx::GlobalSocketListeningServerContext(future_handle.get(), std::string(":") + display_number));
    });
}
