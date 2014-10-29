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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "hwc_vsync.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>

namespace mg = mir::graphics;
namespace mga = mg::android;

mga::HWCVsync::HWCVsync()
    : vsync_occurred(false),
      last_vsync(std::chrono::nanoseconds::zero())
{
}

void mga::HWCVsync::wait_for_vsync()
{
    std::unique_lock<std::mutex> lk(vsync_wait_mutex);
    vsync_occurred = false;
    while(!vsync_occurred)
    {
        vsync_trigger.wait(lk);
    }
}

void mga::HWCVsync::notify_vsync(std::chrono::nanoseconds time)
{
    std::unique_lock<std::mutex> lk(vsync_wait_mutex);

    vsync_occurred = true;
    last_vsync = time;

    vsync_trigger.notify_all();
}

std::chrono::nanoseconds mga::HWCVsync::last_vsync_for(mg::DisplayConfigurationOutputId output)
{
    if (output != mg::DisplayConfigurationOutputId{0})
    {
        BOOST_THROW_EXCEPTION(std::runtime_error(
            "HWCVsync got non zero output ID but multi-monitor is not yet supported"));
    }
    
    std::unique_lock<std::mutex> lk(vsync_wait_mutex);
    return last_vsync;
}
