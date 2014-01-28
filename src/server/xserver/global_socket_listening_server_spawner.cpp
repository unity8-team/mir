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

namespace mx = mir::X;

mx::GlobalSocketListeningServerContext::GlobalSocketListeningServerContext(std::shared_ptr<mir::process::Handle> server_handle, std::string connection_string)
    : server_handle(server_handle),
      connection_string(connection_string)
{
}

char const* mx::GlobalSocketListeningServerContext::client_connection_string()
{
    return connection_string.c_str();
}

std::future<std::unique_ptr<mx::ServerContext>> mx::GlobalSocketListeningServerSpawner::create_server(std::shared_ptr<mir::process::Spawner> const& spawner)
{
    return std::async(std::launch::async, [spawner]()
    {
        mir::pipe::Pipe displayfd_pipe;
        auto displayfd = std::to_string(displayfd_pipe.write_fd());

        auto future_handle = spawner->run_from_path("Xorg",
                                                    {"-displayfd", displayfd.c_str()},
                                                    {displayfd_pipe.write_fd()});

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
