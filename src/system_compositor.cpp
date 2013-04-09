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

#include <mir/run_mir.h>
#include <cstdio>
#include <thread>
#include <boost/exception/diagnostic_information.hpp>

int SystemCompositor::run()
{
    dm_connection.start();

    try
    {
        mir::run_mir(config, [](mir::DisplayServer&) {/* empty init */});

        return 0;
    }
    catch (boost::program_options::error const&)
    {
        // Can't run with these options - but no need for additional output
        return 1;
    }
    catch (std::exception const& error)
    {
        std::cerr << "ERROR: " << boost::diagnostic_information(error) << std::endl;
        return 1;
    }
}
