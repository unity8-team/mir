/*
 * Copyright © 2015 Canonical Ltd.
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
 * Author: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <iostream>
#include <cassert>
#include <string>

#include "tests/include/mir_test_framework/executable_path.h"

namespace mtf = mir_test_framework;

static void appendenv(const char* varname, const char* append)
{
    char buf[1024] = "";
    const char* value = append;
    const char* old = getenv(varname);
    if (old != NULL)
    {
        snprintf(buf, sizeof(buf)-1, "%s:%s", old, append);
        buf[sizeof(buf)-1] = '\0';
        value = buf;
    }
    setenv(varname, value, 1);
}

int main(int argc, char** argv)
{
    using namespace std::literals::string_literals;
    assert(argc > 0);

    std::string client_path = mtf::library_path() + "/client-modules";
    setenv("MIR_CLIENT_PLATFORM_PATH", client_path.c_str(), 1);
    std::cout << "MIR_CLIENT_PLATFORM_PATH=" << client_path << std::endl;

    std::string server_path = mtf::library_path() + "/server-modules";
    setenv("MIR_SERVER_PLATFORM_PATH", server_path.c_str(), 1);
    std::cout << "MIR_SERVER_PLATFORM_PATH=" << server_path << std::endl;

    appendenv("LD_LIBRARY_PATH", mtf::library_path().c_str());
    std::cout << "LD_LIBRARY_PATH=" << mtf::library_path() << std::endl;

    std::string real_binary = mtf::executable_path() + "/" + EXECUTABLE;
    std::cout << "Running: " << real_binary << std::endl;
    
    argv[0] = const_cast<char*>(real_binary.c_str());
    execv(argv[0], argv);

    perror(("Failed to execute real binary "s + real_binary).c_str());
    return 1;
}
