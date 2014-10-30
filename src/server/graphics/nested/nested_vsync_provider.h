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

#ifndef MIR_GRAPHICS_NESTED_VSYNC_PROVIDER_H_
#define MIR_GRAPHICS_NESTED_VSYNC_PROVIDER_H_

#include "mir/frontend/vsync_provider.h"

#include <mutex>
#include <map>

namespace mir
{
namespace graphics
{
namespace nested
{

// TODO: Could be in detail namespace
struct VsyncProvider : public frontend::VsyncProvider
{
public:
    void notify_of_vsync(DisplayConfigurationOutputId id, std::chrono::nanoseconds vsync_time);
    std::chrono::nanoseconds last_vsync_for(DisplayConfigurationOutputId id);
    
private:
    std::mutex last_vsync_guard;
    std::map<DisplayConfigurationOutputId, std::chrono::nanoseconds> last_vsync;
};

}
}
}

#endif // MIR_GRAPHICS_NESTED_VSYNC_PROVIDER_H_
