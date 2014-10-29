
/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#include "mir_test_framework/platform_loader_helpers.h"

#include "mir/shared_library.h"
#include "mir_test_framework/executable_path.h"

#include <initializer_list>
#include <vector>
#include <stdexcept>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mtf = mir_test_framework;

namespace
{
std::string find_library_in_paths(std::string const& library_name,
                                  std::initializer_list<std::string> paths)
{
    for (auto const& path : paths)
    {
        struct stat sb;
        std::string candidate_filename = path + "/" + library_name;
        if (stat(candidate_filename.c_str(), &sb) == 0)
        {
            if (S_ISREG(sb.st_mode) || S_ISLNK(sb.st_mode))
            {
                return candidate_filename;
            }
        }
    }

    std::string failure_msg{"Failed to find library: "};
    failure_msg = failure_msg + library_name + " Tried paths: ";
    for (auto const& path : paths)
    {
        failure_msg += path + "\n";
    }
    throw std::runtime_error{failure_msg};
}

}

std::string mtf::client_platform_stub_path()
{
    return find_library_in_paths("client-platform-dummy.so",
                                 {mtf::library_path(), MIR_CLIENT_PLATFORM_PATH});
}

std::string mtf::client_platform_mesa_path()
{
    return find_library_in_paths("mesa.so",
                                 {mtf::library_path() + "/client-modules", MIR_CLIENT_PLATFORM_PATH});
}

std::string mtf::client_platform_android_path()
{
    return find_library_in_paths("android.so",
                                 {mtf::library_path() + "/client-modules", MIR_CLIENT_PLATFORM_PATH});
}

std::string mtf::server_platform_stub_path()
{
    return find_library_in_paths("platform-graphics-dummy.so",
                                 {mtf::library_path(), MIR_SERVER_PLATFORM_PLUGIN_PATH});
}

std::string mtf::server_platform_mesa_path()
{
    return find_library_in_paths("graphics-mesa.so",
                                 {mtf::library_path() + "/server-modules", MIR_SERVER_PLATFORM_PLUGIN_PATH});
}

std::string mtf::server_platform_android_path()
{
    return find_library_in_paths("graphics-android.so",
                                 {mtf::library_path() + "/server-modules", MIR_SERVER_PLATFORM_PLUGIN_PATH});
}
