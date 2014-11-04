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
 * Authored By: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_FRONTEND_VSYNC_PROVIDER_H_
#define MIR_FRONTEND_VSYNC_PROVIDER_H_

#include "mir/graphics/display_configuration.h"

#include <chrono>

namespace mir
{
namespace frontend
{
class VsyncProvider
{
public:
    virtual ~VsyncProvider() = default;
    
    virtual std::chrono::nanoseconds last_vsync_for(graphics::DisplayConfigurationOutputId output) = 0;

protected:
    VsyncProvider() = default;

    VsyncProvider& operator=(VsyncProvider const& other) = delete;
    VsyncProvider(VsyncProvider const& other) = delete;
};
}
}

#endif // MIR_FRONTEND_VSYNC_PROVIDER_H_
