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

#ifndef MIR_GRAPHICS_ANDROID_HWC_VSYNC_H_
#define MIR_GRAPHICS_ANDROID_HWC_VSYNC_H_

#include "mir/frontend/vsync_provider.h"

#include "hwc_vsync_coordinator.h"

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace mir
{
namespace graphics
{
namespace android
{

class HWCVsync : public HWCVsyncCoordinator
{
public:
    HWCVsync();

    void wait_for_vsync();
    void notify_vsync(std::chrono::nanoseconds time);
    
    std::chrono::nanoseconds last_vsync_for(graphics::DisplayConfigurationOutputId output) override;    

private:
    std::mutex vsync_wait_mutex;
    std::condition_variable vsync_trigger;

    bool vsync_occurred;
    std::chrono::nanoseconds last_vsync;
};

}
}
}
#endif /* MIR_GRAPHICS_ANDROID_HWC_VSYNC_COORDINATOR_H_ */
