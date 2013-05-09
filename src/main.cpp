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

#include "system_compositor.h"
#include <mir/report_exception.h>

int main(int argc, char const* argv[])
try
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " from_dm_fd to_dm_fd" << std::endl;
        return 1;
    }

    auto from_dm_fd = atoi(argv[1]);
    auto to_dm_fd = atoi(argv[2]);

    SystemCompositor system_compositor(from_dm_fd, to_dm_fd);
    system_compositor.run(argc, argv);

    return 0;
}
catch (...)
{
    mir::report_exception(std::cerr);
    return 1;
}
