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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/graphics/platform.h"
#include "platform_probe.h"

std::shared_ptr<mir::SharedLibrary>
mir::graphics::module_for_device(std::vector<std::shared_ptr<SharedLibrary>> const& modules)
{
    std::cout<<"Hello!"<<std::endl;
    for(auto& module : modules)
    {
        std::cout<<"\tTrying to probe..."<<std::endl;
        auto probe = module->load_function<mir::graphics::PlatformProbe>("probe_platform");
        if (probe() == mir::graphics::best)
        {
            std::cout<<"\tWe win!"<<std::endl;
            return module;
        }
    }
    throw std::runtime_error{"Failed to find platform for current system"};
}
