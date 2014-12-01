/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_FD_SOCKET_TRANSMISSION_H_
#define MIR_FD_SOCKET_TRANSMISSION_H_
#include "mir/fd.h"
#include <vector>
#include <system_error>
#include <stdexcept>

namespace mir
{
struct socket_error : std::system_error
{
    socket_error(std::string const& message);
};

struct socket_disconnected_error : std::system_error
{
    socket_disconnected_error(std::string const& message);
};

struct fd_reception_error : std::runtime_error
{
    fd_reception_error(std::string const& message);
};

bool socket_error_is_transient(int error_code);
void send_fds(mir::Fd const& socket, std::vector<mir::Fd> const& fd);
void receive_data(mir::Fd const& socket, void* buffer, size_t bytes_requested, std::vector<mir::Fd>& fds);
}
#endif /* MIR_FD_SOCKET_TRANSMISSION_H_ */
