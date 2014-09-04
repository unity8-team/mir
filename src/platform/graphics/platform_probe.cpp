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

#include "mir/graphics/platform.h"
#include "platform_probe.h"

std::shared_ptr<mir::SharedLibrary>
mir::graphics::module_for_device(std::vector<std::shared_ptr<SharedLibrary>> const& modules,
                                 PlatformProbeReport& report)
{
    mir::graphics::PlatformPriority best_priority_so_far = mir::graphics::unsupported;
    std::shared_ptr<mir::SharedLibrary> best_module_so_far;
    for(auto& module : modules)
    {
        try
        {
            auto probe = module->load_function<mir::graphics::PlatformProbe>("probe_platform");
            auto module_priority = probe();
            auto describe = module->load_function<DescribeModule>("describe_module");
            report.module_probed(*describe(), module_priority);
            if (module_priority > best_priority_so_far)
            {
                best_priority_so_far = module_priority;
                best_module_so_far = module;
            }
        }
        catch (std::runtime_error const& error)
        {
            report.invalid_module_probed(error);
        }
    }
    if (best_priority_so_far > mir::graphics::unsupported)
    {
        report.module_selected(*best_module_so_far->load_function<DescribeModule>("describe_module")());
        return best_module_so_far;
    }
    throw std::runtime_error{"Failed to find platform for current system"};
}
