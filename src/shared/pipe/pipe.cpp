/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/pipe.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

namespace mp = mir::pipe;

mp::Pipe::Pipe()
    : Pipe(0)
{
}

mp::Pipe::Pipe(int flags)
{
    if (::pipe2(pipefd, flags))
    {
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(std::runtime_error("Failed to create pipe"))
                << boost::errinfo_errno(errno));
    }
}

mp::Pipe::~Pipe()
{
    close(pipefd[0]);
    close(pipefd[1]);
}

int mp::Pipe::read_fd() const
{
    return pipefd[0];
}

int mp::Pipe::write_fd() const
{
    return pipefd[1];
}
