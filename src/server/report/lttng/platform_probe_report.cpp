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

#include "platform_probe_report.h"

#include "mir/report/lttng/mir_tracepoint.h"

#define TRACEPOINT_DEFINE
#define TRACEPOINT_PROBE_DYNAMIC_LINKAGE
#include "platform_probe_report_tp.h"



void mir::report::lttng::PlatformProbeReport::module_probed(mir::graphics::ModuleProperties const& module, mir::graphics::PlatformPriority probe_value)
{
    mir_tracepoint(mir_server_platform_probe, module_probed, module.name, probe_value);
}

void mir::report::lttng::PlatformProbeReport::invalid_module_probed(std::exception const& error)
{
    mir_tracepoint(mir_server_platform_probe, invalid_module_probed, error.what());
}

void mir::report::lttng::PlatformProbeReport::module_selected(mir::graphics::ModuleProperties const& module)
{
    mir_tracepoint(mir_server_platform_probe, module_selected, module.name);
}
