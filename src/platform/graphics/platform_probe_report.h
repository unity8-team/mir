/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_GRAPHICS_PLATFORM_PROBE_REPORT_H_
#define MIR_GRAPHICS_PLATFORM_PROBE_REPORT_H_

#include "mir/graphics/platform.h"

namespace mir
{
namespace graphics
{

class PlatformProbeReport
{
public:
    PlatformProbeReport() = default;
    virtual ~PlatformProbeReport() = default;

    PlatformProbeReport(PlatformProbeReport const&) = delete;
    PlatformProbeReport& operator=(PlatformProbeReport const&) = delete;

    virtual void module_probed(ModuleProperties const& module, PlatformPriority probe_value) = 0;
    virtual void invalid_module_probed(std::exception const& error) = 0;
    virtual void module_selected(ModuleProperties const& module) =  0;
};
}
}

#endif // MIR_GRAPHICS_PLATFORM_PROBE_REPORT_H_
