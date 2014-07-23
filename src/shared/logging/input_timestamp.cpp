/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir/logging/input_timestamp.h"
#include <ctime>
#include <cstdio>
#include <string>

std::string mir::logging::input_timestamp(nsecs_t when, nsecs_t next_frame_eta)
{
    // Input events use CLOCK_REALTIME, and so we must...
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    nsecs_t now = ts.tv_sec * 1000000000LL + ts.tv_nsec;
    long age_usec = (now - when) / 1000L;

    char str[64];
    if (next_frame_eta > now)
    {
        /*
         * "visible lag" is the estimated time difference between the specified
         * input event time "when" and some consequence of that getting
         * rendered. Note this is local rendering so for clients the lag does
         * not include the extra time that RPC, composition and page flipping
         * will take.
         */
        long lag_usec = (next_frame_eta - when) / 1000L;
        snprintf(str, sizeof str,
                 "%lld (%ld.%03ld ms ago, %ld.%03ld ms visible lag)",
                 static_cast<long long>(when),
                 age_usec / 1000L, age_usec % 1000L,
                 lag_usec / 1000L, lag_usec % 1000L);
    }
    else  // next_frame_eta never happened. Assume no new frame.
    {
        snprintf(str, sizeof str, "%lld (%ld.%03ld ms ago)",
                 static_cast<long long>(when),
                 age_usec / 1000L, age_usec % 1000L);
    }

    return std::string(str);
}

