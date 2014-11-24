/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */

#include "mir/logging/dumb_console_logger.h"

#include <iostream>
#include <cstdio>
#include <chrono>

namespace ml = mir::logging;

ml::DumbConsoleLogger::DumbConsoleLogger()
    : start_time{clock.now()}
{
}

void ml::DumbConsoleLogger::log(ml::Severity severity,
                                const std::string& message,
                                const std::string& component)
{

    static const char* lut[5] =
            {
                "CC",
                "EE",
                "WW",
                "II",
                "DD"
            };

    std::ostream& out = severity < ml::Severity::informational ? std::cerr : std::cout;

    auto relative = clock.now() - start_time;
    auto usec = std::chrono::duration_cast<std::chrono::microseconds>
                    (relative).count();
    long whole = usec / 1000000L;
    long fraction = usec % 1000000L;
    char now[32];
    snprintf(now, sizeof(now), "%ld.%06ld", whole, fraction);

    out << "["
        << now
        << "] ("
        << lut[static_cast<int>(severity)]
        << ") "
        << component
        << ": "
        << message
        << "\n";
}
