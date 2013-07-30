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
 * Authored by: Thomi Richards <thomi.richards@canonical.com>
 */

#include "results.h"
#include "threading.h"

#include <unistd.h>

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

const std::string option_str("hn:t:p");
const std::string usage("Usage:\n"
    "   -h:         This help text.\n"
    "   -n seconds  Number of seconds to run. Default is 600 (10 minutes).\n"
    "   -t threads  Number of threads to create. Default is the number of cores\n"
    "               on this machine.\n"
    );

int main(int argc, char **argv)
{
    std::chrono::seconds duration_to_run(60 * 10);
    unsigned int num_threads = std::thread::hardware_concurrency();
    int arg;
    opterr = 0;
    while ((arg = getopt(argc, argv, option_str.c_str())) != -1)
    {
        switch (arg)
        {
        case 'n':
            duration_to_run = std::chrono::seconds(std::atoi(optarg));
            break;

        case 't':
            // TODO: limit to sensible values.
            num_threads = std::atoi(optarg);
            break;
        case '?':
        case 'h':
        default:
            std::cout << usage << std::endl;
            return -1;
        }
    }

    std::vector<std::future<ThreadResults>> futures;
    for (unsigned int i = 0; i < num_threads; i++)
    {
        std::cout << "Creating thread..." << std::endl;
        futures.push_back(
            std::async(
                std::launch::async,
                run_mir_test,
                duration_to_run
                )
            );
    }
    std::vector<ThreadResults> results;
    for (auto &t: futures)
    {
        results.push_back(t.get());
    }
    bool success = true;
    for (auto &r: results)
    {
        std::cout << r.summary();
        success &= r.success();
    }
    return success ? 0 : 1;
}
