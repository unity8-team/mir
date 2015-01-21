/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3,
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
 * Authored by:
 *  Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/executable_path.h"

#include <libgen.h>
#include <stdexcept>
#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>
#include <boost/filesystem.hpp>

std::string mir::executable_path()
{
    char buf[1024];
    auto tmp = readlink("/proc/self/exe", buf, sizeof buf);
    if (tmp < 0)
        BOOST_THROW_EXCEPTION(boost::enable_error_info(
                                  std::runtime_error("Failed to find our executable path"))
                              << boost::errinfo_errno(errno));
    if (tmp > static_cast<ssize_t>(sizeof(buf) - 1))
        BOOST_THROW_EXCEPTION(std::runtime_error("Path to executable is too long!"));
    buf[tmp] = '\0';
    return dirname(buf);
}

std::string mir::default_server_platform_path()
{
    try
    {
        auto base_path = executable_path();

        for (auto const& path : {base_path + "/../lib/server-modules/", base_path + "/../lib/mir/server-platform/"})
            if (boost::filesystem::exists(path))
                return path;
    }
    catch(boost::exception &)
    {
        // a failure in searching the module path inside build directories is nothing the deployed server libraries
        // should have to handle, hence ignored here
    }

    return MIR_SERVER_PLATFORM_PATH;
}

std::string mir::default_client_platform_path()
{
    try
    {
        auto base_path = executable_path();

        for (auto const& path : {base_path + "/../lib/client-modules/", base_path + "/../lib/mir/client-platform/"})
            if (boost::filesystem::exists(path))
                return path;
    }
    catch(boost::exception &)
    {
        // a failure in searching the module path inside build directories is nothing the deployed client libraries
        // should have to handle, hence ignored here
    }

    return MIR_CLIENT_PLATFORM_PATH;
}

