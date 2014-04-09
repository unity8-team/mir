/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir_test_framework/detect_server.h"

#include <chrono>
#include <thread>
#include <fstream>

bool mir_test_framework::detect_server(
    std::string const& socket_name,
    std::chrono::milliseconds const& timeout)
{
    auto limit = std::chrono::steady_clock::now() + timeout;

    bool socket_exists = false;
    do
    {
        if (!socket_exists)
        {
            std::this_thread::yield();
        }
        socket_exists = mir_test_framework::socket_exists(socket_name);
    }
    while (!socket_exists && std::chrono::steady_clock::now() < limit);

    return socket_exists;
}

bool mir_test_framework::socket_exists(std::string const& socket_name)
{
    std::string abstract_socket_name{socket_name};
    abstract_socket_name.insert(std::begin(abstract_socket_name), '@');

    std::ifstream socket_names_file("/proc/net/unix");
    std::string line;
    while (std::getline(socket_names_file, line))
    {
       if (line.find(abstract_socket_name) != std::string::npos)
           return true;
    }
    return false;
}

