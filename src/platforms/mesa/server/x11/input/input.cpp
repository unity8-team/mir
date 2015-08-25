/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "input_platform.h"
#include "../X11_resources.h"
#include "mir/module_properties.h"

namespace mo = mir::options;
namespace mi = mir::input;
namespace mx = mir::X;
namespace mix = mi::X;

mir::UniqueModulePtr<mi::Platform> create_input_platform(
    std::shared_ptr<mo::Option> const& /*options*/,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& /*emergency_cleanup_registry*/,
    std::shared_ptr<mi::InputDeviceRegistry> const& input_device_registry,
    std::shared_ptr<mi::InputReport> const& /*report*/,
    std::shared_ptr<void> module_context)
{
    auto context = std::static_pointer_cast<mx::X11Resources>(module_context);

    return mir::make_module_ptr<mix::XInputPlatform>(input_device_registry, context->get_conn());
}

void add_input_platform_options(
    boost::program_options::options_description& /*config*/)
{
}

mi::PlatformPriority probe_input_platform(
    mo::Option const& /*options*/)
{
    return mi::PlatformPriority::best;
}

namespace
{
mir::ModuleProperties const description = {
    "x11-input",
    MIR_VERSION_MAJOR,
    MIR_VERSION_MINOR,
    MIR_VERSION_MICRO
};
}

mir::ModuleProperties const* describe_input_module()
{
    return &description;
}
