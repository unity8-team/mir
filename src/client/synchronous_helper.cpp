/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "synchronous_helper.h"

#include <poll.h>

void dispatch_connection_until(MirConnection* connection, std::function<bool()> predicate)
{
    pollfd fd;
    fd.fd = connection->watch_fd();
    fd.events = POLLIN;
    while(!predicate() && (poll(&fd, 1, -1) > 0))
    {
        connection->dispatch();
    }
}

