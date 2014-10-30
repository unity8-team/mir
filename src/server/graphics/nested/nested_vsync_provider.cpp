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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "nested_vsync_provider.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg = mir::graphics;
namespace mgn = mg::nested;

void mgn::VsyncProvider::notify_of_vsync(mg::DisplayConfigurationOutputId id, std::chrono::nanoseconds vsync_time)
{
    std::lock_guard<std::mutex> lg(last_vsync_guard);
    last_vsync[id] = vsync_time;
}

std::chrono::nanoseconds mgn::VsyncProvider::last_vsync_for(mg::DisplayConfigurationOutputId id)
{
    std::lock_guard<std::mutex> lg(last_vsync_guard);
    
    // In this structure we see two flaws of the DisplayConfigurationOutputId mechanism
    // 1. Failure to find id is not an exception, we may have just not seen a vsync time yet
    // how do we differentiate without complex integration with display notification changes?
    // 2. Similarly, how do we know when we are done with an ID?
    if (last_vsync.find(id) == last_vsync.end())
        return std::chrono::nanoseconds::zero();
    
    return last_vsync[id];
}
