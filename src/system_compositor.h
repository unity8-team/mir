/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Robert Ancell <robert.ancell@canonical.com>
 */

#ifndef SYSTEM_COMPOSITOR_H_
#define SYSTEM_COMPOSITOR_H_

#include "dm_connection.h"

#include <mir/default_server_configuration.h>

class SystemCompositor
{
public:
    SystemCompositor(int argc, char const* argv[], int from_dm_fd, int to_dm_fd) :
        config(argc, argv),
        dm_connection(from_dm_fd, to_dm_fd) {};

    int run();

private:
    mir::DefaultServerConfiguration config;
    DMConnection dm_connection;
};

#endif /* SYSTEM_COMPOSITOR_H_ */
